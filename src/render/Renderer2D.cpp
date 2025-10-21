#include "Renderer2D.h"
#include <gtc/type_ptr.hpp>
#include <iostream>

bool Renderer2D::init() 
{
    if (!program.loadFromFiles("basic2d.vert", "basic2d.frag")) 
    {
        std::cerr << "Renderer2D failed to load shaders from disk.\n";

        return false;
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    //Vec2 pos, vec4 color.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, color));

    uVP = glGetUniformLocation(program.id(), "uVP");

    return true;
}

void Renderer2D::shutdown() 
{
    program.destroy();

    if (ebo) glDeleteBuffers(1, &ebo), ebo = 0;
    if (vbo) glDeleteBuffers(1, &vbo), vbo = 0;
    if (vao) glDeleteVertexArrays(1, &vao), vao = 0;
}

void Renderer2D::begin(const glm::mat4& vp) 
{
    vpMat = vp;
    mesh.clear();
}

void Renderer2D::submitSegment(const glm::vec2& a, const glm::vec2& b, float thicknessPx, const Color& c) 
{
    addThickSegment(mesh, a, b, thicknessPx * 0.5f, c);
}

void Renderer2D::submitPolyline(const std::vector<glm::vec2>& pts, float thicknessPx, const Color& c) 
{
    if (pts.size() < 2) return;

    for (size_t i = 0; i + 1 < pts.size(); ++i) 
    {
        submitSegment(pts[i], pts[i + 1], thicknessPx, c);
    }
}

void Renderer2D::submitDisc(const glm::vec2& center, float radiusPx, const Color& c, int segs)
{
    addDisc(mesh, center, radiusPx, segs, c);
}

void Renderer2D::end() 
{
    program.use();
    glUniformMatrix4fv(uVP, 1, GL_FALSE, glm::value_ptr(vpMat));

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_DYNAMIC_DRAW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);
}

void Renderer2D::flush() {}