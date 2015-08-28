#ifndef ECUMENE_FUNCTION_CALL_H
#define ECUMENE_FUNCTION_CALL_H

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include <msgpack.hpp>

#include "ecumene/function_call_result.h"

typedef struct _zmsg_t zmsg_t;

namespace ecumene {

struct FunctionCall {
    const std::string ecmKey;
    zmsg_t *args;
    const std::function<void(const FunctionCallResult &&)> callback;
    std::chrono::steady_clock::time_point timeoutAt;

    explicit FunctionCall(
            const std::string &ecmKey,
            const msgpack::sbuffer &sbuf,
            const std::function<void(const FunctionCallResult &&)> &callback,
            const std::chrono::seconds &timeout);
    FunctionCall(FunctionCall &&other);
    ~FunctionCall();

    FunctionCall() = delete;
    FunctionCall(const FunctionCall &) = delete;
    void operator =(const FunctionCall &) = delete;
};

}

#endif /* ECUMENE_FUNCTION_CALL_H */
