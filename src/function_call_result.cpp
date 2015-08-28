#include <czmq.h>

#include "ecumene/function_call_result.h"

namespace ecumene {

FunctionCallResult::FunctionCallResult(zframe_t **statusPtr, zframe_t **resultPtr)
    : _result(*resultPtr)
{
    *resultPtr = nullptr;

    zframe_t *status = *statusPtr;
    if (zframe_streq(status, "")) {
        _status = Status::Success;
    } else if (zframe_streq(status, "I")) {
        _status = Status::InvalidArgument;
    } else if (zframe_streq(status, "U")) {
        _status = Status::UndefinedReference;
    } else if (zframe_streq(status, "N")) {
        _status = Status::NetworkError;
    } else {
        _status = Status::UnknownError;
    }
    zframe_destroy(&status);
    *statusPtr = nullptr;
}

FunctionCallResult::FunctionCallResult(FunctionCallResult &&other)
    : _status(other._status)
    , _result(other._result)
{
    other._result = nullptr;
}

FunctionCallResult::~FunctionCallResult()
{
    zframe_destroy(&_result);
}

FunctionCallResult::Status FunctionCallResult::status() const
{
    return _status;
}

const char * FunctionCallResult::data() const
{
    return reinterpret_cast<const char *>(zframe_data(_result));
}

std::size_t FunctionCallResult::size() const
{
    return zframe_size(_result);
}

}
