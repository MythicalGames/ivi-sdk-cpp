cmake_minimum_required(VERSION 3.14)
project(ivi-sdk-test)

# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 11)

include(CTest)
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY 	https://github.com/google/googletest.git
  GIT_TAG 			release-1.10.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
if(MSVC)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif() #MSVC

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(ivi-sdk-test 
	"ivi-sdk-test.cpp")

target_link_libraries(ivi-sdk-test
	PRIVATE ivi-sdk-cpp
	PRIVATE ivi-sdk-cpp-generated
	PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(ivi-sdk-test)

