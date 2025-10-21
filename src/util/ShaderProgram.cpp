#include "ShaderProgram.h"
#include "Util.h"
#include <fstream>
#include <sstream>
#include <iostream>

GLuint ShaderProgram::compile(GLenum type, const std::string& pathIn) 
{
    auto path = resolveShaderPath(pathIn);
    std::ifstream f(path);

    if (!f) 
    {
        std::cerr << "Shader Program could not read "
            << (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
            << " shader: " << pathIn << "\n";

        return 0;
    }

    std::stringstream ss; 
    ss << f.rdbuf();
    std::string src = ss.str();
    GLuint sh = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(sh, 1, &c, nullptr);
    glCompileShader(sh);
    GLint ok = 0; 
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (!ok) 
    {
        GLint len = 0; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0'); glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::cerr << "Shader Program Compile failed (" << path.string() << "):\n" << log << "\n";
        glDeleteShader(sh); return 0;
    }

    return sh;
}

bool ShaderProgram::loadFromFiles(const std::string& vs, const std::string& fs) 
{
    destroy();
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);

    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return false; }

    programId = glCreateProgram();
    glAttachShader(programId, v);
    glAttachShader(programId, f);
    glLinkProgram(programId);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(programId, GL_LINK_STATUS, &ok);

    if (!ok) 
    {
        GLint len = 0; glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0'); glGetProgramInfoLog(programId, len, nullptr, log.data());
        std::cerr << "Shader Program Link failed:\n" << log << "\n";
        glDeleteProgram(programId); programId = 0; return false;
    }

    return true;
}

void ShaderProgram::use() const { glUseProgram(programId); }
void ShaderProgram::destroy() { if (programId) glDeleteProgram(programId), programId = 0; }