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
#include <memory> // Useful for smart pointers later, optional for now
#include <string> // For string manipulation

// === Project Headers ===
#include "src/NodeGraph.h" // Includes Pin, Link, Node base struct
#include "src/Nodes.h"     // Includes specific node types like ImageInputNode, OutputNode

// === File Dialog Include ===
#include <portable-file-dialogs.h>

// === Global Variables ===
static NodeGraph g_Graph; // Holds all nodes and links

// === Helper Functions ===
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

// Helper to find a node by ID
Node* FindNodeById(int id) {
    for (Node* node : g_Graph.nodes) {
        if (node->id == id) {
            return node;
        }
    }
    return nullptr;
}


// === Main Application ===
int main(int, char**)
{
    printf("Starting application...\n");

    // --- 1. Setup GLFW window ---
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) { /* ... error handling ... */ std::cerr << "GLFW Init Failed\n"; return 1; }
    printf("GLFW Initialized.\n");

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Node Image Processor (ImNodes)", NULL, NULL);
    if (window == NULL) { /* ... error handling ... */ std::cerr << "GLFW Window Creation Failed\n"; glfwTerminate(); return 1; }
    printf("GLFW Window Created.\n");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // --- 2. Initialize ImGui & ImNodes ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    printf("ImGui Context Created.\n");
    ImNodes::CreateContext();
    printf("ImNodes Context Created.\n");

    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();
    ImNodes::StyleColorsDark();
    printf("ImGui/ImNodes Style Set.\n");

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) { /* ... error handling ... */ std::cerr << "ImGui GLFW Backend Failed\n"; ImNodes::DestroyContext(); ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate(); return 1; }
    printf("ImGui GLFW Backend Initialized.\n");
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) { /* ... error handling ... */ std::cerr << "ImGui OpenGL3 Backend Failed\n"; ImGui_ImplGlfw_Shutdown(); ImNodes::DestroyContext(); ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate(); return 1; }
    printf("ImGui OpenGL3 Backend Initialized.\n");


    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);

    printf("Entering main loop...\n");
    // --- 3. Main Render Loop ---
    while (!glfwWindowShouldClose(window))
    {
        int node_id_to_set_position = -1;
        ImVec2 new_node_screen_pos;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Draw the Node Editor ---
        ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
        ImGui::Begin("Node Editor");

        // Split the window into two sections: node canvas and properties
        ImGui::Columns(2, "NodeEditorColumns", true);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - 300); // Main canvas gets all space except 300px

        // Draw the node canvas in the left column
        ImNodes::BeginNodeEditor();

        // Draw Existing Nodes
        for (Node* node : g_Graph.nodes) {
            ImNodes::BeginNode(node->id);

            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(node->name.c_str());
            ImNodes::EndNodeTitleBar();

            // Draw Input Pins
            for (Pin& pin : node->inputPins) {
                ImNodes::BeginInputAttribute(pin.id);
                ImGui::TextUnformatted(pin.name.c_str());
                ImNodes::EndInputAttribute();
            }

            // Draw Output Pins
            for (Pin& pin : node->outputPins) {
                ImNodes::BeginOutputAttribute(pin.id);
                ImGui::TextUnformatted(pin.name.c_str());
                ImNodes::EndOutputAttribute();
            }

            ImNodes::EndNode();

            // Update stored position if node is dragged
            if (ImNodes::IsNodeSelected(node->id) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                node->graphPosition = ImNodes::GetNodeGridSpacePos(node->id);
            }
        }

        // Draw Links
        for (const Link& link : g_Graph.links) {
            ImNodes::Link(link.id, link.startPinId, link.endPinId);
        }

        // Handle Adding New Nodes via Context Menu
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
        if (ImGui::BeginPopupContextWindow("NodeContextMenu")) {
            ImVec2 click_screen_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

            auto QueueAddNode = [&](Node* newNode) {
                g_Graph.nodes.push_back(newNode);
                node_id_to_set_position = newNode->id;
                new_node_screen_pos = click_screen_pos;
                printf("Queued add %s (ID: %d)\n", newNode->name.c_str(), newNode->id);
            };

            if (ImGui::MenuItem("Image Input Node")) {
                int node_id = g_Graph.nextNodeId++; int pin_id = g_Graph.nextPinId++;
                QueueAddNode(new ImageInputNode(node_id, pin_id));
            }
            if (ImGui::MenuItem("Output Node")) {
                int node_id = g_Graph.nextNodeId++; int pin_id = g_Graph.nextPinId++;
                QueueAddNode(new OutputNode(node_id, pin_id));
            }
            if (ImGui::MenuItem("Brightness/Contrast")) {
                int node_id = g_Graph.nextNodeId++; int pin_in = g_Graph.nextPinId++; int pin_out = g_Graph.nextPinId++;
                QueueAddNode(new BrightnessContrastNode(node_id, pin_in, pin_out));
            }
            // Add more MenuItems for other node types later

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        ImNodes::EndNodeEditor(); // End the node editor canvas

        // Switch to the right column for properties
        ImGui::NextColumn();

        // Properties Panel (now integrated into the canvas)
        ImGui::BeginChild("Properties", ImVec2(0, 0), true);
        ImGui::Text("Properties");
        ImGui::Separator();

        int first_selected_node_id = -1;
        int selected_count = 0;

        // Get selected nodes
        for (Node* node : g_Graph.nodes) {
            if (ImNodes::IsNodeSelected(node->id)) {
                selected_count++;
                if (first_selected_node_id == -1) {
                    first_selected_node_id = node->id;
                }
            }
        }

        if (selected_count == 0) {
            ImGui::Text("No node selected.");
        } else if (selected_count == 1) {
            Node* selected_node = FindNodeById(first_selected_node_id);
            
            if (selected_node) {
                // Node header
                ImGui::Text("Selected Node:");
                ImGui::Separator();
                ImGui::Text("ID: %d", selected_node->id);
                ImGui::Text("Name: %s", selected_node->name.c_str());
                ImGui::Separator();

                // Node-specific properties
                if (BrightnessContrastNode* bcNode = dynamic_cast<BrightnessContrastNode*>(selected_node)) {
                    ImGui::Text("Parameters:");
                    ImGui::Text("  Brightness: %.2f", bcNode->brightness);
                    ImGui::Text("  Contrast: %.2f", bcNode->contrast);
                    ImGui::Separator();
                    
                    bool changed = false;
                    changed |= ImGui::SliderFloat("Brightness", &bcNode->brightness, -100.0f, 100.0f, "%.0f");
                    changed |= ImGui::SliderFloat("Contrast", &bcNode->contrast, 0.0f, 3.0f, "%.2f");
                    if (changed) {
                        printf("Node %d parameters updated (B:%.2f, C:%.2f)\n", bcNode->id, bcNode->brightness, bcNode->contrast);
                    }
                }
                else if (ImageInputNode* inputNode = dynamic_cast<ImageInputNode*>(selected_node)) {
                    ImGui::Text("Parameters:");
                    ImGui::TextWrapped("File Path: %s", inputNode->filePath.empty() ? "<None>" : inputNode->filePath.c_str());

                    if (ImGui::Button("Browse...")) {
                        auto selection = pfd::open_file("Select an Image",
                            ".",
                            { "Image Files", "*.jpg *.jpeg *.png *.bmp",
                              "All Files", "*" },
                            pfd::opt::none).result();

                        if (!selection.empty()) {
                            inputNode->filePath = selection[0];
                            printf("Selected file: %s\n", inputNode->filePath.c_str());
                            inputNode->process();
                        } else {
                            printf("File selection cancelled.\n");
                        }
                    }
                    ImGui::Separator();

                    ImGui::Text("Metadata:");
                    if (inputNode->imgWidth > 0) {
                        ImGui::Text("  Dimensions: %d x %d", inputNode->imgWidth, inputNode->imgHeight);
                        ImGui::Text("  Format: %s", inputNode->imgFormat.c_str());
                    } else {
                        ImGui::Text("  <No image loaded>");
                    }
                }
                else if (OutputNode* outputNode = dynamic_cast<OutputNode*>(selected_node)) {
                    ImGui::Text("Output Properties");
                    // Add output node specific properties here
                }
            }
        } else {
            ImGui::Text("%d nodes selected", selected_count);
            ImGui::Text("Multi-selection editing not supported yet");
        }

        ImGui::EndChild(); // End Properties
        ImGui::Columns(1); // Reset columns

        ImGui::End(); // End Node Editor window

        // Set position for newly added node (AFTER EndNodeEditor)
        if (node_id_to_set_position != -1) {
            Node* new_node_ptr = FindNodeById(node_id_to_set_position); // Use helper
            if (new_node_ptr) {
                printf("Attempting to set position for Node ID %d\n", node_id_to_set_position);
                ImNodes::SetNodeScreenSpacePos(node_id_to_set_position, new_node_screen_pos);
                // Immediately get the grid pos after setting screen pos
                new_node_ptr->graphPosition = ImNodes::GetNodeGridSpacePos(node_id_to_set_position);
                printf("  Stored grid position: (%.1f, %.1f)\n", new_node_ptr->graphPosition.x, new_node_ptr->graphPosition.y);
            }
             node_id_to_set_position = -1; // Reset
        }


        // Handle Link Creation / Deletion
        int start_attr, end_attr;
        if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
             printf("Link created: %d -> %d\n", start_attr, end_attr);
             int linkId = g_Graph.nextLinkId++;
             g_Graph.links.push_back(Link(linkId, start_attr, end_attr));
             // TODO: Add link validation here
        }

        int link_id_to_destroy;
        if (ImNodes::IsLinkDestroyed(&link_id_to_destroy)) {
             printf("Link destroyed: %d\n", link_id_to_destroy);
             auto iter = std::remove_if(g_Graph.links.begin(), g_Graph.links.end(),
                                        [link_id_to_destroy](const Link& link) { return link.id == link_id_to_destroy; });
             if (iter != g_Graph.links.end()) {
                 g_Graph.links.erase(iter, g_Graph.links.end());
             }
        }

        // Handle Node Deletion
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
            // Get selected nodes
            std::vector<int> selected_nodes;
            for (Node* node : g_Graph.nodes) {
                if (ImNodes::IsNodeSelected(node->id)) {
                    selected_nodes.push_back(node->id);
                }
            }

            if (!selected_nodes.empty()) {
                // First, remove all links connected to the nodes being deleted
                g_Graph.links.erase(
                    std::remove_if(g_Graph.links.begin(), g_Graph.links.end(),
                        [&selected_nodes](const Link& link) {
                            // Find if either start or end of the link is connected to a selected node
                            for (const Node* node : g_Graph.nodes) {
                                if (std::find(selected_nodes.begin(), selected_nodes.end(), node->id) != selected_nodes.end()) {
                                    // Check if link is connected to this node's pins
                                    for (const Pin& pin : node->inputPins) {
                                        if (pin.id == link.startPinId || pin.id == link.endPinId) return true;
                                    }
                                    for (const Pin& pin : node->outputPins) {
                                        if (pin.id == link.startPinId || pin.id == link.endPinId) return true;
                                    }
                                }
                            }
                            return false;
                        }
                    ),
                    g_Graph.links.end()
                );

                // Then remove the selected nodes
                for (int node_id : selected_nodes) {
                    auto it = std::find_if(g_Graph.nodes.begin(), g_Graph.nodes.end(),
                        [node_id](const Node* node) { return node->id == node_id; });
                    if (it != g_Graph.nodes.end()) {
                        delete *it; // Free the memory
                        g_Graph.nodes.erase(it);
                        printf("Deleted node %d\n", node_id);
                    }
                }
            }
        }

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
    ImGui_ImplOpenGL3_Shutdown(); // Correct function name
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    printf("Cleanup finished. Exiting.\n");

    return 0;
}