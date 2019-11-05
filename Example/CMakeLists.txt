cmake_minimum_required(VERSION 3.15)
project(Example)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED COMPONENTS system iostreams)
link_directories(../cmake-build-debug)

add_executable(Example main.cpp)
target_link_libraries(Example test_ID_R_D ${Boost_LIBRARIES})