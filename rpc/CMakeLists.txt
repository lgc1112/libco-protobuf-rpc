set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/pb
    ${ROOT_DIR}/protobuf-3.20.0/src
    ${ROOT_DIR}/libco
    ${ROOT_DIR}/llbc/llbc/include)
link_directories(
    ${ROOT_DIR}/protobuf-3.20.0/src/.libs
    ${ROOT_DIR}/libco/solib
    ${ROOT_DIR}/llbc/output/gmake/release64
    ${CMAKE_CURRENT_SOURCE_DIR}/build)

# include_directories(
#     ./src
#     ./pb
#     ../protobuf-3.20.0/src
#     ../libco
#     ../llbc/llbc/include)
# link_directories(
#     ../protobuf-3.20.0/src/.libs
#     ../libco/solib
#     ../llbc/output/gmake/release64
#     ./build)

# SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
file(GLOB PB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/pb/*.cc")
file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")


set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
add_executable(client ${PB_FILES} ${SRC_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client.cpp)
target_link_libraries(client libprotobuf.a  libllbc.so libcolib.so)
add_executable(server ${PB_FILES} ${SRC_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/src/server/server.cpp)
target_link_libraries(server libprotobuf.a libllbc.so libcolib.so)
add_executable(server2 ${PB_FILES} ${SRC_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/src/server/server2.cpp)
target_link_libraries(server2 libprotobuf.a libllbc.so libcolib.so)

add_executable(client_stress_test ${PB_FILES} ${SRC_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client_stress_test.cpp)
target_link_libraries(client_stress_test libprotobuf.a  libllbc.so libcolib.so)
