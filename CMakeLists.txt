cmake_minimum_required(VERSION 3.10)
project(crow_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force console subsystem for MinGW
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console")

include_directories(${CMAKE_SOURCE_DIR}/Crow/include)

add_executable(server main.cpp)

# link Windows network libs needed by asio on Windows
target_link_libraries(server
    ws2_32
    mswsock
    advapi32
)
