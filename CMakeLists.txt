cmake_minimum_required(VERSION 3.18)
project(FadyArenaAscension LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
FetchContent_MakeAvailable(raylib)

set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_WS ON CACHE BOOL "" FORCE)
FetchContent_Declare(ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.6
)
FetchContent_MakeAvailable(ixwebsocket)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

add_executable(FadyArenaAscension src/main.cpp)
target_link_libraries(FadyArenaAscension PRIVATE raylib ixwebsocket nlohmann_json::nlohmann_json)

if(APPLE)
    target_link_libraries(FadyArenaAscension PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

if(WIN32)
    target_link_libraries(FadyArenaAscension PRIVATE winmm)
endif()
