PROJECT(cogserver CXX)
cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMake 3.1.0+ required.
# See: https://stackoverflow.com/a/29871891
#set(THREADS_PREFER_PTHREAD_FLAG ON)
#find_package(Threads REQUIRED)

# Boost 1.66+ required.
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS system program_options date_time filesystem)

add_executable(cogserver main.cpp)
target_link_libraries(cogserver PUBLIC ${Boost_LIBRARIES})
