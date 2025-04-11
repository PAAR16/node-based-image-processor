#include <stdio.h> // For printf

// Include OpenGL loader/headers AFTER GLFW
// Use ImGui's bundled OpenGL loader (we disabled GLAD in CMakeLists.txt)
// Change these lines:
// #include <libs/imgui/imgui.h>
// #include <libs/imgui/backends/imgui_impl_glfw.h>
// #include <libs/imgui/backends/imgui_impl_opengl3.h>

// TO use double quotes instead:
#include "libs/imgui/imgui.h"                     // Use quotes
#include "libs/imgui/backends/imgui_impl_glfw.h"  // Use quotes
#include "libs/imgui/backends/imgui_impl_opengl3.h" // Use quotes
// Important: Include glad/gl.h after imgui_impl_opengl3.h if using glad,
// otherwise include your OpenGL header (like GL/gl.h or glew.h) here if needed.
// Since we use imgui's loader, this might not be strictly necessary, but doesn't hurt.
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h> // Apple's OpenGL headers
#else
#include <GL/gl.h> // Standard OpenGL header (might be needed on MinGW)
                   // Or alternatively use GLEW or GLAD if preferred
#endif

// Include GLFW header AFTER OpenGL headers
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream> // For std::cerr, std::cout
#include <opencv2/opencv.hpp> // Include OpenCV header


// GLFW error callback function
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**)
{
    // --- 1. Setup GLFW window ---
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    // Decide GL+GLSL versions
    #if defined(IMGUI_IMPL_OPENGL_ES2)
        // GL ES 2.0 + GLSL 100
        const char* glsl_version = "#version 100";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    #elif defined(__APPLE__)
        // GL 3.2 + GLSL 150
        const char* glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    #else
        // GL 3.0 + GLSL 130
        const char* glsl_version = "#version 130";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // Optional on Windows/Linux
        //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Optional
    #endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Node Image Processor", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // --- 2. Initialize ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking (important for node editors)
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark(); // Or ImGui::StyleColorsLight(); ImGui::StyleColorsClassic();

    // When viewports are enabled, tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
         std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
         ImGui::DestroyContext();
         glfwDestroyWindow(window);
         glfwTerminate();
         return 1;
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
         std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
         ImGui_ImplGlfw_Shutdown();
         ImGui::DestroyContext();
         glfwDestroyWindow(window);
         glfwTerminate();
         return 1;
    }

    // Load Fonts (Optional)
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("path/to/font.ttf", 16.0f);

    // Our state for the application (e.g., clear color)
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool show_demo_window = true; // Show the ImGui demo window

    // --- 3. Main Render Loop ---
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- ImGui Content Goes Here ---

        // Show the ImGui Demo Window (useful for testing)
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        // Example: Create a simple window
        ImGui::Begin("Hello, world!");
        ImGui::Text("This is your Node Image Processor.");
        ImGui::Checkbox("Show Demo Window", &show_demo_window);
        ImGui::ColorEdit3("Clear color", (float*)&clear_color);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();

        // --- Rendering ---

        // Rendering ImGui
        ImGui::Render();

        // Get display size and set viewport
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        // Clear the screen
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render ImGui draw data
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows (for docking/viewports)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        // Swap the front and back buffers
        glfwSwapBuffers(window);
    }

    // --- 4. Cleanup ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}