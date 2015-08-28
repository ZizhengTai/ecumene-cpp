#ifndef ECUMENE_FUNCTION_CALL_RESULT_H
#define ECUMENE_FUNCTION_CALL_RESULT_H

#include <cstddef>
#include <functional>

typedef struct _zframe_t zframe_t;

namespace ecumene {

class FunctionCallResult {
public:
    enum Status {
        Success,
        InvalidArgument,
        UndefinedReference,
        NetworkError,
        UnknownError
    };

    explicit FunctionCallResult(zframe_t **statusPtr, zframe_t **resultPtr);
    FunctionCallResult(FunctionCallResult &&other);
    ~FunctionCallResult();

    FunctionCallResult() = delete;
    FunctionCallResult(const FunctionCallResult &) = delete;
    void operator =(const FunctionCallResult &) = delete;

    Status status() const;
    const char *data() const;
    std::size_t size() const;

private:
    Status _status;
    zframe_t *_result;
};

using FunctionCallResultCallback =
    std::function<void(const FunctionCallResult &&)>;

}

#endif /* ECUMENE_FUNCTION_CALL_RESULT_H */
