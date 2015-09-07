#ifndef ECUMENE_FUNCTION_IMPL_H
#define ECUMENE_FUNCTION_IMPL_H

#include <string>

#include <msgpack.hpp>

#include "ecumene/exception.h"
#include "ecumene/heartbeat_service.h"
#include "ecumene/memory.h"
#include "ecumene/worker_agent.h"

#define UNUSED(x) (void)(x)

namespace ecumene {

namespace detail {

template<class Fnc, class ... Types, std::size_t ... Indices>
auto applyTupleImpl(
        Fnc &&fnc,
        const std::tuple<Types...> &tuple,
        std::index_sequence<Indices...>)
{
	return std::forward<Fnc>(fnc)(std::get<Indices>(tuple)...);	
}

template<class Fnc, class ... Types>
auto applyTuple(Fnc &&fnc, const std::tuple<Types...> &tuple)
{
	return applyTupleImpl(
            std::forward<Fnc>(fnc),
            tuple,
            std::index_sequence_for<Types...>());
}

}

template<class T>
class FunctionImpl;

template<class R, class ... Args>
class FunctionImpl<R(Args...)> {
public:
    explicit FunctionImpl(
            const std::function<R(Args...)> &func,
            const std::string &ecmKey,
            const std::string &localEndpoint,
            const std::string &publicEndpoint)
        : _func(func)
        , _ecmKey(ecmKey)
        , _publicEndpoint(publicEndpoint)
        , _agent(
                ecmKey,
                localEndpoint,
                publicEndpoint,
                [this](const msgpack::unpacked &unpacked, msgpack::sbuffer &sbuf) {
                    auto result = detail::applyTuple(
                            _func,
                            unpacked.get().as<decltype(_argsTuple)>());
                    msgpack::pack(sbuf, result);
                })
    {
        HeartbeatService::sharedInstance().registerWorker(
                _ecmKey, _publicEndpoint);
    }

    FunctionImpl(const FunctionImpl &) = delete;
    FunctionImpl(FunctionImpl &&) = delete;
    void operator =(const FunctionImpl &) = delete;

    ~FunctionImpl()
    {
        unregister();
    }

    R operator ()(Args ... args)
    {
        return _func(args...);
    }

    void unregister()
    {
        HeartbeatService::sharedInstance().unregisterWorker(
                _ecmKey, _publicEndpoint);
    }

private:
    const std::function<R(Args...)> _func;
    std::tuple<Args...> _argsTuple;
    const std::string _ecmKey;
    const std::string _publicEndpoint;
    const WorkerAgent _agent;
};

}

#endif /* ECUMENE_FUNCTION_IMPL_H */
