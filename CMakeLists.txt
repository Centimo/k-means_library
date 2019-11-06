cmake_minimum_required(VERSION 3.15)
project(test_ID_R_D)

set(CMAKE_CXX_STANDARD 17)

add_library(test_ID_R_D SHARED K_means_lib.cpp K_means_lib.h K_means_processor.cpp K_means_processor.h utils.hpp)