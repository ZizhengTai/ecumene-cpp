#ifndef ECUMENE_WORKER_AGENT_H
#define ECUMENE_WORKER_AGENT_H

#include <functional>
#include <string>

#include <msgpack.hpp>

typedef struct _zsock_t zsock_t;
typedef struct _zactor_t zactor_t;

namespace ecumene {

class WorkerAgent {
public:
    explicit WorkerAgent(
            const std::string &ecmKey,
            const std::string &localEndpoint,
            const std::string &publicEndpoint,
            const std::function<void(const msgpack::unpacked &, msgpack::sbuffer &)> callback);
    ~WorkerAgent();

    WorkerAgent(const WorkerAgent &) = delete;
    WorkerAgent(WorkerAgent &&) = delete;
    WorkerAgent & operator =(const WorkerAgent &) = delete;

private:
    const std::string _ecmKey;
    const std::string _localEndpoint;
    const std::string _publicEndpoint;
    const std::function<void(const msgpack::unpacked &, msgpack::sbuffer &)> _callback;
    zactor_t *_actor;

    static void actorTask(zsock_t *pipe, void *args);
};

}

#endif /* ECUMENE_WORKER_AGENT_H */
