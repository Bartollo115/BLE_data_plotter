cmake_minimum_required(VERSION 3.20)
project(BleTelemetryDashboard VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL  OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)
add_subdirectory(external/simpleble/simpleble)
add_subdirectory(external/imgui)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ble.cpp
    src/packet.cpp
    src/data_queue.cpp
    src/plot_buffer.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    simpleble::simpleble
    imgui_plot
    glfw
)
