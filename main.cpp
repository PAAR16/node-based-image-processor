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
NodeGraph g_Graph;  // Global graph instance

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

// --- NEW HELPER: Find Pin by ID ---
Pin* FindPinById(int id) {
    for (Node* node : g_Graph.nodes) {
        for (Pin& pin : node->inputPins) {
            if (pin.id == id) return &pin;
        }
        for (Pin& pin : node->outputPins) {
            if (pin.id == id) return &pin;
        }
    }
    return nullptr;
}

// --- NEW HELPER: Check if Input Pin Already Linked ---
bool IsInputPinLinked(int inputPinId) {
    for (const Link& link : g_Graph.links) {
        if (link.endPinId == inputPinId) {
            return true;
        }
    }
    return false;
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
            if (ImGui::MenuItem("Color Channel Splitter")) {
                int node_id = g_Graph.nextNodeId++;
                int pin_in = g_Graph.nextPinId++;
                int pin_red = g_Graph.nextPinId++;
                int pin_green = g_Graph.nextPinId++;
                int pin_blue = g_Graph.nextPinId++;
                QueueAddNode(new ColorChannelSplitterNode(node_id, pin_in, pin_red, pin_green, pin_blue));
            }
            if (ImGui::MenuItem("Blur")) {
                int node_id = g_Graph.nextNodeId++;
                int pin_in = g_Graph.nextPinId++;
                int pin_out = g_Graph.nextPinId++;
                QueueAddNode(new BlurNode(node_id, pin_in, pin_out));
            }
            if (ImGui::MenuItem("Threshold")) {
                int node_id = g_Graph.nextNodeId++;
                int pin_in = g_Graph.nextPinId++;
                int pin_out = g_Graph.nextPinId++;
                QueueAddNode(new ThresholdNode(node_id, pin_in, pin_out));
            }
            if (ImGui::MenuItem("Edge Detection")) {
                int node_id = g_Graph.nextNodeId++;
                int pin_in = g_Graph.nextPinId++;
                int pin_out = g_Graph.nextPinId++;
                QueueAddNode(new EdgeDetectionNode(node_id, pin_in, pin_out));
            }
            if (ImGui::MenuItem("Blend")) {
                int node_id = g_Graph.nextNodeId++;
                int pin_in1 = g_Graph.nextPinId++;
                int pin_in2 = g_Graph.nextPinId++;
                int pin_out = g_Graph.nextPinId++;
                QueueAddNode(new BlendNode(node_id, pin_in1, pin_in2, pin_out));
            }
            if (ImGui::MenuItem("Noise Generator")) {
                int node_id = g_Graph.nextNodeId++;
                int pin_out = g_Graph.nextPinId++;
                QueueAddNode(new NoiseGenerationNode(node_id, pin_out));
            }
            if (ImGui::MenuItem("Convolution Filter")) {
                int node_id = g_Graph.nextNodeId++;
                int pin_in = g_Graph.nextPinId++;
                int pin_out = g_Graph.nextPinId++;
                QueueAddNode(new ConvolutionFilterNode(node_id, pin_in, pin_out));
            }

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

        // NEW: Add Process Button at the top of properties panel
        if (ImGui::Button("Process Graph")) {
            g_Graph.executeGraph(); // Correct case for method name
        }
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
                    ImGui::Text("Brightness/Contrast Settings:");
                    ImGui::Separator();
                    
                    bool changed = false;

                    // Reset All button
                    if (ImGui::Button("Reset All##BC")) {
                        bcNode->reset();
                        changed = true;
                    }

                    // Individual parameter resets
                    if (ImGui::Button("Reset##Brightness")) {
                        bcNode->brightness = BrightnessContrastNode::DEFAULT_BRIGHTNESS;
                        changed = true;
                    }
                    ImGui::SameLine();
                    changed |= ImGui::SliderFloat("Brightness", &bcNode->brightness, -100.0f, 100.0f);

                    if (ImGui::Button("Reset##Contrast")) {
                        bcNode->contrast = BrightnessContrastNode::DEFAULT_CONTRAST;
                        changed = true;
                    }
                    ImGui::SameLine();
                    changed |= ImGui::SliderFloat("Contrast", &bcNode->contrast, 0.0f, 3.0f);

                    if (changed) bcNode->process();
                }
                else if (ImageInputNode* inputNode = dynamic_cast<ImageInputNode*>(selected_node)) {
                    ImGui::Text("Image Input Settings:");
                    ImGui::Separator();

                    // File path display/load button
                    ImGui::Text("File: %s", inputNode->filePath.empty() ? "No file selected" : inputNode->filePath.c_str());
                    if (ImGui::Button("Load Image")) {
                        auto selection = pfd::open_file("Choose image file", ".",
                            { "Image files", "*.png *.jpg *.jpeg *.bmp" });
                        if (!selection.result().empty()) {
                            inputNode->filePath = selection.result()[0];
                            inputNode->process();
                        }
                    }

                    // Display metadata
                    if (!inputNode->loadedImage.empty()) {
                        ImGui::Separator();
                        ImGui::Text("Metadata:");
                        ImGui::BulletText("Dimensions: %dx%d", inputNode->imgWidth, inputNode->imgHeight);
                        ImGui::BulletText("Format: %s", inputNode->imgFormat.c_str());
                        ImGui::BulletText("File Size: %.2f MB", inputNode->fileSize / (1024.0 * 1024.0));
                        ImGui::BulletText("Channels: %d", inputNode->loadedImage.channels());
                        ImGui::BulletText("Depth: %d-bit", inputNode->loadedImage.elemSize1() * 8);
                    }
                }
                else if (OutputNode* outputNode = dynamic_cast<OutputNode*>(selected_node)) {
                    ImGui::Text("Output Properties");
                    ImGui::Separator();

                    // --- NEW: Display Preview Image ---
                    if (outputNode->previewTextureId != 0 && outputNode->textureWidth > 0 && outputNode->textureHeight > 0)
                    {
                        ImGui::Text("Preview:");
                        // Display the texture using ImGui::Image
                        // Ensure correct casting for ImTextureID (depends on ImGui backend)
                        // For OpenGL, it's typically the GLuint cast to void*
                        ImTextureID tex_id = (void*)(intptr_t)outputNode->previewTextureId;

                        // Control preview size (e.g., fit width)
                        float panelWidth = ImGui::GetContentRegionAvail().x;
                        float aspect = (float)outputNode->textureHeight / (float)outputNode->textureWidth;
                        float previewHeight = panelWidth * aspect;
                        ImVec2 previewSize(panelWidth, previewHeight);

                        ImGui::Image(tex_id, previewSize);
                    } else {
                        ImGui::Text("Preview: <No image data>");
                    }
                    ImGui::Separator();

                    // Add Save Controls
                    if (!outputNode->resultImage.empty()) {
                        // Format selection
                        const char* formats[] = { "PNG", "JPEG", "BMP" };
                        static int formatIndex = 0;
                        if (ImGui::Combo("Format", &formatIndex, formats, IM_ARRAYSIZE(formats))) {
                            outputNode->selectedFormat = formats[formatIndex];
                        }

                        // Quality settings
                        if (outputNode->selectedFormat == "JPEG") {
                            ImGui::SliderInt("JPEG Quality", &outputNode->jpegQuality, 0, 100);
                        } else if (outputNode->selectedFormat == "PNG") {
                            ImGui::SliderInt("PNG Compression", &outputNode->pngCompression, 0, 9);
                        }

                        if (ImGui::Button("Save Image...")) {
                            std::string extension = "." + outputNode->selectedFormat;
                            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                            
                            auto selection = pfd::save_file(
                                "Save Image",
                                ".",
                                { "Image Files", "*" + extension },
                                pfd::opt::none
                            ).result();

                            if (!selection.empty()) {
                                outputNode->saveFilePath = selection;
                                // Ensure correct extension
                                if (outputNode->saveFilePath.substr(outputNode->saveFilePath.length() - extension.length()) != extension) {
                                    outputNode->saveFilePath += extension;
                                }
                                
                                if (outputNode->SaveImageToDisk()) {
                                    printf("Image saved successfully to: %s\n", outputNode->saveFilePath.c_str());
                                } else {
                                    printf("Failed to save image to: %s\n", outputNode->saveFilePath.c_str());
                                }
                            }
                        }
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No image data to save");
                    }
                    ImGui::Separator();
                }
                else if (ColorChannelSplitterNode* splitterNode = dynamic_cast<ColorChannelSplitterNode*>(selected_node)) {
                    ImGui::Text("Channel Splitter Settings:");
                    ImGui::Separator();

                    bool changed = false;
                    changed |= ImGui::Checkbox("Output as Grayscale", &splitterNode->outputAsGrayscale);
                    
                    if (changed) {
                        printf("Color Channel Splitter node %d settings updated\n", splitterNode->id);
                        splitterNode->process();
                    }

                    ImGui::Separator();
                    ImGui::Text("Output Channels:");
                    ImGui::BulletText("Red (Channel 0)");
                    ImGui::BulletText("Green (Channel 1)");
                    ImGui::BulletText("Blue (Channel 2)");
                }
                else if (BlurNode* blurNode = dynamic_cast<BlurNode*>(selected_node)) {
                    ImGui::Text("Blur Settings:");
                    ImGui::Separator();

                    bool changed = false;
                    
                    if (ImGui::Button("Reset All##Blur")) {
                        blurNode->reset();
                        changed = true;
                    }

                    if (ImGui::Button("Reset##Radius")) {
                        blurNode->radius = BlurNode::DEFAULT_RADIUS;
                        changed = true;
                    }
                    ImGui::SameLine();
                    changed |= ImGui::SliderInt("Radius", &blurNode->radius, 1, 20);

                    if (blurNode->directionalBlur) {
                        if (ImGui::Button("Reset##Angle")) {
                            blurNode->angle = BlurNode::DEFAULT_ANGLE;
                            changed = true;
                        }
                        ImGui::SameLine();
                        changed |= ImGui::SliderAngle("Angle", &blurNode->angle, 0.0f, 360.0f);
                    }

                    if (changed) blurNode->process();
                }
                else if (ThresholdNode* threshNode = dynamic_cast<ThresholdNode*>(selected_node)) {
                    ImGui::Text("Threshold Settings:");
                    ImGui::Separator();

                    bool changed = false;

                    // Threshold method selection
                    const char* methods[] = { "Binary", "Adaptive", "Otsu" };
                    changed |= ImGui::Combo("Method", &threshNode->thresholdMethod, methods, IM_ARRAYSIZE(methods));

                    // Method-specific controls
                    if (threshNode->thresholdMethod == 0) { // Binary
                        changed |= ImGui::SliderInt("Threshold", &threshNode->thresholdValue, 0, 255);
                    }
                    else if (threshNode->thresholdMethod == 1) { // Adaptive
                        changed |= ImGui::SliderInt("Block Size", &threshNode->blockSize, 3, 99, "%d");
                        if (threshNode->blockSize % 2 == 0) threshNode->blockSize++; // Ensure odd
                        changed |= ImGui::SliderFloat("C", &threshNode->C, -10.0f, 10.0f); // Now using float
                    }

                    // Show histogram
                    if (threshNode->histogramTexId != 0) {
                        ImGui::Text("Histogram:");
                        ImGui::Image((void*)(intptr_t)threshNode->histogramTexId, 
                                    ImVec2(256, 100));
                    }

                    if (changed) {
                        threshNode->process();
                    }
                }
                else if (EdgeDetectionNode* edgeNode = dynamic_cast<EdgeDetectionNode*>(selected_node)) {
                    ImGui::Text("Edge Detection Settings:");
                    ImGui::Separator();

                    bool changed = false;

                    // Algorithm selection
                    const char* algorithms[] = { "Sobel", "Canny" };
                    changed |= ImGui::Combo("Algorithm", &edgeNode->algorithm, algorithms, IM_ARRAYSIZE(algorithms));

                    if (edgeNode->algorithm == 0) { // Sobel
                        const char* sizes[] = { "3x3", "5x5", "7x7" };
                        int sizeIndex = (edgeNode->kernelSize - 3) / 2;
                        if (ImGui::Combo("Kernel Size", &sizeIndex, sizes, IM_ARRAYSIZE(sizes))) {
                            edgeNode->kernelSize = 3 + (sizeIndex * 2);
                            changed = true;
                        }
                    } else { // Canny
                        changed |= ImGui::SliderInt("Lower Threshold", &edgeNode->cannyThresh1, 0, 255);
                        changed |= ImGui::SliderInt("Upper Threshold", &edgeNode->cannyThresh2, 0, 255);
                    }

                    changed |= ImGui::Checkbox("Overlay on Original", &edgeNode->overlayMode);

                    if (changed) {
                        edgeNode->process();
                    }
                }
                else if (BlendNode* blendNode = dynamic_cast<BlendNode*>(selected_node)) {
                    ImGui::Text("Blend Settings:");
                    ImGui::Separator();

                    bool changed = false;

                    // Blend mode selection
                    const char* modes[] = { "Normal", "Multiply", "Screen", "Overlay", "Difference" };
                    changed |= ImGui::Combo("Blend Mode", &blendNode->blendMode, modes, IM_ARRAYSIZE(modes));

                    // Opacity control
                    changed |= ImGui::SliderFloat("Opacity", &blendNode->opacity, 0.0f, 1.0f);

                    if (changed) {
                        blendNode->process();
                    }
                }
                else if (NoiseGenerationNode* noiseNode = dynamic_cast<NoiseGenerationNode*>(selected_node)) {
                    ImGui::Text("Noise Generator Settings:");
                    ImGui::Separator();

                    bool changed = false;
                    const char* noiseTypes[] = { "Perlin", "Simplex", "Worley" };
                    changed |= ImGui::Combo("Type", &noiseNode->noiseType, noiseTypes, IM_ARRAYSIZE(noiseTypes));
                    changed |= ImGui::SliderFloat("Scale", &noiseNode->scale, 1.0f, 100.0f);
                    changed |= ImGui::SliderInt("Octaves", &noiseNode->octaves, 1, 8);
                    changed |= ImGui::SliderFloat("Persistence", &noiseNode->persistence, 0.0f, 1.0f);
                    changed |= ImGui::Checkbox("Use as Displacement", &noiseNode->useAsDisplacement);
                    
                    if (changed) {
                        noiseNode->process();
                    }
                }
                else if (ConvolutionFilterNode* convNode = dynamic_cast<ConvolutionFilterNode*>(selected_node)) {
                    ImGui::Text("Convolution Filter Settings:");
                    ImGui::Separator();

                    bool changed = false;
                    const char* presets[] = { "Identity", "Sharpen", "Emboss", "Edge Enhance" };
                    if (ImGui::Combo("Preset", &convNode->presetIndex, presets, IM_ARRAYSIZE(presets))) {
                        convNode->updatePreset(convNode->presetIndex);
                        changed = true;
                    }

                    ImGui::Text("Kernel Values:");
                    for (int i = 0; i < convNode->kernelSize; i++) {
                        for (int j = 0; j < convNode->kernelSize; j++) {
                            ImGui::PushID(i * convNode->MAX_KERNEL_SIZE + j);
                            if (ImGui::DragFloat("##v", &convNode->kernel[i * convNode->MAX_KERNEL_SIZE + j], 0.1f, -5.0f, 5.0f)) {
                                changed = true;
                                convNode->updateKernelPreview();
                            }
                            ImGui::PopID();
                            if (j < convNode->kernelSize-1) ImGui::SameLine();
                        }
                    }

                    if (convNode->previewTexId != 0) {
                        ImGui::Text("Kernel Preview:");
                        ImGui::Image((void*)(intptr_t)convNode->previewTexId, ImVec2(100, 100));
                    }

                    if (changed) {
                        convNode->process();
                    }
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


        // Handle Link Creation with Validation [MODIFIED BLOCK]
        int start_pin_id, end_pin_id;
        if (ImNodes::IsLinkCreated(&start_pin_id, &end_pin_id))
        {
            printf("Link creation attempt: %d -> %d\n", start_pin_id, end_pin_id);

            Pin* startPin = FindPinById(start_pin_id);
            Pin* endPin = FindPinById(end_pin_id);
            bool linkIsValid = true;

            // 1. Check if pins were found
            if (!startPin || !endPin) {
                fprintf(stderr, "  Error: Could not find pins for link creation (%d or %d).\n", 
                        start_pin_id, end_pin_id);
                linkIsValid = false;
            }

            // 2. Check Input/Output compatibility
            if (linkIsValid && startPin->kind == endPin->kind) {
                fprintf(stderr, "  Error: Cannot link %s pin to %s pin.\n",
                        (startPin->kind == PinKind::Input ? "Input" : "Output"),
                        (endPin->kind == PinKind::Input ? "Input" : "Output"));
                linkIsValid = false;
            }

            // 3. Ensure start is Output, end is Input
            if (linkIsValid && startPin->kind == PinKind::Input) {
                std::swap(start_pin_id, end_pin_id);
                std::swap(startPin, endPin);
                printf("    (Swapped link direction: Now %d -> %d)\n", start_pin_id, end_pin_id);
            }

            // 4. Check if Input pin already has a connection
            if (linkIsValid && IsInputPinLinked(endPin->id)) {
                fprintf(stderr, "  Error: Input pin %d (%s on Node %d) is already connected.\n",
                        endPin->id, endPin->name.c_str(), endPin->node->id);
                linkIsValid = false;
            }

            // 5. TODO: Add Data Type Compatibility Check here later if needed

            // 6. Add the link if all checks passed
            if (linkIsValid) {
                int linkId = g_Graph.nextLinkId++;
                printf("  Link VALID. Adding Link %d (%d -> %d)\n", linkId, start_pin_id, end_pin_id);
                g_Graph.links.push_back(Link(linkId, start_pin_id, end_pin_id));
            } else {
                printf("  Link INVALID. Link not added.\n");
            }
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