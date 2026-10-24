cmake_minimum_required(VERSION 3.20)
project(VPU_TaskScheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(scheduler src/main.cpp)
target_include_directories(scheduler PRIVATE include)
target_link_libraries(scheduler PRIVATE Threads::Threads)
