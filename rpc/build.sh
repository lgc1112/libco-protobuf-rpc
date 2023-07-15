###
 # @file: 
 # @Author: regangcli
 # @copyright: Tencent Technology (Shenzhen) Company Limited
 # @Date: 2023-06-15 16:49:43
 # @edit: regangcli
 # @brief: 
### 

# (cd pb && ../../protobuf-3.20.0/src/protoc *.proto --cpp_out=.)
# rm ./CMakeCache.txt ./CMakeFiles -rf cmake_install.cmake Makefile librpc.a core* client server

# export LD_LIBRARY_PATH="../../protobuf-3.20.0/src/.libs:${LD_LIBRARY_PATH}"
# build_dir="./build/"
# rm -fr ${build_dir}
# mkdir -p ${build_dir} && cd ${build_dir}

# cmake -DCMAKE_BUILD_TYPE=Debug .. && make VERBOSE=1 -j10

# pkill server
# ./server
