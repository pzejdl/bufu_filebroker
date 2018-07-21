# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

project(http_server)

# C++ compiler flags
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wextra -rdynamic -O0 -g")

# Sets the version we have
#set(Boost_ADDITIONAL_VERSIONS "1.53")
set(BOOST_ROOT /opt/bufu_fileserver/boost)

# Requires multithreaded librariesg
set(Boost_USE_MULTITHREADED ON)

# Automatically finds boost
find_package(Boost 1.67 REQUIRED COMPONENTS system)

# Using pthread
find_package(Threads REQUIRED)

# Defines the executable
add_executable(main server/request.cpp server/request_handler.cpp server/listener.cpp server/server.cpp main.cpp)

# Specifies include paths
target_include_directories(main PRIVATE server ${Boost_INCLUDE_DIRS})

# Workaround for pthread
target_compile_options(main PUBLIC "-pthread")

# Specifies link paths
target_link_libraries(main ${Boost_LIBRARIES})
target_link_libraries(main ${CMAKE_THREAD_LIBS_INIT})
