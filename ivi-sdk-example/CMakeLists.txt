# CMakeList.txt : CMake project for ivi-sdk-native, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.12)

project ("ivi-sdk-example" C CXX)

set(ivi_sdk_example_src
	"src/ivi-sdk-example.cpp")

add_executable(ivi-sdk-example ${ivi_sdk_example_src})

target_link_libraries(ivi-sdk-example 
	PRIVATE ivi-sdk-cpp)
