cmake_minimum_required(VERSION 3.6)
project(fur_project)

set(CMAKE_CXX_STANDARD 17)

include_directories (src/)
add_executable(${PROJECT_NAME} src/main.cpp src/game.cpp
        src/gamelogic.cpp src/scenegraph.cpp
        src/utilities/timeutils.cpp src/utilities/glfont.cpp src/utilities/glutils.cpp
        src/utilities/imageLoader.cpp src/utilities/shapes.cpp src/utilities/mesh.cpp)

add_definitions (-DPROJECT_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\")

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT glowbox)

if (MSVC)
#    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1") # todo figure out how to add wpedantic on msvc in this hell system
#    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG")
else()
    set(DEBUG_CXX_FLAGS "-g -Wall -Wextra -Wpedantic")
    set(RELEASE_CXX_FLAGS "-O3")
endif()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(${PROJECT_NAME} PUBLIC __DEBUG__)
    if (MSVC)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -g -rdynamic -Wall -Wextra -Wpedantic -O0)
    endif()
else()
    if (MSVC)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -O3)
    endif()
endif()

#glfw
option (GLFW_INSTALL OFF)
option (GLFW_BUILD_DOCS OFF)
option (GLFW_BUILD_EXAMPLES OFF)
option (GLFW_BUILD_TESTS OFF)
include_directories(lib/glfw/include)
add_subdirectory(lib/glfw)
add_definitions(-DGLFW_INCLUDE_NONE)
target_link_libraries(${PROJECT_NAME} glfw)

#glad
include_directories(lib/glad_gen/include)
add_subdirectory(lib/glad/cmake)
target_sources(${PROJECT_NAME} PRIVATE lib/glad_gen/src/glad.c)

#glm
include_directories(lib/glm/)

#fmt
add_subdirectory (lib/fmt)
target_link_libraries (${PROJECT_NAME} fmt::fmt)

#tinyobj
include_directories(lib/tinyobjloader)

#lodepng
include_directories(lib/lodepng)
target_sources(${PROJECT_NAME} PRIVATE lib/lodepng/lodepng.cpp)