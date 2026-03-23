cmake_minimum_required(VERSION 3.23)
project(clix_test_package LANGUAGES CXX)

find_package(clix CONFIG REQUIRED)

add_executable(clix_test_package src/test_package.cpp)
target_link_libraries(clix_test_package PRIVATE clix::clix)
target_compile_features(clix_test_package PRIVATE cxx_std_17)
