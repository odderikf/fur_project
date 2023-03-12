#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "game_logic.hpp"


void handle_poll_events(GLFWwindow *window){

}


void run_game(GLFWwindow* window){

    initialize_game(window);

    // Rendering Loop
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwPollEvents();
        handle_poll_events(window);

        update(window);
        render(window);

        // Flip buffers
        glfwSwapBuffers(window);
    }
}