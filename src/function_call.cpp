#include <czmq.h>
#include <msgpack.hpp>

#include "ecumene/function_call.h"

#define UNUSED(x) (void)(x)

namespace ecumene {

FunctionCall::FunctionCall(
        const std::string &ecmKey,
        const msgpack::sbuffer &sbuf,
        const std::function<void(const FunctionCallResult &&)> &callback,
        const std::chrono::seconds &timeout)
    : ecmKey(ecmKey)
    , args(zmsg_new())
    , callback(callback)
    , timeoutAt(std::chrono::steady_clock::now() + timeout)
{
    assert(args);

    int rc = zmsg_addmem(args, sbuf.data(), sbuf.size());
    UNUSED(rc);
    assert(rc == 0);
}

FunctionCall::FunctionCall(FunctionCall &&other)
    : ecmKey(std::move(other.ecmKey))
    , args(other.args)
    , callback(std::move(other.callback))
    , timeoutAt(other.timeoutAt)
{
    other.args = nullptr;
}

FunctionCall::~FunctionCall()
{
    zmsg_destroy(&args);
}

}
