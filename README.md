# IVI C++ SDK

## General info
The *IVI C++ SDK* is a turnkey wrapper and management layer around the underlying [IVI gRPC API](https://github.com/MythicalGames/ivi-sdk-proto/).

## Building

* Uses CMake.
* Tested against Win32 MSVC 16 (VS 2019) and Ubuntu 20.04 with both gcc and clang.
* Should be compatible with any _C++11_ compliant toolchain that can build gRPC, and should also work with _C++14_ and _C++17_ language standards.

Given cmake and an appropriate C++ toolchain installed, from the root directory, to get the example up and running simply run:
* `cmake -B <build-dir> -S <ivi-sdk-cpp-dir>` to auto-configure the cmake cache and download dependencies.  Add `-DBUILD_TESTING=ON` if you wish to build the googletest-based unit testing executable as well.
* `cmake --build <build-dir>` to compile everything, including the example program `ivi-sdk-example`
* Pass cmake `-DBUILD_TESTING=ON` to configure the unit test executable `ivi-sdk-test` available in the `ivi-sdk-cpp/tests/` subdirectory.

See `ivi-sdk-example.cpp` for a basic demonstration on how to interact with the SDK.

## Running the Example

Run the `ivi-sdk-example` executable to see it connect and perform some trivial and naive calls to the IVI API, including bad RPC calls to demonstrate error handling.  You will have to supply a valid environment ID, API key, and optionally a specific IVI host as command line arguments or environment variables.

Additionally, on Windows you will need to manually supply a certificate PEM file for gRPC's SSL connection. The one packaged with gRPC is sufficient and can be specified by setting this [gRPC environment variable](https://github.com/grpc/grpc/blob/master/doc/environment_variables.md) appropriately after running cmake:
```GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=<CMAKE_BUILD_ROOT>\_deps\grpc-ivi-src\etc\roots.pem```

## Application Integration

The IVI SDK's `CMakeLists.txt` configurations are purposely minimalist and rely on [`FetchContent`](https://cmake.org/cmake/help/latest/module/FetchContent.html) for the gRPC dependency to simplify integration into other dependency trees, and are based on [gRPC C++'s recommendations](https://github.com/grpc/grpc/tree/master/src/cpp).  The IVI cmake configuration purposely eschews system-installed gRPC detection in preference of a local known-working version fetched under the name `grpc-ivi`.  Theoretically any compatible version of gRPC should work but in practice this may cause problems.  If you do not use `cmake` as or in your primary build system, you of course can prebuild the `ivi-sdk-cpp` library and copy the headers, just keep in mind ABI compatibility and the public STL dependencies will require consistent compilation configurations.

The SDK is built as a static library by default.  It is buildable as a shared library by passing cmake `-DIVI_SDK_SHARED_LIB=ON`.  This may be necessary eg if the application binary requires linking different versions of gRPC.  If this applies to your application, be aware that some gRPC objects (eg, `grpc::Channel`) will not function correctly if allocated from a different library instance from where they are passed (eg, thread-local-storage bugs).

`ivi-util.h` exposes several preprocessor and runtime directives for basic configuration, particularly logging, that can also be specified via `cmake -D` or `add_compile_definition`.  You will want to examine these when making your application production-ready.  See the header comments.

## API Basics

The primary interfaces exposed by the `ivi-sdk-cpp` lib are:
* `ivi-model.h` - data structure definitions
* `ivi-client.h` - individual client types
* `ivi-client-mgr.h` - management classes which own and manage instances of the various client types
* `ivi-config.h` - configuration parameters to initialize client class instances

The `IVIClientManagerAsync` class is a turnkey solution to manage the several client types and provide a simple non-blocking interface as well as robust fault-tolerance.  It binds application listener functions to the IVI engine's data streams; proper stream processing is necessary for the IVI engine to operate.  See the header comments for more usage information.

The `IVIClientManagerSync` class provides a blocking interface to the IVI API.  It is recommended to use this **only for debugging or manual operations**, as some IVI RPCs can block for a very long time before returning results (tens of seconds to minutes).

There is no SDK requirement to use the prewritten _ClientManager_ classes, they only rely on the public interfaces of the various Client wrappers and gRPC; there is no usage of `friend` semantics in the SDK or other tight coupling.

The default cmake configuration exposes the generated protobuf and gRPC headers via a separate library, `ivi-sdk-cpp-generated`, which is specifically not listed as a public dependency of `ivi-sdk-cpp` to prevent massive header pollution from protobuf and gRPC.  Adding the generated lib to your own dependency tree allows you to:
* Manipulate lower-level gRPC parameters, such as connection and channel arguments.
* Write your own Client or ClientManager implementations, eg to use your own I/O threading semantics with gRPC.

The SDK is written with public dependencies only on basic STL types.  These types have been aliased in `ivi-types.h` should you decide to subsitute other STL-interface-compatible types, however be aware that the underlying protobuf and gRPC libraries heavily rely on STL.  If you do subsitute your own types into ivi-types.h, it is strongly advised to ensure the `ivi-sdk-test` unit tester passes all tests.

## Minutae

* The SDK has **not** been tested against protobuf/gRPC's _Arena_ allocation configuration.
* This C++ SDK somewhat mirrors the [IVI Java SDK](https://github.com/MythicalGames/ivi-sdk-java), with a few important semantic differences:
  * The C++ SDK only executes the stream callbacks when receiving server data.  It does **not** automatically call these _executors_ after receiving the results of semantically related unary RPCs, unary RPC result processing is entirely up to the client application, **unlike** the Java SDK.  Instead the results are returned to the caller to passed to the passed-in callback for sync and async clients respectively.
  * Call errors on unary RPC requests are reported via the IVIResult.Status() code, exception semantics are **not** used for API errors.  Any C++ exceptions originating from the SDK are possible programming errors and may be reported as bugs.
  * gRPC internally will abort the running program if it runs into certain unrecoverable error states.  These are typically the result of configuration errors or programming errors.  A handful of known detectable errors will also lead to the program-exit behavior from the SDK unless `IVI_ENABLE_EXIT_ON_FAIL_CHECK` is set to 0 at build time.
  
## Development notes

If you have the desire to dive into the source and modify the clients or bypass the `IVIClientManager` types in favor of your own management mechanisms, here are some notes on the SDK implementation design:
* Classes/structs generally follow RAII and functional-composition design principles
* Heavy reliance on `C++11` semantics, including RVO / NRVO, lambda calculus, move semantics, aggregate initialization, and implicit member construction/destruction ordering
* Avoidance of `C++11` language / library features that are known to be deprecated in subsequent / future language standards.
* The boilerplate gRPC calls have been hidden away in some moderately complex class and function templates, start with `ivi-client-t.h` if you are curious about the inner workings.

