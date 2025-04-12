#pragma once

#include "NodeGraph.h" // Includes the base Node, Pin, Link, GetInputImageData
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <optional>
#include <algorithm> // For std::transform in ImageInputNode (optional)

// Make sure OpenGL header is included (needed for GLuint)
// This might come from NodeGraph.h if it includes imgui.h which includes gl headers via backend,
// but including it here makes the dependency explicit.
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif


// --- Image Input Node ---
struct ImageInputNode : public Node {
    std::string filePath = "";
    cv::Mat loadedImage;
    int imgWidth = 0;
    int imgHeight = 0;
    std::string imgFormat = ""; // Store format (from extension)

    ImageInputNode(int id, int pinId) : Node(id, "Image Input") {
        Pin outPin(pinId, "Output", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    // Override the process method
    void process() override {
        printf("Processing node: %s (ID: %d)\n", name.c_str(), id);
        loadedImage.release(); // Clear previous image data
        imgWidth = 0;
        imgHeight = 0;
        imgFormat = "";

        if (!filePath.empty()) {
            printf("  Attempting to load image: %s\n", filePath.c_str());
            loadedImage = cv::imread(filePath); // Load the image
            if (loadedImage.empty()) {
                fprintf(stderr, "  Error: Could not load image from %s\n", filePath.c_str());
                // Clear output pin data if loading failed
                if (!outputPins.empty()) {
                     outputPins[0].imageData.release();
                }
            } else {
                imgWidth = loadedImage.cols;
                imgHeight = loadedImage.rows;
                // Extract format from file extension (basic)
                size_t dotPos = filePath.find_last_of(".");
                if (dotPos != std::string::npos) {
                    imgFormat = filePath.substr(dotPos + 1);
                    // Convert to uppercase for display consistency
                     std::transform(imgFormat.begin(), imgFormat.end(), imgFormat.begin(), ::toupper);
                }
                printf("  Loaded image: %s (%dx%d) Format: %s\n", filePath.c_str(), imgWidth, imgHeight, imgFormat.c_str());
                 // Store loaded image in the output pin
                 if (!outputPins.empty()) {
                     outputPins[0].imageData = loadedImage; // Store the result
                     printf("  Stored loaded image in output pin %d\n", outputPins[0].id);
                 }
            }
        } else {
            printf("  No file path set.\n");
             // Clear output pin data if no file path
             if (!outputPins.empty()) {
                 outputPins[0].imageData.release();
             }
        }
    }
};


// --- Output Node ---
struct OutputNode : public Node {
    cv::Mat resultImage;
    GLuint previewTextureId = 0;
    int textureWidth = 0;
    int textureHeight = 0;
    
    // Add new members for save settings
    std::string saveFilePath;
    int jpegQuality = 95;  // 0-100 for JPEG
    int pngCompression = 6; // 0-9 for PNG
    std::string selectedFormat = "PNG"; // Default format

    OutputNode(int id, int pinId) : Node(id, "Output") {
        Pin inPin(pinId, "Input", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);
    }

    // Destructor to clean up texture
    ~OutputNode() override {
        if (previewTextureId != 0) {
            // It's good practice to check if an OpenGL context is still current
            // before calling glDeleteTextures, but in simple shutdown it's often okay.
            glDeleteTextures(1, &previewTextureId);
            printf("Deleted texture %d for Output Node %d\n", previewTextureId, id);
        }
    }

    // Function to update the OpenGL texture
    void UpdatePreviewTexture() {
        if (resultImage.empty()) {
            if (previewTextureId != 0) {
                glDeleteTextures(1, &previewTextureId);
                previewTextureId = 0;
                textureWidth = 0;
                textureHeight = 0;
                 printf("Cleared texture for Output Node %d\n", id);
            }
            return;
        }

        // Convert BGR/BGRA/Gray (OpenCV) to RGBA for OpenGL/ImGui display
        cv::Mat displayMat;
        if (resultImage.channels() == 3) {
            cv::cvtColor(resultImage, displayMat, cv::COLOR_BGR2RGBA);
        } else if (resultImage.channels() == 1) {
            cv::cvtColor(resultImage, displayMat, cv::COLOR_GRAY2RGBA);
        } else if (resultImage.channels() == 4) {
             cv::cvtColor(resultImage, displayMat, cv::COLOR_BGRA2RGBA); // Assuming BGRA if 4 channels
        } else {
             fprintf(stderr, "Warning: Unsupported channel count (%d) for preview in Node %d\n", resultImage.channels(), id);
             // Optionally clear texture if format is unsupported
             if (previewTextureId != 0) { glDeleteTextures(1, &previewTextureId); previewTextureId = 0; textureWidth = 0; textureHeight = 0; }
             return;
        }

        // Generate or update texture
        if (previewTextureId == 0) {
            glGenTextures(1, &previewTextureId);
            printf("Generated texture %d for Output Node %d\n", previewTextureId, id);
        }

        glBindTexture(GL_TEXTURE_2D, previewTextureId);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // Use GL_CLAMP
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); // Use GL_CLAMP

        // Upload pixel data - check if resize is needed
        if (textureWidth != displayMat.cols || textureHeight != displayMat.rows) {
             glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, displayMat.cols, displayMat.rows, 0,
                          GL_RGBA, GL_UNSIGNED_BYTE, displayMat.data);
             textureWidth = displayMat.cols;
             textureHeight = displayMat.rows;
             printf("Allocated texture %d (%dx%d) for Output Node %d\n", previewTextureId, textureWidth, textureHeight, id);
        } else {
             glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, displayMat.cols, displayMat.rows,
                             GL_RGBA, GL_UNSIGNED_BYTE, displayMat.data);
             // printf("Updated texture %d for Output Node %d\n", previewTextureId, id); // Can be spammy
        }

