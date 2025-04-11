#pragma once

#include "NodeGraph.h" // Include the base definitions
#include <string>
#include <opencv2/opencv.hpp> // Need OpenCV for image nodes

// --- Image Input Node ---
struct ImageInputNode : public Node {
    std::string filePath = "";
    cv::Mat loadedImage; // OpenCV image data

    ImageInputNode(int id, int pinId) : Node(id, "Image Input") {
        // Explicitly create the pin and add to the correct vector
        Pin outPin(pinId, "Output", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin); // Add to outputPins vector
    }

    // Override the process method (example)
    void process() override {
        printf("Processing node: %s (ID: %d)\n", name.c_str(), id);
        if (!filePath.empty()) {
            loadedImage = cv::imread(filePath);
            if (loadedImage.empty()) {
                fprintf(stderr, "Error: Could not load image from %s\n", filePath.c_str());
            } else {
                printf("  Loaded image: %s (%dx%d)\n", filePath.c_str(), loadedImage.cols, loadedImage.rows);
                // Later: Pass this loadedImage to connected nodes
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