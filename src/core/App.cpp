#include "App.h"
#include "../render/Transforms.h"
#include "../util/SaveSystem.h"
#include "../util/Util.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include <gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

static const char* GLSL_VER = "#version 330";

//Framebuffer/camera.
void App::framebufferSizeCallback(GLFWwindow*, int W, int H)
{
    glViewport(0, 0, W, H);
}

glm::mat4 App::viewProj() const
{
    float W = (float)fbW, H = (float)fbH;
    glm::mat4 proj = glm::ortho(0.f, W, 0.f, H, -1.f, 1.f);
    glm::mat4 tr = glm::translate(glm::mat4(1.f), glm::vec3(W * 0.5f, H * 0.5f, 0.f));
    glm::mat4 sc = glm::scale(glm::mat4(1.f), glm::vec3(doc.camZoom, doc.camZoom, 1.f));
    glm::mat4 tr2 = glm::translate(glm::mat4(1.f), glm::vec3(-doc.camCenter, 0.f));
    return proj * (tr * sc * tr2);
}

glm::vec2 App::worldToScreen(const glm::vec2& p) const
{
    float W = (float)fbW, H = (float)fbH;
    return (p - doc.camCenter) * doc.camZoom + glm::vec2(W * 0.5f, H * 0.5f);
}

glm::vec2 App::screenToWorld(double sx, double sy) const
{
    float W = (float)fbW, H = (float)fbH;
    glm::vec2 s((float)sx, (float)(H - sy));
    return (s - glm::vec2(W * 0.5f, H * 0.5f)) / doc.camZoom + doc.camCenter;
}

//Lifecycle.
bool App::init(int w, int h, const std::string& title)
{
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    win = glfwCreateWindow(w, h, title.c_str(), nullptr, nullptr);
    if (!win) { glfwTerminate(); return false; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to load GL\n";
        return false;
    }

    glfwGetFramebufferSize(win, &fbW, &fbH);
    framebufferSizeCallback(win, fbW, fbH);
    glfwSetFramebufferSizeCallback(win,
        [](GLFWwindow* w, int W, int H)
        {
            auto* app = (App*)glfwGetWindowUserPointer(w);
            app->fbW = W; app->fbH = H;
            framebufferSizeCallback(w, W, H);
        });
    glfwSetWindowUserPointer(win, this);

    if (!renderer.init()) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init(GLSL_VER);

    exportDir = ensureOutputDir().string();
    return true;
}

void App::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    renderer.shutdown();
    if (win) { glfwDestroyWindow(win); win = nullptr; }
    glfwTerminate();
}

//Effect cache.
void App::updateEffect(Line& l)
{
    std::vector<glm::vec2> base{ l.a, l.b };
    l.effect = iterateTransform(base, l.koch2Iters, l.dragonIters);
    l.dirty = false;
}

void App::rebuildEffectsIfDirty()
{
    for (auto& l : doc.originals) if (l.dirty) updateEffect(l);
}

