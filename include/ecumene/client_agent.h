#ifndef ECUMENE_CLIENT_AGENT_H
#define ECUMENE_CLIENT_AGENT_H

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "ecumene/function_call.h"

typedef struct _zsock_t zsock_t;

namespace ecumene {

class ClientAgent {
public:
    static ClientAgent &sharedInstance();

    ClientAgent(const ClientAgent &) = delete;
    ClientAgent(ClientAgent &&) = delete;
    void operator =(const ClientAgent &) = delete;

    void send(FunctionCall &&call);

private:
    ClientAgent();
    ~ClientAgent();

    static void actorTask(zsock_t *pipe, void *args);

    enum ActorStatus {
        NotReady,
        Ready,
        SendRequested,
        ShouldDestroy,
        Destroyed
    } actorStatus;
    std::mutex actorLock;
    std::condition_variable actorCV;
    std::string sendId;

    std::mutex callsMutex;
    std::map<unsigned long long, FunctionCall> calls;
};

}

#endif /* ECUMENE_CLIENT_AGENT_HPP */
