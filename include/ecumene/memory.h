#ifndef ECUMENE_MEMORY_H
#define ECUMENE_MEMORY_H

#include <functional>
#include <memory>

typedef struct _zframe_t zframe_t;
typedef struct _zmsg_t zmsg_t;
typedef struct _zsock_t zsock_t;
typedef struct _zpoller_t zpoller_t;

namespace ecumene {

namespace detail {

using FrameDel = std::function<void(zframe_t *)>;
using MsgDel = std::function<void(zmsg_t *)>;
using SockDel = std::function<void(zsock_t *)>;
using PollerDel = std::function<void(zpoller_t *)>;

std::unique_ptr<zframe_t, FrameDel> makeFrame(zframe_t *f);
std::unique_ptr<zmsg_t, MsgDel> makeMsg(zmsg_t *m);
std::unique_ptr<zsock_t, SockDel> makeSock(zsock_t *s);
std::unique_ptr<zpoller_t, PollerDel> makePoller(zpoller_t *p);

}

}

#endif /* ECUMENE_MEMORY_H */