//Picking.
void App::pickHover(double mx, double my)
{
    hoveredId = 0;
    glm::vec2 m = screenToWorld(mx, my);
    float tol = 8.f / doc.camZoom;

    auto distPoint = [](const glm::vec2& p, const glm::vec2& q)
        {
            return glm::length(p - q);
        };
    auto distSeg = [](const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
        {
            glm::vec2 ab = b - a;
            float t = glm::dot(p - a, ab) / glm::dot(ab, ab);
            t = glm::clamp(t, 0.f, 1.f);
            return glm::length((a + t * ab) - p);
        };

    for (auto& l : doc.originals)
    {
        if (distPoint(m, l.a) <= tol || distPoint(m, l.b) <= tol || distSeg(m, l.a, l.b) <= tol)
        {
            hoveredId = l.id;
            break;
        }
    }
}

//Group helpers.
static void rebuildRegularPolyLines(Document& doc, RegularPolyGroup& g)
{
    int N = std::max(3, g.sides);
    float base = glm::radians(g.rotationDeg);
    for (int i = 0; i < N && i < (int)g.lineIds.size(); ++i)
    {
        float t0 = base + (i) * 6.2831853f / N;
        float t1 = base + (i + 1) * 6.2831853f / N;
        glm::vec2 p0 = g.center + g.radius * glm::vec2(std::cos(t0), std::sin(t0));
        glm::vec2 p1 = g.center + g.radius * glm::vec2(std::cos(t1), std::sin(t1));
        if (auto* l = findLine(doc, g.lineIds[i]))
        {
            l->a = p0; l->b = p1; l->dirty = true;
        }
    }
}

static void toggleSelectMany(Document& doc, const std::vector<Id>& ids)
{
    for (auto id : ids) toggleSelection(doc, id);
}

static void setSelectionMany(Document& doc, const std::vector<Id>& ids)
{
    doc.selection = ids;
    std::sort(doc.selection.begin(), doc.selection.end());
    doc.selection.erase(std::unique(doc.selection.begin(), doc.selection.end()), doc.selection.end());
}

static RegularPolyGroup* hitNearestRegCenter(Document& doc, const glm::vec2& world, float tolWorld)
{
    RegularPolyGroup* best = nullptr;
    const float tol2 = tolWorld * tolWorld;
    float bestD2 = tol2;

    for (auto& g : doc.regPolys)
    {
        glm::vec2 dv = world - g.center;
        float d2 = glm::dot(dv, dv);
        if (d2 <= tol2 && d2 <= bestD2)
        {
            bestD2 = d2;
            best = &g;
        }
    }
    return best;
}

//Input/interaction.
void App::handleInput()
{
    ImGuiIO& io = ImGui::GetIO();

    auto isCtrlDown = [&]()
        {
            if (io.KeyCtrl) return true;
            return glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        };

    double mx, my; glfwGetCursorPos(win, &mx, &my);
    glm::vec2 world = screenToWorld(mx, my);

    bool lmbNow = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool justPressed = (!lmbWasDown && lmbNow);
    bool justReleased = (lmbWasDown && !lmbNow);

    if (io.WantCaptureMouse)
    {
        isDragging = false;
        creating = false;
        createHasDrag = false;
        polyActive = false;
        snapActive = false;
        dragGrab = Grab::None;
        dragId = 0;
        dragGroupId = 0;
        dragIds.clear(); dragAStart.clear(); dragBStart.clear();
        polyLineIds.clear();
        lmbWasDown = lmbNow;
        return;
    }

    pickHover(mx, my);

    if (io.MouseWheel != 0.f)
    {
        glm::vec2 before = screenToWorld(mx, my);
        float z = doc.camZoom * (io.MouseWheel > 0 ? 1.1f : 0.9f);
        doc.camZoom = glm::clamp(z, 0.1f, 10.f);
        glm::vec2 after = screenToWorld(mx, my);
        doc.camCenter += (before - after);
        if (!std::isfinite(doc.camZoom)) doc.camZoom = 1.f;
    }

    static bool panning = false;
    static glm::vec2 lastScreen{};
    if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
    {
        glm::vec2 cur((float)mx, (float)my);
        if (!panning) { panning = true; lastScreen = cur; }
        else
        {
            glm::vec2 delta = cur - lastScreen; lastScreen = cur;
            doc.camCenter -= delta / doc.camZoom;
        }
    }
    else panning = false;

    //Press.
    if (justPressed)
    {
        pressWorld = world;

        if (tool == Tool::Select)
        {
            //Center hit (regular polygon).
            {
                float tolPx = 6.f, tolWorld = tolPx / doc.camZoom;
                if (RegularPolyGroup* gHit = hitNearestRegCenter(doc, world, tolWorld))
                {
                    setSelectionMany(doc, gHit->lineIds);
                    dragGrab = Grab::Center;
                    isDragging = true;
                    dragGroupId = gHit->id;
                    groupCenterStart = gHit->center;
                    goto after_press_center_check;
                }
            }

            //Center hit on already-selected group.
            {
                RegularPolyGroup* g = nullptr;
                for (auto id : doc.selection) { g = findRegPolyByLine(doc, id); if (g) break; }
                if (g)
                {
                    float tolPx = 6.f, tolWorld = tolPx / doc.camZoom;
                    if (glm::length(world - g->center) <= tolWorld)
                    {
                        dragGrab = Grab::Center;
                        isDragging = true;
                        dragGroupId = g->id;
                        groupCenterStart = g->center;
                        goto after_press_center_check;
                    }
                }
            }
        after_press_center_check:;

            if (hoveredId && !isDragging)
            {
                bool ctrl = isCtrlDown();

                auto* reg = findRegPolyByLine(doc, hoveredId);
                auto* arb = findArbPolyByLine(doc, hoveredId);

                if (reg)
                {
                    if (!ctrl) setSingleSelection(doc, hoveredId);
                    else toggleSelection(doc, hoveredId);
                }
                else if (arb)
                {
                    if (!ctrl) setSingleSelection(doc, hoveredId);
                    else toggleSelection(doc, hoveredId);
                }
                else
                {
                    if (!ctrl) setSingleSelection(doc, hoveredId);
                    else toggleSelection(doc, hoveredId);
                }

                if (auto* l = findLine(doc, hoveredId))
                {
                    float tol = 8.f / doc.camZoom;
                    if (glm::length(world - l->a) <= tol)
                    {
                        dragGrab = Grab::EndA; isDragging = true; dragId = hoveredId; aStart = l->a; bStart = l->b;
                    }
                    else if (glm::length(world - l->b) <= tol)
                    {
                        dragGrab = Grab::EndB; isDragging = true; dragId = hoveredId; aStart = l->a; bStart = l->b;
                    }
                    else
                    {
                        dragGrab = Grab::Middle; isDragging = true; dragId = hoveredId;
                        dragIds = doc.selection;
                        dragAStart.clear(); dragBStart.clear();
                        dragAStart.reserve(dragIds.size());
                        dragBStart.reserve(dragIds.size());
                        for (auto id : dragIds)
                        {
                            if (auto* li = findLine(doc, id)) { dragAStart.push_back(li->a); dragBStart.push_back(li->b); }
                            else { dragAStart.push_back(glm::vec2(0)); dragBStart.push_back(glm::vec2(0)); }
                        }
                        if (l) { aStart = l->a; bStart = l->b; }
                    }
                }
            }
            else if (!isDragging)
            {
                if (!isCtrlDown()) clearSelection(doc);
                isDragging = false; dragGrab = Grab::None; dragId = 0; dragGroupId = 0;
                dragIds.clear(); dragAStart.clear(); dragBStart.clear();
            }
        }
        else if (tool == Tool::Line)
        {
            creating = true; createStart = world; createCurrent = world; createHasDrag = false;
        }
        else if (tool == Tool::Poly)
        {
            if (!polyActive)
            {
                polyActive = true;
                creating = true;
                polyLineIds.clear();
                polyFirst = world;
                polyLast = world;
                createStart = polyLast;
                createCurrent = world;
                createHasDrag = false;
                snapActive = false;
            }
            else
            {
                creating = true;
                createStart = polyLast;
                createCurrent = world;
                createHasDrag = false;
                snapActive = false;
            }
        }
        else if (tool == Tool::RegularPoly)
        {
            creating = true; createStart = world; createCurrent = world; createHasDrag = false;
        }
    }

    //Move.
    if (lmbNow)
    {
        if (tool == Tool::Select && isDragging)
        {
            if (dragGrab == Grab::Center && dragGroupId)
            {
                if (auto* g = findRegPoly(doc, dragGroupId))
                {
                    glm::vec2 delta = world - pressWorld;
                    g->center = groupCenterStart + delta;
                    rebuildRegularPolyLines(doc, *g);
                }
            }
            else if (dragGrab == Grab::Middle)
            {
                glm::vec2 delta = world - pressWorld;
                for (size_t i = 0; i < dragIds.size(); ++i)
                {
                    if (auto* l = findLine(doc, dragIds[i]))
                    {
                        l->a = dragAStart[i] + delta;
                        l->b = dragBStart[i] + delta;
                        l->dirty = true;
                    }
                }
            }
            else if (dragId)
            {
                if (auto* l = findLine(doc, dragId))
                {
                    if (dragGrab == Grab::EndA) { l->a = world; l->dirty = true; }
                    else if (dragGrab == Grab::EndB) { l->b = world; l->dirty = true; }
                }
            }
        }

        if (creating && (tool == Tool::Line || tool == Tool::Poly || tool == Tool::RegularPoly))
        {
            glm::vec2 cur = world;

            if (tool == Tool::Poly && polyActive)
            {
                const float snapPx = 10.f;
                float tolWorld = snapPx / doc.camZoom;
                if (glm::length(world - polyFirst) <= tolWorld && (polyLineIds.size() >= 1))
                {
                    cur = polyFirst;
                    snapActive = true;
                    snapPoint = polyFirst;
                }
                else
                {
                    snapActive = false;
                }
            }

            createCurrent = cur;
            if (!createHasDrag && glm::length(createCurrent - createStart) > 0.25f) createHasDrag = true;
        }
    }

    //Release.
    if (justReleased)
    {
        if (tool == Tool::Select && isDragging)
        {
            if (dragGrab == Grab::Center && dragGroupId)
            {
                if (auto* g = findRegPoly(doc, dragGroupId))
                {
                    glm::vec2 newCenter = g->center;
                    bool changed = glm::length(newCenter - groupCenterStart) > dragEpsilon;
                    if (changed)
                    {
                        history.push(std::make_unique<CmdRegularPolyParams>(
                            g->id, groupCenterStart, g->radius, g->rotationDeg,
                            newCenter, g->radius, g->rotationDeg), doc);
                    }
                    else
                    {
                        g->center = groupCenterStart;
                        rebuildRegularPolyLines(doc, *g);
                    }
                }
                isDragging = false; dragGrab = Grab::None; dragGroupId = 0;
                dragIds.clear(); dragAStart.clear(); dragBStart.clear();
            }
            else if (dragGrab == Grab::Middle && !dragIds.empty())
            {
                glm::vec2 delta = world - pressWorld;
                bool changed = glm::length(delta) > dragEpsilon;
                if (changed)
                {
                    std::vector<glm::vec2> a1, b1; a1.reserve(dragIds.size()); b1.reserve(dragIds.size());
                    for (size_t i = 0; i < dragIds.size(); ++i)
                    {
                        a1.push_back(dragAStart[i] + delta);
                        b1.push_back(dragBStart[i] + delta);
                    }
                    history.push(std::make_unique<CmdEditManyEndpoints>(dragIds, dragAStart, dragBStart, a1, b1), doc);
                }
                else
                {
                    for (size_t i = 0; i < dragIds.size(); ++i)
                    {
                        if (auto* l = findLine(doc, dragIds[i]))
                        {
                            l->a = dragAStart[i];
                            l->b = dragBStart[i];
                            l->dirty = true;
                        }
                    }
                }
                isDragging = false; dragGrab = Grab::None; dragId = 0;
                dragIds.clear(); dragAStart.clear(); dragBStart.clear();
            }
            else if (dragId)
            {
                if (auto* l = findLine(doc, dragId))
                {
                    bool changed = (glm::length(l->a - aStart) > dragEpsilon) || (glm::length(l->b - bStart) > dragEpsilon);
                    if (changed)
                    {
                        history.push(std::make_unique<CmdEditEndpoints>(l->id, aStart, bStart, l->a, l->b), doc);
                    }
                    else
                    {
                        l->a = aStart; l->b = bStart; l->dirty = true;
                    }
                }
                isDragging = false; dragGrab = Grab::None; dragId = 0;
            }
        }

        //Finish creation.
        if (creating)
        {
            if (tool == Tool::Line)
            {
                glm::vec2 end = createCurrent;
                if (glm::length(end - createStart) > 0.5f)
                {
                    Line l; l.id = doc.nextId++; l.a = createStart; l.b = end; l.color = uiColor; l.thicknessPx = uiThickness; l.dirty = true;
                    history.push(std::make_unique<CmdCreateLine>(l), doc);
                    setSingleSelection(doc, l.id);
                }
                creating = false; createHasDrag = false;
            }
            else if (tool == Tool::Poly)
            {
                glm::vec2 end = createCurrent;
                bool longEnough = glm::length(end - createStart) > 0.5f;

                if (longEnough)
                {
                    Line l;
                    l.id = doc.nextId++;
                    l.a = createStart; l.b = end;
                    l.color = uiColor; l.thicknessPx = uiThickness; l.dirty = true;
                    history.push(std::make_unique<CmdCreateLine>(l), doc);
                    polyLineIds.push_back(l.id);
                    setSingleSelection(doc, l.id);
                    polyLast = end;
                }

                bool closedNow = snapActive && (glm::length(end - polyFirst) <= 1e-4f) && (polyLineIds.size() >= 2);
                if (closedNow)
                {
                    ArbitraryPolyGroup g;
                    g.id = doc.nextGroupId++;
                    g.lineIds = polyLineIds;
                    history.push(std::make_unique<CmdCreateArbPolyGroup>(g), doc);

                    polyActive = false;
                    creating = false;
                    createHasDrag = false;
                    snapActive = false;
                    polyLineIds.clear();
                }
                else
                {
                    creating = false;
                    createHasDrag = false;
                    snapActive = false;
                }
            }
            else if (tool == Tool::RegularPoly)
            {
                glm::vec2 center = createStart;
                float r = glm::length(createCurrent - center);

                if (r > 0.5f)
                {
                    int N = glm::clamp(regularSides, 3, 20);
                    float base = glm::radians(regularRotation);

                    std::vector<Line> newLines;
                    newLines.reserve(N);

                    std::vector<Id> createdIds;
                    createdIds.reserve(N);

                    for (int i = 0; i < N; ++i)
                    {
                        float t0 = base + (i) * 6.2831853f / N;
                        float t1 = base + (i + 1) * 6.2831853f / N;

                        glm::vec2 p0 = center + r * glm::vec2(std::cos(t0), std::sin(t0));
                        glm::vec2 p1 = center + r * glm::vec2(std::cos(t1), std::sin(t1));

                        Line l;
                        l.id = doc.nextId++;
                        l.a = p0; l.b = p1;
                        l.color = uiColor;
                        l.thicknessPx = uiThickness;
                        l.koch2Iters = uiKoch;
                        l.dragonIters = uiDragon;
                        l.dirty = true;

                        newLines.push_back(l);
                        createdIds.push_back(l.id);
                    }

                    RegularPolyGroup g;
                    g.id = doc.nextGroupId++;
                    g.lineIds = createdIds;
                    g.center = center;
                    g.radius = r;
                    g.sides = N;
                    g.rotationDeg = regularRotation;

                    for (auto& l : newLines) l.groupId = g.id;

                    history.push(std::make_unique<CmdCreateRegularPolygon>(std::move(newLines), g), doc);
                }

                creating = false;
                createHasDrag = false;
            }
        }
    }

    lmbWasDown = lmbNow;
}

//Rendering.
void App::drawScene()
{
    rebuildEffectsIfDirty();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto VP = viewProj();
    renderer.begin(VP);

    for (auto& l : doc.originals)
    {
        renderer.submitPolyline(l.effect.empty() ? std::vector<glm::vec2>{ l.a, l.b } : l.effect, l.thicknessPx, l.color);
    }

    for (auto& l : doc.originals)
    {
        Color c = l.color; c.a *= 0.35f;
        renderer.submitSegment(l.a, l.b, l.thicknessPx, c);
    }

    if (!doc.selection.empty())
    {
        Color k{ 1,1,0,1 };
        for (auto id : doc.selection)
        {
            if (auto* l = findLine(doc, id))
            {
                renderer.submitDisc(l->a, endpointHandlePx, k);
                renderer.submitDisc(l->b, endpointHandlePx, k);
            }
        }

        if (const RegularPolyGroup* g = [&]() -> const RegularPolyGroup*
            {
                for (auto id : doc.selection) { auto* gp = findRegPolyByLine(doc, id); if (gp) return gp; }
                return nullptr;
            }())
        {
            renderer.submitDisc(g->center, 6.f, Color{ 0.2f, 0.8f, 1.f, 1.f });
        }
    }

    if (creating && createHasDrag)
    {
        Color preview = uiColor; preview.a *= 0.65f;

        if (tool == Tool::Line)
        {
            renderer.submitSegment(createStart, createCurrent, uiThickness, preview);
        }
        else if (tool == Tool::Poly)
        {
            renderer.submitSegment(createStart, createCurrent, uiThickness, preview);
            if (snapActive) renderer.submitDisc(snapPoint, 6.f, Color{ 0.2f, 0.8f, 1.f, 1.f });
        }
        else if (tool == Tool::RegularPoly)
        {
            glm::vec2 center = createStart;
            float r = glm::length(createCurrent - center);
            if (r > 0.1f)
            {
                int N = glm::clamp(regularSides, 3, 20);
                float base = glm::radians(regularRotation);
                for (int i = 0; i < N; ++i)
                {
                    float t0 = base + (i) * 6.2831853f / N;
                    float t1 = base + (i + 1) * 6.2831853f / N;
                    glm::vec2 p0 = center + r * glm::vec2(std::cos(t0), std::sin(t0));
                    glm::vec2 p1 = center + r * glm::vec2(std::cos(t1), std::sin(t1));
                    renderer.submitSegment(p0, p1, uiThickness, preview);
                }
            }
        }
    }

    renderer.end();
}

//UI.
void App::drawUI()
{
    ImGui::Begin("Fractal Editor");

    if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_Reorderable))
    {
        //Select/Move.
        if (ImGui::BeginTabItem("Select/Move"))
        {
            tool = Tool::Select;

            ImGui::Text("Selection");
            ImGui::Separator();
            ImGui::BulletText("Click to select. Ctrl+Click adds/removes.");
            ImGui::BulletText("Drag endpoints to edit; drag middle to move selection.");
            ImGui::BulletText("Regular: drag the cyan center to move.");

            ImGui::Separator();
            ImGui::Text("Undo/Redo");
            if (ImGui::Button("Undo")) history.undo(doc);
            ImGui::SameLine();
            if (ImGui::Button("Redo")) history.redo(doc);

            ImGui::Separator();
            ImGui::Text("Style");
            ImGui::SliderFloat("Thickness##quick", &uiThickness, 1.f, 20.f, "%.1f px");
            ImGui::ColorEdit4("Color##quick", &uiColor.r);

            if (!doc.selection.empty())
            {
                if (ImGui::Button("Apply to selected"))
                {
                    history.push(std::make_unique<CmdStyleMany>(doc.selection, uiColor, uiThickness, doc), doc);
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete selected"))
                {
                    history.push(std::make_unique<CmdDeleteMany>(doc.selection), doc);
                    clearSelection(doc);
                }
                ImGui::SameLine(); ImGui::TextDisabled("(%zu)", doc.selection.size());
            }
            else
            {
                ImGui::TextDisabled("Nothing selected.");
            }

            if (!doc.selection.empty())
            {
                RegularPolyGroup* g = nullptr;
                for (auto id : doc.selection) { g = findRegPolyByLine(doc, id); if (g) break; }
                if (g)
                {
                    ImGui::Separator();
                    ImGui::Text("Regular polygon");

                    static glm::vec2 uiCenter{ 0,0 };
                    static float uiRadius = 0.f;
                    static float uiRot = 0.f;
                    static Id uiGroupCached{ 0 };

                    if (uiGroupCached != g->id)
                    {
                        uiGroupCached = g->id;
                        uiCenter = g->center;
                        uiRadius = g->radius;
                        uiRot = g->rotationDeg;
                    }

                    ImGui::DragFloat2("Center", &uiCenter.x, 1.f);
                    ImGui::DragFloat("Radius", &uiRadius, 1.f, 0.f, 100000.f, "%.1f");
                    ImGui::DragFloat("Rotation", &uiRot, 1.f, -360.f, 360.f, "%.0f");
                    ImGui::TextDisabled("Sides: %d   Edges: %zu", g->sides, g->lineIds.size());

                    if (ImGui::Button("Apply"))
                    {
                        float newR = std::max(0.1f, uiRadius);
                        history.push(std::make_unique<CmdRegularPolyParams>(
                            g->id, g->center, g->radius, g->rotationDeg,
                            uiCenter, newR, uiRot), doc);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset"))
                    {
                        uiCenter = g->center; uiRadius = g->radius; uiRot = g->rotationDeg;
                    }
                }
            }

            ImGui::EndTabItem();
        }

        //Create.
        if (ImGui::BeginTabItem("Create"))
        {
            if (ImGui::BeginTabBar("CreateTabs"))
            {
                if (ImGui::BeginTabItem("Line"))
                {
                    tool = Tool::Line;
                    ImGui::Text("Line");
                    ImGui::Separator();
                    ImGui::Text("Click-drag-release to place.");
                    ImGui::Separator();
                    ImGui::Text("Style");
                    ImGui::SliderFloat("Thickness##line", &uiThickness, 1.f, 20.f, "%.1f px");
                    ImGui::ColorEdit4("Color##line", &uiColor.r);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Poly"))
                {
                    tool = Tool::Poly;
                    ImGui::Text("Polygon");
                    ImGui::Separator();
                    ImGui::BulletText("Click-drag edges from last point.");
                    ImGui::BulletText("Snap to the first point to close.");
                    ImGui::Separator();
                    ImGui::Text("Style");
                    ImGui::SliderFloat("Thickness##poly", &uiThickness, 1.f, 20.f, "%.1f px");
                    ImGui::ColorEdit4("Color##poly", &uiColor.r);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Regular Poly"))
                {
                    tool = Tool::RegularPoly;
                    ImGui::Text("Regular polygon");
                    ImGui::Separator();
                    ImGui::Text("Click to set center, drag for radius.");
                    ImGui::Separator();
                    ImGui::Text("Parameters");
                    ImGui::SliderInt("Sides", &regularSides, 3, 20);
                    ImGui::SliderFloat("Rotation", &regularRotation, -180.f, 180.f, "%.0f");
                    ImGui::SameLine();
                    if (ImGui::Button("Triangle")) regularSides = 3;
                    ImGui::SameLine();
                    if (ImGui::Button("Square")) regularSides = 4;
                    ImGui::SameLine();
                    if (ImGui::Button("Hex")) regularSides = 6;
                    ImGui::Separator();
                    ImGui::Text("Style");
                    ImGui::SliderFloat("Thickness##reg", &uiThickness, 1.f, 20.f, "%.1f px");
                    ImGui::ColorEdit4("Color##reg", &uiColor.r);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }

        //Style.
        if (ImGui::BeginTabItem("Style"))
        {
            ImGui::Text("Style");
            ImGui::Separator();
            ImGui::SliderFloat("Thickness##global", &uiThickness, 1.f, 20.f, "%.1f px");
            ImGui::ColorEdit4("Color##global", &uiColor.r);

            if (!doc.selection.empty())
            {
                if (ImGui::Button("Apply to selected"))
                {
                    history.push(std::make_unique<CmdStyleMany>(doc.selection, uiColor, uiThickness, doc), doc);
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete selected"))
                {
                    history.push(std::make_unique<CmdDeleteMany>(doc.selection), doc);
                    clearSelection(doc);
                }
                ImGui::SameLine(); ImGui::TextDisabled("(%zu)", doc.selection.size());
            }
            else
            {
                ImGui::TextDisabled("Nothing selected.");
            }

            ImGui::EndTabItem();
        }

        //Transforms.
        if (ImGui::BeginTabItem("Transforms"))
        {
            ImGui::Text("Transforms");
            ImGui::Separator();
            ImGui::SliderInt("Koch Type-2", &uiKoch, 0, 6);
            ImGui::SliderInt("Dragon", &uiDragon, 0, 18);
            if (!doc.selection.empty())
            {
                if (ImGui::Button("Apply"))
                {
                    history.push(std::make_unique<CmdTransformsMany>(doc.selection, uiKoch, uiDragon, doc), doc);
                }
                ImGui::SameLine(); ImGui::TextDisabled("(%zu)", doc.selection.size());
            }
            else
            {
                ImGui::TextDisabled("Nothing selected.");
            }
            ImGui::EndTabItem();
        }

        //Canvas.
        if (ImGui::BeginTabItem("Canvas"))
        {
            ImGui::Text("Canvas");
            ImGui::Separator();
            ImGui::SliderFloat("Zoom", &doc.camZoom, 0.1f, 10.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Text("Center: (%.1f, %.1f)", doc.camCenter.x, doc.camCenter.y);
            ImGui::Separator();
            ImGui::Text("Undo/Redo");
            if (ImGui::Button("Undo##canvas")) history.undo(doc);
            ImGui::SameLine();
            if (ImGui::Button("Redo##canvas")) history.redo(doc);
            ImGui::EndTabItem();
        }

        //Export.
        if (ImGui::BeginTabItem("Export"))
        {
            static int outW = 1920, outH = 1080;
            static char base[128];
            static bool initBase = false;

            if (!initBase) { snprintf(base, sizeof(base), "%s", exportBase.c_str()); initBase = true; }
            if (ImGui::InputInt("Width", &outW)) {}
            if (ImGui::InputInt("Height", &outH)) {}
            if (ImGui::InputText("Base", base, IM_ARRAYSIZE(base))) { exportBase = base; }

            const auto imgDir = ensureOutputDir("output/images");
            const auto saveDir = ensureOutputDir("output/saves");
            const std::filesystem::path pngPath = imgDir / (exportBase + ".png");
            const std::filesystem::path jsonPath = saveDir / (exportBase + ".json");

            if (ImGui::Button("Save PNG"))
            {
                if (!saveCanvasPNG(renderer, doc, outW, outH, pngPath.string()))
                    std::cerr << "PNG save failed: " << pngPath.string() << "\n";
                else
                    std::cout << "Saved: " << pngPath.string() << "\n";
            }

            ImGui::SameLine();

            if (ImGui::Button("Save Canvas"))
            {
                if (!saveStateJSON(doc, jsonPath.string()))
                    std::cerr << "State save failed: " << jsonPath.string() << "\n";
                else
                    std::cout << "Saved: " << jsonPath.string() << "\n";
            }

            ImGui::SameLine();

            if (ImGui::Button("Load Save"))
            {
                if (!loadStateJSON(doc, jsonPath.string()))
                    std::cerr << "State load failed: " << jsonPath.string() << "\n";
                else
                    std::cout << "Loaded: " << jsonPath.string() << "\n";
            }

            ImGui::TextDisabled("Images: %s", imgDir.string().c_str());
            ImGui::TextDisabled("Saves:  %s", saveDir.string().c_str());
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

//Main loop.
void App::run()
{
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawUI();
        handleInput();

        glViewport(0, 0, fbW, fbH);

        glClearColor(0.12f, 0.12f, 0.125f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawScene();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
    }
}