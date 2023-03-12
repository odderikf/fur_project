#pragma once

#include <glad/glad.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

class Shader {
    GLuint id = -1;
public:
    Shader(){};
    void create();
    void attach(std::string const &filename) const;
    void activate() const{
        glUseProgram(id);
    };

};

