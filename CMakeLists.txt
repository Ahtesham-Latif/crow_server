cmake_minimum_required(VERSION 3.15)
project(crow_server LANGUAGES CXX)

# ---- C++17 (MANDATORY for Crow) ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Windows compatibility ----
add_definitions(-D_WIN32_WINNT=0x0A00)

# ---- MinGW console subsystem ----
if (MINGW)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console"
    )
endif()

# ---- Include directories ----
include_directories(
    ${CMAKE_SOURCE_DIR}/Crow/include
    ${CMAKE_SOURCE_DIR}/httplib
    ${CMAKE_SOURCE_DIR}/controllers
    ${CMAKE_SOURCE_DIR}/models
)

# ---- Executable (ALL .cpp FILES MUST BE LISTED) ----
add_executable(server
    main.cpp
    controllers/category_controller.cpp
    controllers/doctor_controller.cpp
    controllers/schedule_controller.cpp
    controllers/patient_controller.cpp
    controllers/appointment_controller.cpp
)

# ---- Libraries ----
target_link_libraries(server
    sqlite3
    ws2_32
    mswsock
    advapi32
)

# ---- Info ----
message(STATUS "Crow + SQLite3 + cpp-httplib configured successfully!")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
