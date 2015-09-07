#include <memory>

#include <czmq.h>

#include "ecumene/exception.h"
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

            static const auto respond = [](
                    auto &&identity,
                    auto &&id,
                    const char *status,
                    auto &&data,
                    zsock_t *sock) {
                zmsg_t *response = zmsg_new();

                // Identity for ROUTER
                zframe_t *f = identity.release();
                zmsg_append(response, &f);

                // Caller local ID
                f = id.release();
                zmsg_append(response, &f);

                // Status
                f = zframe_new(status, std::strlen(status));
                zmsg_append(response, &f);

                // Result
                f = data.release();
                zmsg_append(response, &f);

                zmsg_send(&response, sock);
            };

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

                respond(identity, id, "", resultFrame, sock);
            } catch (const msgpack::type_error &e) {
                respond(identity, id, "I", detail::makeFrame(zframe_new_empty()), sock);
            } catch (const InvalidArgument &e) {
                respond(identity, id, "I", detail::makeFrame(zframe_new_empty()), sock);
            } catch (const UndefinedReference &e) {
                respond(identity, id, "U", detail::makeFrame(zframe_new_empty()), sock);
            } catch (const NetworkError &e) {
                respond(identity, id, "N", detail::makeFrame(zframe_new_empty()), sock);
            } catch (...) {
                respond(identity, id, "?", detail::makeFrame(zframe_new_empty()), sock);
            }
        }
    }

    zsys_debug("Cleaned up worker agent.");
}

}
