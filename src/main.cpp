#include <iostream>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "window.hpp"
#include "game.hpp"

static void glfwErrorCallback(int error, const char *description){
    std::cerr << "GLFW err: " << error << std::endl << description << std::endl << std::endl;
}

GLFWwindow* initialize_window(){
    if (!glfwInit()) {
        const char *err;
        glfwGetError(&err);
        std::cerr << "Could not start GLFW" << std::endl << err << std::endl;
        exit(EXIT_FAILURE);
    }
    glfwSetErrorCallback(glfwErrorCallback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_RESIZABLE, true);

    auto window = glfwCreateWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, DEFAULT_WINDOW_NAME.c_str(), nullptr, nullptr);

    glfwMakeContextCurrent(window);
    gladLoadGL();

    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    return window;
}



int main()
{
    GLFWwindow* window = initialize_window();
    run_game(window);

    // Terminate GLFW (no need to call glfwDestroyWindow)
    // todo gldeleteframebuffers?
    glfwTerminate();
    return EXIT_SUCCESS;
}
