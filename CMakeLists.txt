cmake_minimum_required(VERSION 3.20)
project(BleTelemetryDashboard VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

find_package(OpenGL REQUIRED)

add_subdirectory(external/glfw)

add_subdirectory(external/simpleble/simpleble)

set(IMGUI_DIR external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp # Assumes OpenGL3 for rendering
)

set(IMPLOT_DIR external/implot)
set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ble.cpp
    src/packet.cpp
    src/data_queue.cpp
    src/plot_buffer.cpp
    src/logger.cpp
    src/csv_loader.cpp
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
    ${OPENGL_INCLUDE_DIR}
)

# ENABLE_LOGGING is set for Debug builds; Release gets no logging overhead.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:ENABLE_LOGGING>
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    src 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    simpleble::simpleble
    glfw
    OpenGL::GL  # Add this
)