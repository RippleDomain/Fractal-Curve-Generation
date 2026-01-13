#pragma once

#define GLFW_INCLUDE_NONE
#include <string>
#include <glm.hpp>
#include <GLFW/glfw3.h>
#include "../render/Renderer2D.h"
#include "../render/Model.h"
#include "../util/Commands.h"

class App
{
public:
    bool init(int w, int h, const std::string& title);
    void run();
    void shutdown();

private:
    // Window/renderer/document.
    GLFWwindow* win{ nullptr };
    int fbW{ 1280 }, fbH{ 720 };
    Renderer2D renderer;
    Document doc;
    History history;

    // Creation state.
    bool creating{ false };
    bool createHasDrag{ false };
    glm::vec2 createStart{};
    glm::vec2 createCurrent{};
    int regularSides{ 6 };
    float regularRotation{ 0 };

    // Chained polygon state (Tool::Poly).
    bool polyActive{ false };
    glm::vec2 polyFirst{}, polyLast{};
    std::vector<Id> polyLineIds;

    // Snap visualization for poly close.
    bool snapActive{ false };
    glm::vec2 snapPoint{};

    // Interaction.
    Tool tool{ Tool::Select };
    bool drawing{ false };
    glm::vec2 dragStart{};
    Id hoveredId{ 0 };
    enum class Grab { None, EndA, EndB, Middle, Center };
    Grab grabKind{ Grab::None };
    Id dragGroupId{ 0 };
    glm::vec2 groupCenterStart{};
    glm::vec2 grabOffset{};
    float endpointHandlePx{ 8.f };

    bool lmbWasDown{ false };
    bool isDragging{ false };
    Id dragId{ 0 };
    Grab dragGrab{ Grab::None };
    std::vector<Id> dragIds;
    std::vector<glm::vec2> dragAStart, dragBStart;
    glm::vec2 aStart{}, bStart{}; // Endpoints at mouse press.
    glm::vec2 pressWorld{}; // World position at mouse press.
    glm::vec2 midOffsetWorld{}; // For middle drags if you want an offset.
    float dragEpsilon{ 0.001f }; // World units; pixels at zoom = 1.

    // Style UI cache.
    Color uiColor{ 1,1,1,1 };
    float uiThickness{ 3.f };
    int uiKoch{ 0 };
    int uiDragon{ 0 };

    // Export.
    std::string exportBase{ "canvas" };
    std::string exportDir;

    // Helpers.
    static void framebufferSizeCallback(GLFWwindow* w, int W, int H);
    glm::mat4 viewProj() const;
    glm::vec2 screenToWorld(double sx, double sy) const;
    glm::vec2 worldToScreen(const glm::vec2& p) const;
    void updateEffect(Line& l);
    void rebuildEffectsIfDirty();

    // Input.
    void handleInput();
    void drawUI();
    void drawScene();
    void pickHover(double mx, double my);
};