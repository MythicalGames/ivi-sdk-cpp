﻿# CMakeList.txt : CMake project for ivi-sdk-cpp
cmake_minimum_required (VERSION 3.12)

project ("ivi-sdk-cpp" C CXX)

option(IVI_SDK_SHARED_LIB "Compile as .so / .dll" OFF)

# Explicitly statically link against our own specific gRPC version 
# (pulled from Git) because we want to ensure version matching with
# the IVI server.  
# While gRPC stipulates wire-compatibility across release versions,
# later comments in gRPC issue #17352 do indicate real and potential
# ways to break communication via gRPC dependencies, eg breaking 
# changes in the encryption layers.
# Static linkage to a gRPC version here combined with building 
# ivi-sdk as a shared library ensures that any wire-compatibility 
# problems that arise with the IVI servers will be traceable to an IVI
# change, and will also prevent conflicts with an IVI-dependent 
# application that has its own version of gRPC.
# Potentially this should also pull in an explicit version of boringssl
# here because that library explicitly documents not to rely
# on cross-version compatibility and is used over the wire.
# Ref: https://github.com/grpc/grpc/issues/17352
# Ref: https://boringssl.googlesource.com/boringssl/

message(STATUS "Using gRPC via add_subdirectory (FetchContent).")
include(FetchContent)
FetchContent_Declare(
	grpc-ivi
	GIT_REPOSITORY https://github.com/grpc/grpc.git
	GIT_TAG        v1.34.0)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(grpc-ivi)

# Since FetchContent uses add_subdirectory under the hood, we can use
# the grpc targets directly from this build.
set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_GRPCPP grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

FetchContent_Declare(
	ivi-sdk-proto
	GIT_REPOSITORY https://github.com/MythicalGames/ivi-sdk-proto
	GIT_TAG        v2.0
)
FetchContent_Populate(ivi-sdk-proto)

set(ivi_sdk_src
	"src/ivi-client.cpp"
	"src/ivi-client-mgr.cpp"
	"src/ivi-config.cpp"
	"src/ivi-enum.cpp"
	"src/ivi-model.cpp"
	"src/ivi-sdk.cpp" 
	"src/ivi-util.cpp"
)

set(ivi_sdk_hdrs
	"include/ivi/ivi-client.h"
	"include/ivi/ivi-client-mgr.h"
	"include/ivi/ivi-client-t.h"
	"include/ivi/ivi-config.h"
	"include/ivi/ivi-enum.h"
	"include/ivi/ivi-executor.h"
	"include/ivi/ivi-model.h"
	"include/ivi/ivi-sdk.h" 
	"include/ivi/ivi-types.h"
	"include/ivi/ivi-util.h" 
)

set(ivi_protos_base_names
	"api/item/definition"
	"api/item/rpc"
	"api/itemtype/definition"
	"api/itemtype/rpc"
	"api/order/definition"
	"api/order/payment"
	"api/order/rpc"
	"api/payment/definition"
	"api/payment/rpc"
	"api/player/definition"
	"api/player/rpc"
	"common/common"
	"common/finalization"
	"common/sort"
	"common/item/definition"
	"common/itemtype/definition"
	"common/order/definition"
	"common/player/definition"
	"streams/common"
	"streams/item/stream"
	"streams/itemtype/stream"
	"streams/order/stream"
	"streams/ping/stream"
	"streams/player/stream"
)

set(ivi_proto_gen_base_dir ${ivi-sdk-proto_BINARY_DIR})
set(ivi_proto_gen_dir "${ivi_proto_gen_base_dir}/ivi/generated")
set(ivi_proto_include_dirs "${grpc-ivi_SOURCE_DIR}/third_party/protobuf/src") # libprotobuf doesn't get us this it seems

file(MAKE_DIRECTORY ${ivi_proto_gen_dir})

#construct our lists of source & generated proto files
list(TRANSFORM ivi_protos_base_names
	PREPEND "${ivi-sdk-proto_SOURCE_DIR}/"
	OUTPUT_VARIABLE ivi_protos)
list(TRANSFORM ivi_protos
	APPEND ".proto")

list(TRANSFORM ivi_protos_base_names
	PREPEND "${ivi_proto_gen_dir}/"
	OUTPUT_VARIABLE ivi_proto_srcs)
