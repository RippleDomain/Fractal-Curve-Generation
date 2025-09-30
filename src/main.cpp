#include <cstdio>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

static void framebufferSizeCallback(GLFWwindow*, int w, int h) 
{
    glViewport(0, 0, w, h);
}

int main() {
    //---GLFW-INIT-STAGE---
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fractal Canvas", nullptr, nullptr);

    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    //---GLAD-LOAD-STAGE---
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) 
    {
        std::fprintf(stderr, "Failed to initialize GLAD\n");

        return -1;
    }

    int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);

    //---IMGUI-INIT-STAGE---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    //---GLM-TEST-STAGE---
    glm::mat4 proj = glm::ortho(0.0f, 1280.0f, 720.0f, 0.0f);
    float proj00 = proj[0][0];

    //---STB-WRITE-STAGE---
    bool wrotePNG = false;
    {
        const int cells = 10;
        const int cellPx = 8;
        const int W = cells * cellPx;
        const int H = cells * cellPx;

        std::vector<unsigned char> rgba(W * H * 4);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                const int cx = x / cellPx;
                const int cy = y / cellPx;
                const bool white = ((cx + cy) & 1) == 0;
                const unsigned char c = white ? 255 : 0;

                const int i = (y * W + x) * 4;
                rgba[i + 0] = c;
                rgba[i + 1] = c;
                rgba[i + 2] = c;
                rgba[i + 3] = 255;
            }
        }

        wrotePNG = (stbi_write_png("testOutput.png", W, H, 4, rgba.data(), W * 4) != 0);
    }

	//---MAIN-LOOP-STAGE---
    ImVec4 clearColor = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("CMake + Libraries OK");
        ImGui::Text("GL:   %s", glVersion ? glVersion : "unknown");
        ImGui::Text("GPU:  %s", glRenderer ? glRenderer : "unknown");
        ImGui::Separator();
        ImGui::Text("GLM ortho()[0][0] = %.3f", proj00);
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "ImGui is rendering.");
        ImGui::Text("%s", wrotePNG ? "Wrote testOutput.png" : "Failed to write test_output.png");
        ImGui::Text("Build and run was successful if all stages are affirmative and this window is visible.");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

	//---CLEANUP-STAGE---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}