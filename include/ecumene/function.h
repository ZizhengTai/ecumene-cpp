#ifndef ECUMENE_FUNCTION_H
#define ECUMENE_FUNCTION_H

#include <exception>
#include <future>
#include <stdexcept>
#include <string>

#include <msgpack.hpp>

#include "ecumene/base_function.h"
#include "ecumene/client_agent.h"

namespace ecumene {

template<class T>
class Function;

template<class R, class ... Args>
class Function<R(Args...)>: protected BaseFunction {
public:
    using BaseFunction::BaseFunction;
    using BaseFunction::setTimeout;

    std::future<R> getFuture(Args ... args)
    {
        auto p = std::make_shared<std::promise<R>>();

        withCallback(
                args...,
                [p](const R &result) {
                    p->set_value(result);
                },
                [p](const std::exception_ptr &eptr) {
                    p->set_exception(eptr);
                });

        return p->get_future();
    }

    template<class T, class U>
    void withCallback(Args ... args, const T &&success, const U &&error)
    {
        // Pack arguments into buffer
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, std::forward_as_tuple(args...));

        FunctionCallResultCallback callback([
                this,
                success = std::move(success),
                error = std::move(error)](const FunctionCallResult &&result) {

            // Handle error
            std::exception_ptr eptr = handleError(result);
            if (eptr) {
                error(eptr);
                return;
            }

            // Try to unpack
            try {
                msgpack::unpacked msg;
                msgpack::unpack(&msg, result.data(), result.size());
                success(msg.get().as<R>());
            } catch (...) {
                error(std::current_exception());
            }
        });

        // Pass to network agent
        ClientAgent::sharedInstance().send(FunctionCall(
                    _ecmKey,
                    std::move(sbuf),
                    std::move(callback),
                    _timeout));
    }

    R operator ()(Args ... args)
    {
        return getFuture(args...).get();
    }
};

}

#endif /* ECUMENE_FUNCTION_H */
