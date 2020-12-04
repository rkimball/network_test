cmake_minimum_required(VERSION 3.10)

project(network_test)

add_executable(test main.cpp)

set_property(TARGET test PROPERTY CXX_STANDARD 14)
