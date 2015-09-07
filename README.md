# What is ecumene-cpp?
This is the C++ client/worker library for the Ecumene RPC protocol.

# Building and Installation

#Features
* **Fast:** Ecumene is based on ZeroMQ and MessagePack, without the overhead of HTTP.
* **Natural:** Ecumene is designed with the interoperability with native function calls in mind, and tries to abstract away the difference between local and remote function calls.
* **Composable:** Ecumene supports the idea of "composable services," where network services can be composed the same way as [`higher-order functions`](https://www.wikiwand.com/en/Higher-order_function).

# Getting Started
myclient.cpp:
```c++
#include <iostream>
#include <string>
#include <ecumene/function.h>
using namespace std;
using namespace ecumene;

void handleError(exception_ptr eptr);

Function<string(string)> greet("myapp.greet");

int main()
{
    // Synchronous call
    cout << greet("Zizheng") << endl;

    // Synchronous call with error handling
    try {
        cout << greet("Zizheng") << endl;
    } catch (const exception &e) {
        cout << "ERROR: " << e.what() << endl;
    }

    // Asynchronous call with future
    auto f = greet.getFuture("Zizheng");
    // Do something else...
    cout << f.get() << endl;

    // Asynchronous call with callback
    greet.withCallback("Zizheng", [](const string &s) {
        cout << s << endl;
    }, [](exception_ptr eptr) {
        handleError(eptr);
    });

    cin.get();
    return 0;
}

void handleError(exception_ptr eptr)
{
    try {
        rethrow_exception(eptr);
    } catch (const exception &e) {
        cout << "ERROR: " << e.what() << endl;
    }
}
```

myworker.cpp:
```c++
#include <iostream>
#include <string>
#include <ecumene/function_impl.h>
using namespace std;
using namespace ecumene;

FunctionImpl<string(string)> greet([](string name) {
    return name + ", welcome to Ecumene!";
}, "myapp.greet", "tcp://*:5555", "tcp://127.0.0.1:5555");

int main()
{
    cin.get();
    return 0;
}
```

Compile with `g++ -std=c++14 -Wall -Wextra -pedantic -O3 -pthread -o myclient myclient.cpp -lczmq -lecumene` and `g++ -std=c++14 -Wall -Wextra -pedantic -O3 -pthread -o myworker myworker.cpp -lczmq -lecumene`.

Then run `./myworker` followed by `./myclient`.

# License
ecumene-cpp is licensed under the GNU Lesser General Public License v3.0. See the [`LICENSE`](./LICENSE) file for details.
