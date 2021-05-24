# CMakeList.txt : CMake project for ivi-sdk-native, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.12)

project ("ivi-sdk-example" C CXX)

set(ivi_sdk_example_src
	"src/ivi-sdk-example.cpp")

add_executable(ivi-sdk-example ${ivi_sdk_example_src})
	
target_include_directories( ivi-sdk-example
    PUBLIC ${ivi-sdk-cpp_INCLUDE_DIR} 
    PUBLIC "${grpc_SOURCE_DIR}/third_party/protobuf/src")

target_link_libraries( ivi-sdk-example 
	PUBLIC ivi-sdk-cpp )

# static linkage
add_compile_definitions(IVI_SDK_NOEXPORT)

#if(MSVC)
#    add_compile_options(/wd4251)
#endif() # MSVC
