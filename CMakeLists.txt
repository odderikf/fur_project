cmake_minimum_required(VERSION 3.20)
project(fur_project)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} src/main.cpp src/game.cpp src/game_logic.cpp
        src/scenegraph.cpp src/shader.cpp src/utils/mesh.cpp)

#glfw
option (GLFW_INSTALL OFF)
option (GLFW_BUILD_DOCS OFF)
option (GLFW_BUILD_EXAMPLES OFF)
option (GLFW_BUILD_TESTS OFF)
include_directories(lib/glfw/include)
add_subdirectory(lib/glfw)
add_definitions(-DGLFW_INCLUDE_NONE)

#glad
include_directories(lib/glad/include)
add_subdirectory(lib/glad/cmake)
target_sources(${PROJECT_NAME} PRIVATE lib/glad/src/glad.c)

#glm
include_directories(lib/glm/)

#tinyobj
include_directories(lib/tinyobjloader)

#GL
find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} glfw opengl32)
