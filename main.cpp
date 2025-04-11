#include <stdio.h> // For printf

// === Dear ImGui & Node Editor Includes ===
#include "libs/imgui/imgui.h" // Using ImGui v1.88
#include "libs/imnodes/imnodes.h" // Using ImNodes
#include "libs/imgui/backends/imgui_impl_glfw.h"
#include "libs/imgui/backends/imgui_impl_opengl3.h"

// === Graphics & System Includes ===
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>

// === Standard Library Includes ===
#include <iostream>

// === Helper Functions ===
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

// === Main Application ===
int main(int, char**)
{
    printf("Starting application...\n");

    // --- 1. Setup GLFW window ---
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }
    printf("GLFW Initialized.\n");

    // GL hints (using GL 3.0 as before)
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Node Image Processor (ImNodes)", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    printf("GLFW Window Created.\n");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // --- 2. Initialize ImGui & ImNodes ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    printf("ImGui Context Created.\n");
    ImNodes::CreateContext(); // Initialize ImNodes
    printf("ImNodes Context Created.\n");

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // NOTE: Docking/Viewports are NOT enabled when using standard ImGui v1.88

    ImGui::StyleColorsDark(); // Set ImGui style
    ImNodes::StyleColorsDark(); // Set ImNodes style to match
    printf("ImGui/ImNodes Style Set.\n");

    // Setup Platform/Renderer backends
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
         std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
         ImNodes::DestroyContext();
         ImGui::DestroyContext();
         glfwDestroyWindow(window);
         glfwTerminate();
         return 1;
    }
    printf("ImGui GLFW Backend Initialized.\n");
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
         std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
         ImGui_ImplGlfw_Shutdown();
         ImNodes::DestroyContext();
         ImGui::DestroyContext();
         glfwDestroyWindow(window);
         glfwTerminate();
         return 1;
    }
    printf("ImGui OpenGL3 Backend Initialized.\n");

    // Background color
    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f); // Dark background

    printf("Entering main loop...\n");
    // --- 3. Main Render Loop ---
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Draw the Node Editor ---
        ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver); // Set default size
        ImGui::Begin("Node Editor"); // Start ImGui window to host the editor

        ImNodes::BeginNodeEditor(); // Start the actual node editor canvas

        // --- Nodes and Links will go here later ---
        // Example: You could uncomment the ImNodes::BeginNode example from the previous step
        //          to see a dummy node appear on the canvas.

        ImNodes::EndNodeEditor(); // End the node editor canvas

        ImGui::End(); // End the host ImGui window


        // --- Rendering ---
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // No platform window rendering needed

        glfwSwapBuffers(window);
    }
    printf("Exited main loop.\n");

    // --- 4. Cleanup ---
    printf("Starting cleanup...\n");
    ImNodes::DestroyContext();      // Destroy ImNodes context
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();        // Destroy ImGui context

    glfwDestroyWindow(window);
    glfwTerminate();
    printf("Cleanup finished. Exiting.\n");

    return 0;
}