# What is ecumene-cpp?
This is the C++ client/worker library for the Ecumene RPC protocol.

# Building and Installation

# Getting Started
my_client.cpp:
```c++
#include <iostream>
#include <ecumene/function.h>
using namespace std;
using namespace ecumene;

Function<string(string)> greet("myapp.greet");

int main()
{

```

# License
ecumene-cpp is licensed under the GNU Lesser General Public License v3.0. See the [`LICENSE`](./LICENSE) file for details.
