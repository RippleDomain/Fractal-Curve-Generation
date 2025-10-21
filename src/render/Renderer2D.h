#pragma once

#include <glm.hpp>
#include <glad/glad.h>
#include <vector>
#include "../util/ShaderProgram.h"
#include "Geometry.h"

class Renderer2D 
{
public:
    ~Renderer2D() { shutdown(); }
    bool init();
    void shutdown();

    void begin(const glm::mat4& vp);
    void submitSegment(const glm::vec2& a, const glm::vec2& b, float thicknessPx, const Color& c);
    void submitPolyline(const std::vector<glm::vec2>& pts, float thicknessPx, const Color& c);
    void submitDisc(const glm::vec2& center, float radiusPx, const Color& c, int segs = 20);
    void end();
    void flush();

private:
    GLuint vao{ 0 }, vbo{ 0 }, ebo{ 0 };
    ShaderProgram program;
    Mesh mesh;
    glm::mat4 vpMat{ 1.0f };
    GLint uVP{ -1 };
};