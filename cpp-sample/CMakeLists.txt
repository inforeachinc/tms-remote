cmake_minimum_required(VERSION 3.13)

project(TMSClientApp)


# Look for Protobuf installation
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using Protobuf ${Protobuf_VERSION}")

# Look for gRPC installation
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

# Look for Protobuf compiler to process *.proto files
find_program(_PROTOBUF_PROTOC protoc)
find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)


# Generate sources from *.proto files
set(TMS_API_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/tmsapigrpc)
file(MAKE_DIRECTORY ${TMS_API_GENERATED_DIR})
get_filename_component(TMS_API_PROTO_DIR ".." ABSOLUTE)
foreach(TMS_API_PROTO_NAME
  TMSRemote
  TMSRemoteCommon
  TMSRemoteEvents
  TMSRemoteRequests
  TMSTradingRequests)
    set(TMS_API_PROTO "${TMS_API_PROTO_DIR}/${TMS_API_PROTO_NAME}.proto")
    set(TMS_API_PROTO_SRC "${TMS_API_GENERATED_DIR}/${TMS_API_PROTO_NAME}.pb.cc")
    set(TMS_API_PROTO_HDR "${TMS_API_GENERATED_DIR}/${TMS_API_PROTO_NAME}.pb.h")
    set(TMS_API_GRPC_SRC "${TMS_API_GENERATED_DIR}/${TMS_API_PROTO_NAME}.grpc.pb.cc")
    set(TMS_API_GRPC_HDR "${TMS_API_GENERATED_DIR}/${TMS_API_PROTO_NAME}.grpc.pb.h")
    add_custom_command(
	  OUTPUT "${TMS_API_PROTO_SRC}" "${TMS_API_PROTO_HDR}" "${TMS_API_GRPC_SRC}" "${TMS_API_GRPC_HDR}"
	  COMMAND ${_PROTOBUF_PROTOC}
	  ARGS --grpc_out "${TMS_API_GENERATED_DIR}"
		--cpp_out "${TMS_API_GENERATED_DIR}"
		-I "${TMS_API_PROTO_DIR}"
		--plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
		"${TMS_API_PROTO}"
	  DEPENDS "${TMS_API_PROTO}")
    list(APPEND TMS_API_PROTO_SRCS ${TMS_API_PROTO_SRC})
    list(APPEND TMS_API_PROTO_HDRS ${TMS_API_PROTO_HDR})
    list(APPEND TMS_API_GRPC_SRCS ${TMS_API_GRPC_SRC})
    list(APPEND TMS_API_GRPC_HDRS ${TMS_API_GRPC_HDR})
endforeach()

# Include generated headers
include_directories("${CMAKE_CURRENT_BINARY_DIR}")


# Create TMS API gRPC library
add_library(tms_api_grpc_lib
  ${TMS_API_GRPC_SRCS}
  ${TMS_API_GRPC_HDRS}
  ${TMS_API_PROTO_SRCS}
  ${TMS_API_PROTO_HDRS})
target_link_libraries(tms_api_grpc_lib
  gRPC::grpc++
  protobuf::libprotobuf)


# Create TMS Client App executable
set(TMS_CLIENT_APP_SRCS
  AsyncListenerBase.cpp
  AsyncListenersManager.cpp
  ClientAppGrpc.cpp
  StatefulSubscriber.cpp
  StatelessSubscriber.cpp
  Utils.cpp)
add_executable(tms_client_app ${TMS_CLIENT_APP_SRCS})
target_link_libraries(tms_client_app PRIVATE 
  tms_api_grpc_lib
  gRPC::grpc++
  protobuf::libprotobuf)
