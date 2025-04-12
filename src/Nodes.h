#pragma once

#include "NodeGraph.h" // Include the base definitions
#include <string>
#include <opencv2/opencv.hpp> // Need OpenCV for image nodes
#include <algorithm> // For std::transform

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
                // Keep path, but clear image data and metadata
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
                // Later: Pass this loadedImage to connected nodes via outputPins[0]
            }
        } else {
            printf("  No file path set.\n");
        }
    }
};

// --- Output Node ---
struct OutputNode : public Node {
    // We'll store the input image data here when connected
    cv::Mat resultImage;

    OutputNode(int id, int pinId) : Node(id, "Output") {
        // Explicitly create the pin and add to the correct vector
       Pin inPin(pinId, "Input", PinKind::Input);
       inPin.node = this;
       inputPins.push_back(inPin); // Add to inputPins vector
   }

    void process() override {
         printf("Processing node: %s (ID: %d)\n", name.c_str(), id);
         // Later: Get image data from the input pin connection
         if (!resultImage.empty()) {
             printf("  Output node has an image (%dx%d)\n", resultImage.cols, resultImage.rows);
             // Later: Add save functionality, display preview
         } else {
             printf("  Output node has no image data.\n");
         }
    }
};

// --- Brightness/Contrast Node (Example Structure) ---
struct BrightnessContrastNode : public Node {
    float brightness = 0.0f; // Parameter (-100 to 100 maps to -1.0 to 1.0?)
    float contrast = 1.0f;   // Parameter (0 to 3)

    BrightnessContrastNode(int id, int inputPinId, int outputPinId) : Node(id, "Brightness/Contrast") {
        // Explicitly create pins
        Pin inPin(inputPinId, "Image In", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);

        Pin outPin(outputPinId, "Image Out", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    void process() override {
         printf("Processing node: %s (ID: %d)\n", name.c_str(), id);
         printf("  Brightness: %.2f, Contrast: %.2f\n", brightness, contrast);
         // Later: Get input image, apply OpenCV brightness/contrast, store result
    }
};