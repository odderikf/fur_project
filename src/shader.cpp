//
// Created by odder on 10/03/2023.
//

#include "shader.hpp"
void Shader::create(){
    id = glCreateProgram();
}

void Shader::attach(const std::string &filename) const {
    GLuint shader;

    std::filesystem::path p = filename;
    std::ifstream file(p);
    if(file.fail()){
        std::cerr << "Could not open file: " << p << std::endl;
        return;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    auto ext = p.extension();
    if (ext == ".comp")      shader = glCreateShader(GL_COMPUTE_SHADER);
    else if (ext == ".frag") shader = glCreateShader(GL_FRAGMENT_SHADER);
    else if (ext == ".geom") shader = glCreateShader(GL_GEOMETRY_SHADER);
    else if (ext == ".tcs")  shader = glCreateShader(GL_TESS_CONTROL_SHADER);
    else if (ext == ".tes")  shader = glCreateShader(GL_TESS_EVALUATION_SHADER);
    else if (ext == ".vert") shader = glCreateShader(GL_VERTEX_SHADER);
    else {
        std::cerr << "invalid shader file: " << p << std::endl;
        return;
    }
    std::string src = ss.str();
    const char * source = src.c_str();
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(!status){
        GLint errlen;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &errlen);
        auto err = new char[errlen];
        glGetShaderInfoLog(shader, errlen, nullptr, err);
        std::cerr << p << std::endl << err << std::endl;
        delete[] err;
        return;
    }
    glAttachShader(id, shader);
    glDeleteShader(shader);
}