list(TRANSFORM ivi_proto_srcs
	APPEND ".pb.cc")

list(TRANSFORM ivi_protos_base_names
	PREPEND "${ivi_proto_gen_dir}/"
	OUTPUT_VARIABLE ivi_proto_hdrs)
list(TRANSFORM ivi_proto_hdrs
	APPEND ".pb.h")

list(TRANSFORM ivi_protos_base_names
	PREPEND "${ivi_proto_gen_dir}/"
	OUTPUT_VARIABLE ivi_grpc_srcs)
list(TRANSFORM ivi_grpc_srcs
	APPEND ".grpc.pb.cc")

list(TRANSFORM ivi_protos_base_names
     PREPEND "${ivi_proto_gen_dir}/"
     OUTPUT_VARIABLE ivi_grpc_hdrs_base)

list(TRANSFORM ivi_grpc_hdrs_base
	APPEND ".grpc.pb.h"
	OUTPUT_VARIABLE ivi_grpc_hdrs)

add_custom_command(
      OUTPUT ${ivi_proto_srcs} ${ivi_proto_hdrs} ${ivi_grpc_srcs} ${ivi_grpc_hdrs}
      DEPENDS ${ivi_protos}
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS 
	--grpc_out "${ivi_proto_gen_dir}"
        --cpp_out "${ivi_proto_gen_dir}"
        -I "${ivi-sdk-proto_SOURCE_DIR}" 
        -I "${ivi_proto_include_dirs}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        ${ivi_protos}
)

add_library(ivi-sdk-cpp-generated STATIC
	${ivi_proto_srcs}     
	${ivi_proto_hdrs}     
	${ivi_grpc_srcs} 
	${ivi_grpc_hdrs} 
)

target_include_directories( ivi-sdk-cpp-generated
	PUBLIC ${ivi_proto_gen_dir}
	PUBLIC ${ivi_proto_gen_base_dir}
	PUBLIC ${ivi_proto_include_dirs}
)

target_link_libraries(ivi-sdk-cpp-generated
	PUBLIC ${_REFLECTION}
	PUBLIC ${_GRPC_GRPCPP}
	PUBLIC ${_PROTOBUF_LIBPROTOBUF}
)

# static vs dynamic linkage specifier for IVI
if(IVI_SDK_SHARED_LIB)

	add_library (ivi-sdk-cpp SHARED
		${ivi_sdk_src}
		${ivi_sdk_hdrs})
	
	target_compile_definitions(ivi-sdk-cpp PRIVATE IVI_SDK_EXPORT)
	# only export the explicity exported IVI_SDK_API interfaces
	set_property(TARGET ivi-sdk-cpp PROPERTY C_VISIBILITY_PRESET hidden)
	set_property(TARGET ivi-sdk-cpp PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET ivi-sdk-cpp PROPERTY VISIBILITY_INLINES_HIDDEN ON)
	if(MSVC)
		add_compile_options(/wd4251) #disable warnings about STL linkage
	else()
		set_property(TARGET ivi-sdk-cpp 
			PROPERTY POSITION_INDEPENDENT_CODE 1)
		set_property(TARGET ivi-sdk-cpp-generated 
			PROPERTY POSITION_INDEPENDENT_CODE 1)
	endif() # MSVC
	
else() # NOT IVI_SDK_EXPORT

	add_library (ivi-sdk-cpp STATIC
		${ivi_sdk_src}
		${ivi_sdk_hdrs})
	target_compile_definitions(ivi-sdk-cpp PUBLIC IVI_SDK_NOEXPORT)
	
endif() # IVI_SDK_EXPORT

target_include_directories(ivi-sdk-cpp
	PUBLIC include 
)
target_link_libraries(ivi-sdk-cpp
	PRIVATE ivi-sdk-cpp-generated
)

if(BUILD_TESTING)
	add_subdirectory("tests")
endif()

set_target_properties(ivi-sdk-cpp 
	PROPERTIES PUBLIC_HEADER "${ivi_sdk_hdrs}")

install(TARGETS ivi-sdk-cpp
	PUBLIC_HEADER DESTINATION "include/ivi"
)
