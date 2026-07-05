cmake_minimum_required(VERSION 3.10)

# Project Name and Version
project(CryptoWallet VERSION 1.0)

# Specify C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the 'include' directory so the compiler can find our header files
include_directories(include)

# Tell CMake to compile both main.cpp AND sqlite3.c into our executable
# (Make sure this is the ONLY add_executable line in this file!)
add_executable(CryptoWallet src/main.cpp src/sqlite3.c)

# Add threading support (SQLite requires this on some systems)
find_package(Threads REQUIRED)
target_link_libraries(CryptoWallet Threads::Threads)

# Link Windows networking libraries (required by Crow)
if(WIN32)
    target_link_libraries(CryptoWallet ws2_32 mswsock)
endif()
