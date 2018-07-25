# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8.10)

project(bufu_fileserver)
set (BUFU_FILESERVER_VERSION "\"0.52.2\"")

# C++ compiler flags
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wextra -rdynamic -O2 -g")

# TODO: - Static linking doesn't work at the moment
#       - Check https://stackoverflow.com/questions/35116327/when-g-static-link-pthread-cause-segmentation-fault-why/45271521#45271521 
#       - And enable Boost_USE_STATIC_LIBS below
#       - And probably disable "find_package(Threads REQUIRED)"
#set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wextra -rdynamic -O0 -g -static -static-libstdc++ -static-libgcc -pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")

# Check for specific compiler version
if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  message(FATAL_ERROR "This project is tested only with GNU G++ compiler.")
endif()
if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.3)
  message(FATAL_ERROR "GNU G++ version must be at least 6.3.0")
endif()


# Sets the version we have
#set(Boost_ADDITIONAL_VERSIONS "1.53")
set(BOOST_ROOT /opt/bufu_fileserver/boost)

# Requires multithreaded librariesg
set(Boost_USE_MULTITHREADED ON)

# Require static compilation
#set(Boost_USE_STATIC_LIBS   ON)

# Automatically finds boost
find_package(Boost 1.67 REQUIRED COMPONENTS system filesystem regex chrono)

# Using pthread
find_package(Threads REQUIRED)

# configure a header file to pass some of the CMake settings to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
)

# TODO: Make a separate Makefile for http server
set(HTTP_SOURCES http/1.1/server/request.cpp http/1.1/server/request_handler.cpp http/1.1/server/listener.cpp http/1.1/server/server.cpp)

# Defines the executable
add_executable(main main.cc bu/RunDirectoryObserver.cc bu/RunDirectoryManager.cc bu/bu.cc tools/inotify/INotify.cc ${HTTP_SOURCES})

# Add the binary tree to the search path for include files so the config.h can be found
target_include_directories(main PRIVATE "${PROJECT_BINARY_DIR}")

# Specifies include paths
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${Boost_INCLUDE_DIRS})

# Workaround for pthread
target_compile_options(main PUBLIC "-pthread")

# Specifies link paths
target_link_libraries(main ${Boost_LIBRARIES})
target_link_libraries(main ${CMAKE_THREAD_LIBS_INIT})
