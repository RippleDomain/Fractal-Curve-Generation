#include "SaveSystem.h"
#include "../render/Transforms.h"
#include "../render/Renderer2D.h"
#include "Util.h"
#include <nlohmann/json.hpp>
#include <gtc/matrix_transform.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <glad/glad.h>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

bool saveStateJSON(const Document& doc, const std::string& path) 
{
    json j;
    j["version"] = 1;
    j["cam"] = { {"cx", doc.camCenter.x}, {"cy", doc.camCenter.y}, {"zoom", doc.camZoom} };

    auto& arr = j["lines"] = json::array();

    for (auto& l : doc.originals) 
    {
        json L;

        L["id"] = l.id;
        L["ax"] = l.a.x; L["ay"] = l.a.y;
        L["bx"] = l.b.x; L["by"] = l.b.y;
        L["color"] = { l.color.r, l.color.g, l.color.b, l.color.a };
        L["thickness"] = l.thicknessPx;
        L["koch2"] = l.koch2Iters;
        L["dragon"] = l.dragonIters;

        arr.push_back(L);
    }

    std::ofstream f(path, std::ios::binary);

    if (!f) return false;

    f << j.dump(2);

    return true;
}

bool loadStateJSON(Document& doc, const std::string& path) 
{
    std::ifstream f(path, std::ios::binary);

    if (!f) return false;

    json j; f >> j;
    doc.originals.clear();
    doc.nextId = 1;

    if (j.contains("cam")) 
    {
        auto c = j["cam"];
        doc.camCenter.x = c.value("cx", 0.f);
        doc.camCenter.y = c.value("cy", 0.f);
        doc.camZoom = c.value("zoom", 1.f);
    }

    for (auto& L : j["lines"]) 
    {
        Line l;

        l.id = L.value("id", doc.nextId++);
        doc.nextId = glm::max(doc.nextId, l.id + 1);
        l.a.x = L.value("ax", 0.f); l.a.y = L.value("ay", 0.f);
        l.b.x = L.value("bx", 0.f); l.b.y = L.value("by", 0.f);

        auto col = L["color"];

        l.color = { col[0], col[1], col[2], col[3] };
        l.thicknessPx = L.value("thickness", 3.f);
        l.koch2Iters = L.value("koch2", 0);
        l.dragonIters = L.value("dragon", 0);
        l.dirty = true;
        doc.originals.push_back(l);
    }

    return true;
}

// Replicates the apps viewProj but parameterized by width/height.
static glm::mat4 makeViewProjFor(const Document& doc, int w, int h) 
{
    float W = float(w), H = float(h);

    glm::mat4 proj = glm::ortho(0.f, W, 0.f, H, -1.f, 1.f);
    glm::mat4 tr = glm::translate(glm::mat4(1), glm::vec3(W * 0.5f, H * 0.5f, 0.f));
    glm::mat4 sc = glm::scale(glm::mat4(1), glm::vec3(doc.camZoom, doc.camZoom, 1.f));
    glm::mat4 tr2 = glm::translate(glm::mat4(1), glm::vec3(-doc.camCenter, 0.f));

    return proj * (tr * sc * tr2);
}

bool saveCanvasPNG(Renderer2D& renderer, const Document& doc, int outW, int outH, const std::string& filename) 
{
    if (outW <= 0 || outH <= 0) return false;

    // Save bindings/state we will touch.
    GLint prevFbo = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLint prevViewport[4]; glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Create FBO.
    GLuint fbo = 0, color = 0, rbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, outW, outH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, outW, outH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) 
    {
        // Restore and clean up.
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);

        if (rbo) glDeleteRenderbuffers(1, &rbo);
        if (color) glDeleteTextures(1, &color);
        if (fbo) glDeleteFramebuffers(1, &fbo);

        return false;
    }

	// Viewport to FBO size.
    glViewport(0, 0, outW, outH);

    // Clear and set 2D states.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.12f, 0.12f, 0.125f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw exactly like the canvas.
    const glm::mat4 VP = makeViewProjFor(doc, outW, outH);
    renderer.begin(VP);

    // Effects. If effect cache is empty, draw the base segment.
    for (const auto& l : doc.originals) 
    {
        const std::vector<glm::vec2>& poly = l.effect.empty()
            ? (const std::vector<glm::vec2>&)std::vector<glm::vec2>{ l.a, l.b }
        : l.effect;

        renderer.submitPolyline(poly, l.thicknessPx, l.color);
    }

    // Originals.
    for (const auto& l : doc.originals) {
        Color c = l.color; c.a *= 0.35f;
        renderer.submitSegment(l.a, l.b, l.thicknessPx, c);
    }

    // Selection handles.
    if (!doc.selection.empty()) 
    {
        const Color handle{ 1,1,0,1 };

        for (auto id : doc.selection) 
        {
            if (const auto* ln = findLine(doc, id)) 
            {
                renderer.submitDisc(ln->a, 8.0f, handle);
                renderer.submitDisc(ln->b, 8.0f, handle);
            }
        }

        // Cyan center hint if any selected line belongs to a regular polygon.
        const RegularPolyGroup* g = nullptr;

        for (auto id : doc.selection) 
        { 
            g = findRegPolyByLine(doc, id); if (g) break; 
        }

        if (g) 
        {
            renderer.submitDisc(g->center, 6.0f, Color{ 0.2f, 0.8f, 1.0f, 1.0f });
        }
    }

    renderer.end();

    // Read back pixels.
    std::vector<unsigned char> pixels(size_t(outW) * size_t(outH) * 4);
    glReadPixels(0, 0, outW, outH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Flip rows.
    const size_t stride = size_t(outW) * 4;
    std::vector<unsigned char> row(stride);

    for (int y = 0; y < outH / 2; ++y) 
    {
        unsigned char* top = pixels.data() + size_t(y) * stride;
        unsigned char* bot = pixels.data() + size_t(outH - 1 - y) * stride;

        std::memcpy(row.data(), top, stride);
        std::memcpy(top, bot, stride);
        std::memcpy(bot, row.data(), stride);
    }

    // Write PNG.
    const int ok = stbi_write_png(filename.c_str(), outW, outH, 4, pixels.data(), int(stride));

    // Restore state.
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    if (rbo) glDeleteRenderbuffers(1, &rbo);
    if (color) glDeleteTextures(1, &color);
    if (fbo) glDeleteFramebuffers(1, &fbo);

    return ok != 0;
}