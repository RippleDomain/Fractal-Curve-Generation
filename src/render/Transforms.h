#pragma once

#include <glm.hpp>
#include <vector>
#include <cmath>

//90 degree helpers.
inline glm::vec2 rot90L(const glm::vec2& v)
{
    return { -v.y, v.x };
}

inline glm::vec2 rot90R(const glm::vec2& v)
{
    return { v.y, -v.x };
}

//Quadratic type-2 Koch.
inline std::vector<glm::vec2> applyKoch2Once(const std::vector<glm::vec2>& in)
{
    if (in.size() < 2) return in;

    static const int U[9] = { 0,1,1,2,2,2,3,3,4 };
    static const int V[9] = { 0,0,1,1,0,-1,-1,0,0 };

    auto rot90L = [](const glm::vec2& v) { return glm::vec2(-v.y, v.x); };

    std::vector<glm::vec2> out;
    out.reserve(in.size() * 9);
    out.push_back(in.front());

    for (size_t i = 0; i + 1 < in.size(); ++i)
    {
        const glm::vec2 p = in[i];
        const glm::vec2 q = in[i + 1];

        const glm::vec2 d = q - p;
        const float L = glm::length(d);

        if (L <= 0.0f)
        {
            out.push_back(q);
            continue;
        }

        const glm::vec2 f = d / L; //Forward.
        const glm::vec2 n = rot90L(f); //Left-normal.
        const float s = L * 0.25f; //Quarter step.

        //Emit interior anchors (1..7), then snap last to q.
        for (int k = 1; k <= 7; ++k)
        {
            out.push_back(p + f * (U[k] * s) + n * (V[k] * s));
        }

        out.push_back(q);
    }

    return out;
}

//Heighway dragon.
inline std::vector<glm::vec2> applyDragonOnce(const std::vector<glm::vec2>& in)
{
    if (in.size() < 2) return in;

    std::vector<glm::vec2> out;
    out.reserve(in.size() * 2 + 1);
    out.push_back(in.front());

    bool left = false;

    for (size_t i = 0; i + 1 < in.size(); ++i)
    {
        const glm::vec2 a = in[i];
        const glm::vec2 b = in[i + 1];
        const glm::vec2 m = 0.5f * (a + b);
        const glm::vec2 d = 0.5f * (b - a);
        const glm::vec2 k = left ? (m + rot90L(d)) : (m + rot90R(d));
        out.push_back(k);
        out.push_back(b);
        left = !left;
    }

    return out;
}

//Iterate with a segment budget.
inline std::vector<glm::vec2> iterateTransform(
    const std::vector<glm::vec2>& base,
    int koch2Iters,
    int dragonIters,
    size_t maxSegments = 200000)
{
    std::vector<glm::vec2> cur = base;

    for (int k = 0; k < koch2Iters; ++k)
    {
        cur = applyKoch2Once(cur);
        if (cur.size() > maxSegments) break;
    }

    for (int d = 0; d < dragonIters; ++d)
    {
        cur = applyDragonOnce(cur);
        if (cur.size() > maxSegments) break;
    }

    return cur;
}