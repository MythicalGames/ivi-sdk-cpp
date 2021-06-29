cmake_minimum_required (VERSION 3.5)

# Bare bones example top-level configuration

project ("ivi-sdk-native")

if(IVI_SDK_SHARED_LIB)
	if(MSVC)
		add_compile_options(/wd4251) #disable warnings about STL linkage
	endif() # MSVC
	if(WIN32)
		# Naive binary output settings if we're a Windows DLL
		set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
	endif()
endif() #IVI_SDK_SHARED_LIB

add_subdirectory("ivi-sdk-cpp")

add_subdirectory("ivi-sdk-example")

