#include <chrono>
#include <mutex>
#include <thread>

#include <czmq.h>

#include "ecumene/client_agent.h"
#include "ecumene/memory.h"

#define UNUSED(x) (void)(x)

namespace ecumene {

static const uint16_t PROTOCOL_VERSION = 0;

ClientAgent &ClientAgent::sharedInstance()
{
    static ClientAgent sharedInstance;
    return sharedInstance;
}

void ClientAgent::send(FunctionCall &&call)
{
    static unsigned long long sequence = 0;
    {
        std::lock_guard<std::mutex> lock(callsMutex);
        calls.insert(std::make_pair(sequence, std::move(call)));
    }
    {
        std::lock_guard<std::mutex> lk(actorLock);
        sendId = std::to_string(sequence++);
        actorStatus = ActorStatus::SendRequested;
    }
    actorCV.notify_one();
}

ClientAgent::ClientAgent()
    : actorStatus(ActorStatus::NotReady)
{
    std::thread([this]() {
        zactor_t *actor = zactor_new(actorTask, this);
        assert(actor);

        {
            std::lock_guard<std::mutex> lk(actorLock);
            actorStatus = ActorStatus::Ready;
        }
        actorCV.notify_one();

        bool shouldDestroy = false;
        while (!shouldDestroy) {
            std::unique_lock<std::mutex> lk(actorLock);
            actorCV.wait(lk, [this] {
                return actorStatus == ActorStatus::ShouldDestroy ||
                    actorStatus == ActorStatus::SendRequested;
            });

            if (actorStatus == ActorStatus::ShouldDestroy) {
                shouldDestroy = true;
            } else {
                zstr_sendx(actor, "$SEND", sendId.c_str(), nullptr);
                actorStatus = ActorStatus::Ready;
            }
        }

        zactor_destroy(&actor);

        std::lock_guard<std::mutex> lk(actorLock);
        actorStatus = ActorStatus::Destroyed;
        actorCV.notify_one();
    }).detach();

    std::unique_lock<std::mutex> lk(actorLock);
    actorCV.wait(lk, [this] { return actorStatus == ActorStatus::Ready; });
}

ClientAgent::~ClientAgent()
{
    {
        std::unique_lock<std::mutex> lk(actorLock);
        actorStatus = ActorStatus::ShouldDestroy;
        actorCV.notify_one();
        actorCV.wait(lk, [this] { return actorStatus == ActorStatus::Destroyed; });
    }

    zsys_info("Cleaned up client agent.");
}

void ClientAgent::actorTask(zsock_t *pipe, void *args)
{
    assert(pipe);
    assert(args);

    ClientAgent &agent = *static_cast<ClientAgent *>(args);

    const auto ecm = detail::makeSock(zsock_new_dealer("tcp://ecumene.io:23332"));
    assert(ecm.get());

    const auto poller = detail::makePoller(zpoller_new(pipe, ecm.get(), nullptr));
    assert(poller.get());

    std::map<std::string, decltype(detail::makeSock(nullptr))> socks;

    int rc = zsock_signal(pipe, 0);
    UNUSED(rc);
    assert(rc == 0);

    bool terminated = false;
    while (!terminated && !zsys_interrupted) {
        zsock_t *sock = static_cast<zsock_t *>(zpoller_wait(poller.get(), 1000));

        if (sock == pipe) {
            // Internal command

            auto msg = detail::makeMsg(zmsg_recv(sock));
            assert(msg.get());
            assert(zmsg_size(msg.get()) >= 1);

            auto command = detail::makeFrame(zmsg_pop(msg.get()));

            if (zframe_streq(command.get(), "$TERM")) {
                terminated = true;
            } else if (zframe_streq(command.get(), "$SEND")) {
                std::unique_ptr<char> id(zmsg_popstr(msg.get()));

                std::lock_guard<std::mutex> lock(agent.callsMutex);

                char *end;
                const auto iterCall =
                    agent.calls.find(std::strtoull(id.get(), &end, 10));
                if (iterCall != agent.calls.cend()) {
                    auto &call = iterCall->second;
                    assert(call.args);

                    const auto iterSock = socks.find(call.ecmKey);
                    if (iterSock != socks.cend()) {
                        // Use existing worker socket
                        const auto &worker = iterSock->second;
                        assert(worker.get());

                        rc = zstr_sendm(worker.get(), id.get());
                        assert(rc == 0);

                        rc = zmsg_send(&call.args, worker.get());
                        assert(rc == 0);
                    } else {
                        // Ask Ecumene for new worker

                        zmsg_t *workerRequest = zmsg_new();
                        zframe_t *version =
                            zframe_new(&PROTOCOL_VERSION, sizeof (uint16_t));
                        zmsg_append(workerRequest, &version);
                        zmsg_addstr(workerRequest, id.get());
                        zmsg_addstr(workerRequest, call.ecmKey.c_str());

                        rc = zmsg_send(&workerRequest, ecm.get());
                        assert(rc == 0);
                    }
                }
            }
        } else if (sock == ecm.get()) {
            // Worker assignment from Ecumene
            zsys_debug("Assigned!");

            char *idData, *ecmKeyData, *statusData, *endpointData;
            rc = zstr_recvx(
                    sock,
                    &idData,
                    &ecmKeyData,
                    &statusData,
                    &endpointData,
                    nullptr);
            assert(rc == 4);

            // RAII protection
            std::unique_ptr<char> id(idData),
                                  ecmKey(ecmKeyData),
                                  status(statusData),
                                  endpoint(endpointData);

            if (streq(status.get(), "")) {
                // Success

                if (socks.find(ecmKey.get()) == socks.cend()) {
                    zsys_debug("Connecting to worker...");

                    auto worker = detail::makeSock(zsock_new_dealer(endpoint.get()));
                    assert(worker.get());

                    rc = zpoller_add(poller.get(), worker.get());
                    assert(rc == 0);

                    socks.insert(std::make_pair(ecmKey.get(), std::move(worker)));
                }

                {
                    std::lock_guard<std::mutex> lk(agent.actorLock);
                    agent.sendId = id.get();
                    agent.actorStatus = ActorStatus::SendRequested;
                }
                agent.actorCV.notify_one();
            } else if (streq(status.get(), "U")) {
                // Undefined reference

                std::unique_lock<std::mutex> lock(agent.callsMutex);

                char *end;
                const auto it = agent.calls.find(std::strtoull(id.get(), &end, 10));
                if (it != agent.calls.cend()) {
                    auto call = std::move(it->second);
                    agent.calls.erase(it);
                    lock.unlock();

                    zframe_t *statusFrame = zframe_new("U", 1);
                    zframe_t *resultFrame = zframe_new_empty();
                    call.callback(FunctionCallResult(&statusFrame, &resultFrame));
                }
            }
        } else if (sock) {
            // Response from some worker
            
            zmsg_t *msg = zmsg_recv(sock);
            assert(msg);
            assert(zmsg_size(msg) == 3);

            // RAII protection
            std::unique_ptr<char> id(zmsg_popstr(msg));
            zframe_t *statusFrame = zmsg_pop(msg);
            zframe_t *resultFrame = zmsg_pop(msg);
            FunctionCallResult result(&statusFrame, &resultFrame);
            zmsg_destroy(&msg);

            std::unique_lock<std::mutex> lock(agent.callsMutex);

            char *end;
            const auto it = agent.calls.find(std::strtoull(id.get(), &end, 10));
            if (it != agent.calls.cend()) {
                auto call = std::move(it->second);
                agent.calls.erase(it);
                lock.unlock();

                call.callback(std::move(result));
            }
        }

        // Check timeout
        std::lock_guard<std::mutex> lock(agent.callsMutex);

        auto now = std::chrono::steady_clock::now();

        auto it = agent.calls.begin();
        while (it != agent.calls.end()) {
            if (now >= it->second.timeoutAt) {
                auto call = std::move(it->second);
                it = agent.calls.erase(it);

                zframe_t *statusFrame = zframe_new("N", 1);
                zframe_t *resultFrame = zframe_new_empty();
                call.callback(FunctionCallResult(&statusFrame, &resultFrame));
            } else {
                ++it;
            }
        }
    }
}

}