        glBindTexture(GL_TEXTURE_2D, 0); // Unbind
    }

    // Add save functionality
    bool SaveImageToDisk() {
        if (resultImage.empty()) {
            printf("Error: No image data to save\n");
            return false;
        }

        std::vector<int> params;
        if (selectedFormat == "JPEG" || selectedFormat == "JPG") {
            params.push_back(cv::IMWRITE_JPEG_QUALITY);
            params.push_back(jpegQuality);
        } else if (selectedFormat == "PNG") {
            params.push_back(cv::IMWRITE_PNG_COMPRESSION);
            params.push_back(pngCompression);
        }

        try {
            return cv::imwrite(saveFilePath, resultImage, params);
        } catch (const cv::Exception& ex) {
            printf("Error saving image: %s\n", ex.what());
            return false;
        }
    }

    void process() override {
         printf("Processing node: %s (ID: %d)\n", name.c_str(), id);
         bool dataChanged = false;
         cv::Mat previousResult = resultImage; // Keep track if content actually changed
         resultImage.release(); // Assume we start fresh

         if (!inputPins.empty()) {
            extern NodeGraph g_Graph; // Use global graph for simplicity
            std::optional<cv::Mat> inputData = GetInputImageData(g_Graph, inputPins[0].id);

            if (inputData.has_value() && !inputData.value().empty()) {
                // Check if dimensions or type changed, or just assume changed if received
                if (previousResult.empty() || previousResult.cols != inputData.value().cols || previousResult.rows != inputData.value().rows || previousResult.type() != inputData.value().type()) {
                     dataChanged = true;
                 } // Could add content comparison if needed, but often assume change on new input
                 else {
                     // For now, assume content changed if input exists
                     dataChanged = true;
                 }

                printf("  Output node received image data (%dx%d) from pin %d.\n", inputData.value().cols, inputData.value().rows, inputPins[0].id);
                resultImage = inputData.value().clone();
            } else {
                printf("  Output node did not receive image data from pin %d.\n", inputPins[0].id);
                 if (!previousResult.empty()) { // If we had data before but now don't
                     dataChanged = true;
                 }
            }
         } else {
             printf("  Output node has no input pins defined.\n");
             if (!previousResult.empty()) { // If we had data before but now have no pins
                 dataChanged = true;
             }
         }

         // Update texture only if the data actually changed or disappeared
         if (dataChanged) {
             UpdatePreviewTexture();
         }
    }
};


// --- Brightness/Contrast Node ---
struct BrightnessContrastNode : public Node {
    float brightness = 0.0f; // Range: -100 to 100 (maps to beta in convertTo)
    float contrast = 1.0f;   // Range: 0 to 3 (maps to alpha in convertTo)
    
    // Add default values as constants
    const float DEFAULT_BRIGHTNESS = 0.0f;
    const float DEFAULT_CONTRAST = 1.0f;

    BrightnessContrastNode(int id, int inputPinId, int outputPinId) : Node(id, "Brightness/Contrast") {
        Pin inPin(inputPinId, "Image In", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);

        Pin outPin(outputPinId, "Image Out", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    // Add reset method
    void reset() {
        brightness = DEFAULT_BRIGHTNESS;
        contrast = DEFAULT_CONTRAST;
        // Trigger reprocessing after reset
        process();
    }

    void process() override {
         printf("Processing node: %s (ID: %d)\n", name.c_str(), id);
         printf("  Params: Brightness: %.2f, Contrast: %.2f\n", brightness, contrast);

         // Clear previous output
         if (!outputPins.empty()) outputPins[0].imageData.release();

         if (!inputPins.empty() && !outputPins.empty()) {
             extern NodeGraph g_Graph;
             std::optional<cv::Mat> inputDataOpt = GetInputImageData(g_Graph, inputPins[0].id);

             if (inputDataOpt.has_value() && !inputDataOpt.value().empty()) {
                 cv::Mat inputImage = inputDataOpt.value();
                 printf("  Brightness node received image (%dx%d) from pin %d.\n", inputImage.cols, inputImage.rows, inputPins[0].id);

                 cv::Mat processedImage;
                 double alpha = contrast; // Contrast factor
                 double beta = brightness; // Brightness offset

                 inputImage.convertTo(processedImage, -1, alpha, beta);

                 // Store result in output pin
                 outputPins[0].imageData = processedImage;
                 printf("  Stored processed image in output pin %d\n", outputPins[0].id);

             } else {
                 printf("  Brightness node did not receive image data from pin %d.\n", inputPins[0].id);
             }
         } else {
              printf("  Brightness node missing input or output pins.\n");
         }
    }
};