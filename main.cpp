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
#include <vector>
#include <algorithm> // Required for std::remove_if

// === Project Headers ===
#include "src/NodeGraph.h" // Includes Pin, Link, Node base struct
#include "src/Nodes.h"     // Includes specific node types like ImageInputNode, OutputNode

// === Global Variables ===
static NodeGraph g_Graph; // Holds all nodes and links

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

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

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
    ImNodes::CreateContext();
    printf("ImNodes Context Created.\n");

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Docking/Viewports are disabled

    ImGui::StyleColorsDark();
    ImNodes::StyleColorsDark();
    printf("ImGui/ImNodes Style Set.\n");

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
         std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
         ImNodes::DestroyContext(); ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    printf("ImGui GLFW Backend Initialized.\n");
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
         std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
         ImGui_ImplGlfw_Shutdown(); ImNodes::DestroyContext(); ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    printf("ImGui OpenGL3 Backend Initialized.\n");

    // --- Create Initial Nodes ---
    printf("Creating initial nodes...\n");
    {
        int inputNodeId = g_Graph.nextNodeId++;
        int inputPinId = g_Graph.nextPinId++;
        ImageInputNode* inputNode = new ImageInputNode(inputNodeId, inputPinId);
        inputNode->graphPosition = ImVec2(100, 100);
        g_Graph.nodes.push_back(inputNode);

        int outputNodeId = g_Graph.nextNodeId++;
        int outputPinId = g_Graph.nextPinId++;
        OutputNode* outputNode = new OutputNode(outputNodeId, outputPinId);
        outputNode->graphPosition = ImVec2(500, 100); // Increased spacing
        g_Graph.nodes.push_back(outputNode);

        int bcNodeId = g_Graph.nextNodeId++;
        int bcInputPinId = g_Graph.nextPinId++;
        int bcOutputPinId = g_Graph.nextPinId++;
        BrightnessContrastNode* bcNode = new BrightnessContrastNode(bcNodeId, bcInputPinId, bcOutputPinId);
        bcNode->graphPosition = ImVec2(300, 250); // Position below
        g_Graph.nodes.push_back(bcNode);
    }
    printf("Initial nodes created.\n");


    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f); // Dark background

    printf("Entering main loop...\n");
    // --- 3. Main Render Loop ---
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Draw the Node Editor ---
        ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
        ImGui::Begin("Node Editor");

        ImNodes::BeginNodeEditor();

        // Draw Nodes
        for (Node* node : g_Graph.nodes) {
            ImNodes::BeginNode(node->id);

            // Set initial node position only once
            // (Might need a better way to handle loading positions later)
            // bool initial_render = ImGui::IsWindowAppearing(); // This applies to the outer ImGui window, maybe not ideal
            // if (initial_render) { // Crude check
            //     ImNodes::SetNodeGridSpacePos(node->id, node->graphPosition);
            // }
            // Let's skip SetNodeGridSpacePos for now to see if it helps layout

            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(node->name.c_str());
            ImNodes::EndNodeTitleBar();

            // Draw Input Pins (Simplified)
            for (Pin& pin : node->inputPins) {
                ImNodes::BeginInputAttribute(pin.id);
                ImGui::TextUnformatted(pin.name.c_str()); // Just the text
                ImNodes::EndInputAttribute();
            }

            // Draw Output Pins (Simplified)
            for (Pin& pin : node->outputPins) {
                ImNodes::BeginOutputAttribute(pin.id);
                ImGui::TextUnformatted(pin.name.c_str()); // Just the text
                ImNodes::EndOutputAttribute();
            }

            ImNodes::EndNode();

            // Update stored position (Commented out for testing layout)
            // node->graphPosition = ImNodes::GetNodeGridSpacePos(node->id);
        }

        // Draw Links
        for (const Link& link : g_Graph.links) {
            ImNodes::Link(link.id, link.startPinId, link.endPinId);
        }

        ImNodes::EndNodeEditor(); // End the node editor canvas


        // Handle Link Creation / Deletion
        int start_attr, end_attr;
        if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
             printf("Link created: %d -> %d\n", start_attr, end_attr);
             int linkId = g_Graph.nextLinkId++;
             g_Graph.links.push_back(Link(linkId, start_attr, end_attr));
        }

        int link_id_to_destroy; // Renamed variable to avoid shadowing
        if (ImNodes::IsLinkDestroyed(&link_id_to_destroy)) {
             printf("Link destroyed: %d\n", link_id_to_destroy);
             auto iter = std::remove_if(g_Graph.links.begin(), g_Graph.links.end(),
                                        [link_id_to_destroy](const Link& link) { return link.id == link_id_to_destroy; });
             if (iter != g_Graph.links.end()) {
                 g_Graph.links.erase(iter, g_Graph.links.end());
             }
        }


        ImGui::End(); // End the host ImGui window


        // --- Rendering ---
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    printf("Exited main loop.\n");

    // --- 4. Cleanup ---
    printf("Starting cleanup...\n");
    ImNodes::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    printf("Cleanup finished. Exiting.\n");

    // Graph destructor handles node deletion

    return 0;
}