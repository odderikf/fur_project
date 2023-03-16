#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include "gamelogic.h"


void handle_poll_events(GLFWwindow *window){

}

void handleKeyboardInput(GLFWwindow* window)
{
    // Use escape key for terminating the GLFW window
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

void run_game(GLFWwindow* window){

    initialize_game(window);

    // Rendering Loop
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwPollEvents();
        handle_poll_events(window);

        updateFrame(window);
        renderFrame(window);

        handleKeyboardInput(window);
        // Flip buffers
        glfwSwapBuffers(window);
    }
}
