cmake_minimum_required(VERSION 3.15)
project(test_ID_R_D)

set(CMAKE_CXX_STANDARD 17)

add_library(test_ID_R_D SHARED library.cpp library.h K_means_processor.cpp K_means_processor.h Buffer.hpp)