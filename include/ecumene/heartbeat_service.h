#ifndef ECUMENE_HEARTBEAT_SERVICE_H
#define ECUMENE_HEARTBEAT_SERVICE_H

#include <map>
#include <mutex>
#include <string>

typedef struct _zsock_t zsock_t;
typedef struct _zactor_t zactor_t;

namespace ecumene {

class HeartbeatService {
public:
    static HeartbeatService &sharedInstance();

    HeartbeatService(const HeartbeatService &) = delete;
    HeartbeatService(HeartbeatService &&) = delete;
    void operator =(const HeartbeatService &) = delete;

    void registerWorker(
            const std::string &ecmKey,
            const std::string &endpoint);
    void unregisterWorker(
            const std::string &ecmKey,
            const std::string &endpoint);

private:
    HeartbeatService();
    ~HeartbeatService();

    static void actorTask(zsock_t *pipe, void *args);

    std::mutex workersMutex;

    // Alive
    std::multimap<std::string, std::string> workers;

    // To be unregistered
    std::multimap<std::string, std::string> unreg;

    zsock_t *ecm;
    zactor_t *actor;
};

}

#endif /* ECUMENE_HEARTBEAT_SERVICE_H */
