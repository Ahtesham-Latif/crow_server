cmake_minimum_required(VERSION 3.10)
project(crow_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Windows 10 compatibility for cpp-httplib
add_definitions(-D_WIN32_WINNT=0x0A00)

# Force console subsystem for MinGW
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console")

# Crow include
include_directories(${CMAKE_SOURCE_DIR}/Crow/include)

# Add cpp-httplib header (manual download)
include_directories(${CMAKE_SOURCE_DIR}/httplib)

# Add executable
add_executable(server main.cpp)

# Link libraries
target_link_libraries(server
    sqlite3
    ws2_32
    mswsock
    advapi32
)

message(STATUS "Crow + SQLite3 + cpp-httplib configured successfully!")
message(STATUS "Don't forget to update N8N_WEBHOOK_PATH in main.cpp!")