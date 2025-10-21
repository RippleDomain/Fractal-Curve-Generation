#pragma once

#include <glm.hpp>
#include <vector>
#include "Types.h"

//Single vertex (position + color RGBA).
struct Vertex
{
    glm::vec2 pos;
    glm::vec4 color;
};

//CPU-side mesh buffers.
struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    void clear()
    {
        vertices.clear();
        indices.clear();
    }
};

//Left-handed 90 degree perpendicular.
inline glm::vec2 perp(const glm::vec2& v)
{
    return glm::vec2(-v.y, v.x);
}

//Add a thick quad for one segment.
inline void addThickSegment(Mesh& m, const glm::vec2& a, const glm::vec2& b, float halfPx, const Color& c)
{
    glm::vec2 d = b - a;
    float len = glm::length(d);
    if (len <= 1e-6f) return;

    glm::vec2 n = perp(d) / len; //Unit normal.
    glm::vec2 off = n * halfPx; //Half-thickness offset.
    uint32_t base = (uint32_t)m.vertices.size();

    glm::vec4 col{ c.r,c.g,c.b,c.a };
    m.vertices.push_back({ a - off, col });
    m.vertices.push_back({ a + off, col });
    m.vertices.push_back({ b + off, col });
    m.vertices.push_back({ b - off, col });

    //Two triangles.
    m.indices.push_back(base + 0); m.indices.push_back(base + 1); m.indices.push_back(base + 2);
    m.indices.push_back(base + 0); m.indices.push_back(base + 2); m.indices.push_back(base + 3);
}

//Add a small filled circle (n-gon) for endpoint handles.
inline void addDisc(Mesh& m, const glm::vec2& center, float radiusPx, int segments, const Color& c)
{
    if (segments < 8) segments = 8;

    uint32_t centerIdx = (uint32_t)m.vertices.size();
    glm::vec4 col{ c.r,c.g,c.b,c.a };
    m.vertices.push_back({ center, col });

    for (int i = 0; i <= segments; ++i)
    {
        float t = (float)i / segments * 6.28318530718f;
        glm::vec2 p = center + glm::vec2(std::cos(t), std::sin(t)) * radiusPx;
        m.vertices.push_back({ p, col });
    }

    for (int i = 1; i <= segments; ++i)
    {
        m.indices.push_back(centerIdx);
        m.indices.push_back(centerIdx + i);
        m.indices.push_back(centerIdx + i + 1);
    }
}