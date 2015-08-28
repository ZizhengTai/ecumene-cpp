#include <czmq.h>

#include "ecumene/memory.h"

namespace ecumene {

namespace detail {

std::unique_ptr<zframe_t, FrameDel> makeFrame(zframe_t *f)
{
    return std::move(std::unique_ptr<zframe_t, FrameDel>(
                f, [](zframe_t *f) { zframe_destroy(&f); }));
}

std::unique_ptr<zmsg_t, MsgDel> makeMsg(zmsg_t *m)
{
    return std::move(std::unique_ptr<zmsg_t, MsgDel>(
                m, [](zmsg_t *m) { zmsg_destroy(&m); }));
}

std::unique_ptr<zsock_t, SockDel> makeSock(zsock_t *s)
{
    return std::move(std::unique_ptr<zsock_t, SockDel>(
                s, [](zsock_t *s) { zsock_destroy(&s); }));
}

std::unique_ptr<zpoller_t, PollerDel> makePoller(zpoller_t *p)
{
    return std::move(std::unique_ptr<zpoller_t, PollerDel>(
                p, [](zpoller_t *p) { zpoller_destroy(&p); }));
}

}

}
