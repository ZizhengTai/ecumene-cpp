#include <memory>

#include <czmq.h>

#include "ecumene/memory.h"
#include "ecumene/worker_agent.h"

#define UNUSED(x) (void)(x)

namespace ecumene {

WorkerAgent::WorkerAgent(
        const std::string &ecmKey,
        const std::string &localEndpoint,
        const std::string &publicEndpoint,
        const std::function<void(const msgpack::unpacked &, msgpack::sbuffer &)> callback)
    : _ecmKey(ecmKey)
    , _localEndpoint(localEndpoint)
    , _publicEndpoint(publicEndpoint)
    , _callback(callback)
    , _actor(zactor_new(actorTask, this))
{
    assert(_actor);
}

WorkerAgent::~WorkerAgent()
{
    zactor_destroy(&_actor);
}

void WorkerAgent::actorTask(zsock_t *pipe, void *args)
{
    assert(pipe);
    assert(args);

    const WorkerAgent &agent = *static_cast<WorkerAgent *>(args);

    const auto worker = detail::makeSock(zsock_new_router(agent._localEndpoint.c_str()));
    assert(worker.get());

    const auto poller = detail::makePoller(zpoller_new(pipe, worker.get(), nullptr));
    assert(poller.get());

    int rc = zsock_signal(pipe, 0);
    UNUSED(rc);
    assert(rc == 0);

    bool terminated = false;
    while (!terminated && !zsys_interrupted) {
        zsock_t *sock = static_cast<zsock_t *>(zpoller_wait(poller.get(), -1));

        if (sock == pipe) {
            std::unique_ptr<char> command(zstr_recv(sock));
            if (streq(command.get(), "$TERM")) {
                terminated = true;
            }
        } else if (sock == worker.get()) {
            auto request = detail::makeMsg(zmsg_recv(sock));
            assert(zmsg_size(request.get()) == 3);

            auto identity = detail::makeFrame(zmsg_pop(request.get()));
            auto id = detail::makeFrame(zmsg_pop(request.get()));

            try {
                auto argsFrame = detail::makeFrame(zmsg_pop(request.get()));

                msgpack::unpacked msg;
                msgpack::unpack(
                        &msg,
                        reinterpret_cast<const char *>(zframe_data(argsFrame.get())),
                        zframe_size(argsFrame.get()));

                msgpack::sbuffer sbuf;
                agent._callback(msg, sbuf);

                auto resultFrame =
                    detail::makeFrame(zframe_new(sbuf.data(), sbuf.size()));

                // Identity for ROUTER
                zframe_t *f = identity.get();
                rc = zframe_send(&f, sock, ZFRAME_MORE);
                assert(rc == 0);
                identity.release();

                // Caller local ID
                f = id.get();
                rc = zframe_send(&f, sock, ZFRAME_MORE);
                assert(rc == 0);
                id.release();

                // Status
                f = zframe_new_empty();
                rc = zframe_send(&f, sock, ZFRAME_MORE);
                assert(rc == 0);

                // Result
                f = resultFrame.get();
                rc = zframe_send(&f, sock, 0);
                assert(rc == 0);
                resultFrame.release();
            } catch (const msgpack::type_error &e) {
                // Identity for ROUTER
                zframe_t *f = identity.get();
                rc = zframe_send(&f, sock, ZFRAME_MORE);
                assert(rc == 0);
                identity.release();

                // Caller local ID
                f = id.get();
                rc = zframe_send(&f, sock, ZFRAME_MORE);
                assert(rc == 0);
                id.release();

                // Status
                f = zframe_new("I", 1);
                rc = zframe_send(&f, sock, ZFRAME_MORE);
                assert(rc == 0);

                // Result
                f = zframe_new_empty();
                rc = zframe_send(&f, sock, 0);
                assert(rc == 0);
            }
        }
    }

    zsys_debug("Cleaned up worker agent.");
}

}
