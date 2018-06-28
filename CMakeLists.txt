# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

project(bufu_fileserver)

# C++ compiler flags
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wextra -rdynamic -O0 -g")

# Sets the version we have
set(Boost_ADDITIONAL_VERSIONS "1.53")

# Requires multithreaded librariesg
set(Boost_USE_MULTITHREADED ON)

# Automatically finds boost
find_package(Boost 1.53 REQUIRED COMPONENTS system filesystem regex)

# Using pthread
find_package(Threads REQUIRED)


# TODO: Make a separate Makefile for http server
set(HTTP_SOURCES http/server/connection.cpp http/server/connection_manager.cpp http/server/mime_types.cpp http/server/reply.cpp http/server/request_handler.cpp http/server/request_parser.cpp http/server/server.cpp)


# Defines the executable
add_executable(main main.cc bu/RunDirectoryObserver.cc bu/RunDirectoryManager.cc bu/bu.cc tools/inotify/INotify.cc ${HTTP_SOURCES})

# Specifies include paths
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${Boost_INCLUDE_DIRS})

# Workaround for pthread
target_compile_options(main PUBLIC "-pthread")

# Specifies link paths
target_link_libraries(main ${Boost_LIBRARIES})
target_link_libraries(main ${CMAKE_THREAD_LIBS_INIT})
