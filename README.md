# IVI C++ SDK

## General info
The *IVI C++ SDK* is a turnkey wrapper and management layer around the underlying [IVI gRPC API](https://github.com/MythicalGames/ivi-sdk-proto/).

## Building

* Uses CMake
* Tested against Win32 MSVC 16 and Ubuntu 20.04 with both gcc and clang.
* Should be compatible with any `C++11` compliant toolchain that can build gRPC, and should also work with `C++14` and `C++17` language standards.

Given cmake and an appropriate C++ toolchain installed, from the ivi-sdk-native directory, simply run:
`cmake --build ./`

See `ivi-sdk-example.cpp` for a basic demonstration on how to interact with the SDK.

Run the `ivi-sdk-example` executable to see it connect and perform some trivial and naive calls to the IVI API, including bad RPC calls to demonstrate error handling.  You will have to supply a valid environment ID, API key, and optionally a specific IVI host as command line arguments or environment variables.

Additionally, on Windows you will need to manually supply a certificate PEM file for gRPC's SSL connection. The one packaged with gRPC is sufficient and can be specified by setting this environment variable appropriately:
```GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=[ROOT]\ivi-sdk-native\_deps\grpc-ivi-src\etc\roots.pem```

The IVI SDK's `CMakeLists.txt` configurations are purposely minimalist and rely on `FetchContent` for the gRPC dependency to simplify integration into other dependency trees, and are based on [gRPC C++'s recommendations](https://github.com/grpc/grpc/tree/master/src/cpp).  The IVI cmake configuration purposely eschews system-installed gRPC detection in preferance of a local known-working version fetched under the name `grpc-ivi` though theoretically any compatible version of gRPC should work.

`ivi-util.h` exposes several preprocessor directives for basic system-level configuration, particularly logging, that can be specified via `cmake -D`.  See the header comments.

## Usage

The primary interfaces are:
* `ivi-model.h` - data structure definitions
* `ivi-client.h` - individual client types
* `ivi-client-mgr.h` - management classes which own and manage instances of the various client types
* `ivi-config.h` - configuration parameters to initialize client class instances

The `IVIClientManagerAsync` class is a turnkey solution to manage the several client types and provide a simple non-blocking interface as well as robust fault-tolerance.  It binds application listener functions to the IVI engine's data streams and is necessary for the IVI engine to function correctly.  See the header comments for more usage information.

The `IVIClientManagerSync` class provides a blocking interface to the IVI API.  It is recommended to use this **only for debugging or manual operations**, as some IVI RPCs can take a very long time to return results (tens of seconds to minutes).

There is no SDK requirement to use the prewritten _ClientManager_ classes, they only rely on the public interfaces of the various Client wrappers and gRPC, there is no usage of `friend` semantics in the SDK.

The SDK is written with public dependencies only on basic STL types.  These types have been aliased in `ivi-types.h` should you decide to subsitute other STL-interface-compatible types, however be aware that the underlying protobuf and gRPC libraries heavily relies on STL.

## Minutae

* The SDK has **not** been tested against protobuf/gRPC's _Arena_ allocation configuration.
* The SDK has **not** been tested to run as a shared library (.so) / dynamically-linked library (DLL), though the necessary linkage specifiers are available and dynamic linkage can be enabled via the cmake option `IVI_SDK_EXPORT`.  If your application already uses a different version of protobuf or gRPC, you will have to either use dynamic linkage to the IVI SDK or reconfigure the IVI SDK build to use your version.  Remember to keep in mind the subtleties of dynamic linkage with exposed STL types.
* This C++ SDK somewhat mirrors the [IVI Java SDK](https://github.com/MythicalGames/ivi-sdk-java), with a few important semantic differences:
  * The C++ SDK only executes the stream callbacks when receiving server data.  It does **not** automatically call these _executors_ after receiving the results of semantically related unary RPCs, unary RPC result processing is entirely up to the client application, unlike the Java SDK.
  * Call errors on unary RPC requests are reported via the IVIResult.Status() code, exception semantics are **not** used for API errors.  Any C++ exceptions originating from the SDK are possible programming errors and may be reported as bugs.
  * gRPC internally will abort the running program if it runs into certain unrecoverable error states.  These are typically the result of configuration errors or programming errors.
  
## Development notes

If you have the desire to dive into the source and modify the clients or bypass the `IVIClientManager` types in favor of your own management mechanisms, here are some notes on the SDK implementation design:
* Classes/structs generally follow RAII and functional-composition design principles
* Heavy reliance on `C++11` semantics, including RVO / NRVO, lambda calculus, move semantics, aggregate initialization, and implicit member construction/destruction ordering
* Avoidance of `C++11` language / library features that are known to be deprecated in subsequent / future language standards.
* The boilerplate gRPC calls have been hidden away in some moderately complex class and function templates, start with `ivi-client-t.h` if you are curious about the inner workings.
* Virtual inheritance is entirely avoided in IVI classes, except where mandated by dependency interfaces.

