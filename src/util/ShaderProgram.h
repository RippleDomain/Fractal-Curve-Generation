#pragma once

#include <string>
#include <glad/glad.h>

class ShaderProgram 
{
public:
    bool loadFromFiles(const std::string& vs, const std::string& fs);
    void use() const;
    GLuint id() const { return programId; }
    void destroy();
private:
    GLuint programId{ 0 };
    GLuint compile(GLenum type, const std::string& path);
};