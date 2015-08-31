#include <chrono>
#include <memory>

#include <czmq.h>

#include "ecumene/heartbeat_service.h"
#include "ecumene/memory.h"

#define UNUSED(x) (void)(x)

namespace ecumene {

static const uint16_t HEARTBEAT_PROTOCOL_VERSION = 0;
static const std::chrono::milliseconds HEARTBEAT_INTERVAL(5000);
static const std::chrono::milliseconds RECONNECT_INTERVAL(30000);
static const char *ECM_ENDPOINT = "tcp://ecumene.io:23331";
static const char *CONNECT_ECM_ENDPOINT = ">tcp://ecumene.io:23331";

HeartbeatService &HeartbeatService::sharedInstance()
{
    static HeartbeatService sharedInstance;
    return sharedInstance;
}

HeartbeatService::HeartbeatService()
    : ecm(zsock_new_push(CONNECT_ECM_ENDPOINT))
    , actor(zactor_new(actorTask, this))
{
    assert(ecm);
    assert(actor);
}

HeartbeatService::~HeartbeatService()
{
    zactor_destroy(&actor);
    zsock_destroy(&ecm);

    zsys_info("Cleaned up heartbeat service.");
}

void HeartbeatService::registerWorker(
        const std::string &ecmKey,
        const std::string &endpoint)
{
    std::lock_guard<std::mutex> lock(workersMutex);
    workers.insert(std::make_pair(ecmKey, endpoint));
}

void HeartbeatService::unregisterWorker(
        const std::string &ecmKey,
        const std::string &endpoint)
{
    std::lock_guard<std::mutex> lock(workersMutex);

    auto range = workers.equal_range(ecmKey);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == endpoint) {
            workers.erase(it);
            break;
        }
    }

    unreg.insert(std::make_pair(ecmKey, endpoint));
}

inline int sendVersion(zsock_t *sock, bool more = true)
{
    zframe_t *version =
        zframe_new(&HEARTBEAT_PROTOCOL_VERSION, sizeof (uint16_t));
    assert(version);

    return zframe_send(&version, sock, more ? ZFRAME_MORE : 0);
}

void HeartbeatService::actorTask(zsock_t *pipe, void *args)
{
    assert(pipe);
    assert(args);

    HeartbeatService &service = *static_cast<HeartbeatService *>(args);

    const auto poller = detail::makePoller(zpoller_new(pipe, nullptr));
    assert(poller.get());

    int rc = zsock_signal(pipe, 0);
    UNUSED(rc);
    assert(rc == 0);

    auto beatAt = std::chrono::steady_clock::now();
    auto reconnectAt = std::chrono::steady_clock::now() + RECONNECT_INTERVAL;

    bool terminated = false;
    while (!terminated && !zsys_interrupted) {
        if (zpoller_wait(poller.get(), 100) == pipe) {
            // Internal command

            auto command = detail::makeFrame(zframe_recv(pipe));
            if (zframe_streq(command.get(), "$TERM")) {
                terminated = true;
            }
        }

        // Unregister
        std::lock_guard<std::mutex> lock(service.workersMutex);

        for (const auto &worker: service.unreg) {
            rc = sendVersion(service.ecm);
            assert(rc == 0);

            rc = zstr_sendx(
                    service.ecm,
                    "U",
                    worker.first.c_str(),
                    worker.second.c_str(),
                    nullptr);
            assert(rc == 0);
        }
        service.unreg.clear();

        auto now = std::chrono::steady_clock::now();
        if (now >= beatAt) {
            // Heartbeat

            if (now >= reconnectAt) {
                zsys_debug("Reconnect!");

                rc = zsock_disconnect(service.ecm, "%s", ECM_ENDPOINT);
                assert(rc == 0);

                rc = zsock_connect(service.ecm, "%s", ECM_ENDPOINT);
                assert(rc == 0);

                reconnectAt = std::chrono::steady_clock::now() + RECONNECT_INTERVAL;
            }

            zsys_debug("Beat!");

            for (const auto &worker: service.workers) {
                rc = sendVersion(service.ecm);
                assert(rc == 0);

                rc = zstr_sendx(
                        service.ecm,
                        "",
                        worker.first.c_str(),
                        worker.second.c_str(),
                        nullptr);
                assert(rc == 0);
            }

            beatAt = std::chrono::steady_clock::now() + HEARTBEAT_INTERVAL;
        }
    }
}

}
