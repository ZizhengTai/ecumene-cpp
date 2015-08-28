#ifndef ECUMENE_BASE_FUNCTION_H
#define ECUMENE_BASE_FUNCTION_H

#include <chrono>
#include <string>

#include "ecumene/function_call_result.h"

namespace ecumene {

class BaseFunction {
public:
    explicit BaseFunction(const std::string &ecmKey);
    BaseFunction(const BaseFunction &other);
    void operator =(const BaseFunction &rhs);

    void setTimeout(const std::chrono::seconds &timeout);

protected:
    std::string _ecmKey;
    std::chrono::seconds _timeout;

    std::exception_ptr handleError(const FunctionCallResult &result) const;
};

}

#endif /* ECUMENE_BASE_FUNCTION_H */
