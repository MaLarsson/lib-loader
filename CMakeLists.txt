cmake_minimum_required(VERSION 3.15)
project(lib-loader CXX)

add_executable(ll main.cpp lib_loader.cpp)

target_compile_features(ll PRIVATE cxx_std_11)

add_subdirectory(deps/dyncall)
target_link_libraries(ll PRIVATE dyncall_s)
target_include_directories(ll PRIVATE deps/dyncall/dyncall)
