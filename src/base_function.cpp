#include "ecumene/base_function.h"

namespace ecumene {

BaseFunction::BaseFunction(const std::string &ecmKey)
    : _ecmKey(ecmKey)
    , _timeout(std::chrono::seconds(15))
{
}

BaseFunction::BaseFunction(const BaseFunction &other)
    : BaseFunction(other._ecmKey)
{
}

void BaseFunction::operator =(const BaseFunction &rhs)
{
    _ecmKey = rhs._ecmKey;
}

void BaseFunction::setTimeout(const std::chrono::seconds &timeout)
{
    _timeout = timeout;
}

std::exception_ptr BaseFunction::handleError(const FunctionCallResult &result) const
{
    switch (result.status()) {
    case FunctionCallResult::Status::Success:
        break;
    case FunctionCallResult::Status::InvalidArgument:
        return std::make_exception_ptr(
                std::invalid_argument("invalid argument to " + _ecmKey));
    case FunctionCallResult::Status::UndefinedReference:
        return std::make_exception_ptr(
                std::runtime_error("undefined reference to " + _ecmKey));
    case FunctionCallResult::Status::NetworkError:
        return std::make_exception_ptr(
                std::runtime_error("failed to call " + _ecmKey + " due to network error"));
    default:
        return std::make_exception_ptr(
                std::runtime_error("unknown error when calling " + _ecmKey));
    }
    return nullptr;
}

}
