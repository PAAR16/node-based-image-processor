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


// --- Color Channel Splitter Node ---
struct ColorChannelSplitterNode : public Node {
    bool outputAsGrayscale = true;

    ColorChannelSplitterNode(int id, int inputPinId, int redPinId, int greenPinId, int bluePinId) 
        : Node(id, "Color Channel Splitter") {
        // Input pin
        Pin inPin(inputPinId, "Image", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);

        // Output pins for each channel
        Pin redPin(redPinId, "Red", PinKind::Output);
        redPin.node = this;
        outputPins.push_back(redPin);

        Pin greenPin(greenPinId, "Green", PinKind::Output);
        greenPin.node = this;
        outputPins.push_back(greenPin);

        Pin bluePin(bluePinId, "Blue", PinKind::Output);
        bluePin.node = this;
        outputPins.push_back(bluePin);
    }

    void process() override {
        printf("Processing ColorChannelSplitter node %d\n", id);
        
        // Clear output pins
        for (auto& pin : outputPins) {
            pin.imageData.release();
        }

        // Get input image
        cv::Mat inputImage = GetInputImageData(inputPins[0]);
        if (inputImage.empty()) {
            printf("  No input image available.\n");
            return;
        }

        // Convert to BGR if image is grayscale
        cv::Mat workingImage;
        if (inputImage.channels() == 1) {
            cv::cvtColor(inputImage, workingImage, cv::COLOR_GRAY2BGR);
        } else {
            workingImage = inputImage.clone();
        }
        
        // Split the channels
        std::vector<cv::Mat> channels;
        cv::split(workingImage, channels);  // channels[0] = Blue, [1] = Green, [2] = Red

        if (channels.size() >= 3) {
            for (int i = 0; i < 3; i++) {
                if (!outputAsGrayscale) {
                    // Create colored visualization
                    cv::Mat colorViz = cv::Mat::zeros(channels[i].size(), CV_8UC3);
                    int idx = 2 - i; // Reverse index for RGB order (0=R, 1=G, 2=B)
                    // Set the specific color channel
                    std::vector<cv::Mat> colorChannels(3);
                    for (int j = 0; j < 3; j++) {
                        colorChannels[j] = (j == idx) ? channels[idx] : cv::Mat::zeros(channels[idx].size(), CV_8UC1);
                    }
                    cv::merge(colorChannels, colorViz);
                    outputPins[i].imageData = colorViz;
                } else {
                    // Output as grayscale (single channel converted to BGR)
                    cv::Mat grayViz;
                    cv::cvtColor(channels[2-i], grayViz, cv::COLOR_GRAY2BGR);
                    outputPins[i].imageData = grayViz;
                }
            }
        }

        printf("  Channels split successfully.\n");
    }
};


// --- Blur Node ---
struct BlurNode : public Node {
    int radius = 3;           // Blur radius (1-20px)
    bool directionalBlur = false;
    float angle = 0.0f;       // Angle for directional blur (0-360 degrees)
    bool showKernel = true;   // Toggle kernel preview
    GLuint kernelPreviewTexId = 0;

    BlurNode(int id, int inputPinId, int outputPinId) : Node(id, "Blur") {
        // Input pin
        Pin inPin(inputPinId, "Image", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);

        // Output pin
        Pin outPin(outputPinId, "Output", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    ~BlurNode() override {
        if (kernelPreviewTexId != 0) {
            glDeleteTextures(1, &kernelPreviewTexId);
        }
    }

    void UpdateKernelPreview() {
        // Create kernel visualization
        int kernelSize = radius * 2 + 1;
        cv::Mat kernel = cv::getGaussianKernel(kernelSize, -1);
        cv::Mat kernel2D = kernel * kernel.t();
        
        // Normalize kernel for visualization
        cv::Mat kernelVis;
        cv::normalize(kernel2D, kernelVis, 0, 255, cv::NORM_MINMAX);
        kernelVis.convertTo(kernelVis, CV_8UC1);
        
        // Convert to RGBA for OpenGL texture
        cv::Mat kernelRGBA;
        cv::cvtColor(kernelVis, kernelRGBA, cv::COLOR_GRAY2RGBA);

        // Generate or update texture
        if (kernelPreviewTexId == 0) {
            glGenTextures(1, &kernelPreviewTexId);
        }

        glBindTexture(GL_TEXTURE_2D, kernelPreviewTexId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kernelSize, kernelSize, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, kernelRGBA.data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void process() override {
        printf("Processing Blur node %d\n", id);

        // Clear output
        if (!outputPins.empty()) {
            outputPins[0].imageData.release();
        }

        // Get input image
        cv::Mat inputImage = GetInputImageData(inputPins[0]);
        if (inputImage.empty()) {
            printf("  No input image available.\n");
            return;
        }

        // Create output image
        cv::Mat outputImage;
        int kernelSize = radius * 2 + 1;

        if (directionalBlur) {
            // Create motion blur kernel
            cv::Mat kernel = cv::Mat::zeros(kernelSize, kernelSize, CV_32F);
            cv::Point2f center(kernelSize / 2.0f, kernelSize / 2.0f);
            cv::Point2f dir(cos(angle * CV_PI / 180.0f), sin(angle * CV_PI / 180.0f));
            
            // Draw line on kernel
            for (int i = -radius; i <= radius; i++) {
                cv::Point2f pt = center + dir * float(i);
                if (pt.x >= 0 && pt.x < kernelSize && pt.y >= 0 && pt.y < kernelSize) {
                    kernel.at<float>(int(pt.y), int(pt.x)) = 1.0f;
                }
            }
            
            // Normalize kernel
            kernel = kernel / cv::sum(kernel)[0];
            
            // Apply directional blur
            cv::filter2D(inputImage, outputImage, -1, kernel);
        } else {
            // Apply Gaussian blur
            cv::GaussianBlur(inputImage, outputImage, cv::Size(kernelSize, kernelSize), 0);
        }

        // Update kernel preview
        if (showKernel) {
            UpdateKernelPreview();
        }

        // Store result
        outputPins[0].imageData = outputImage;
        printf("  Blur applied successfully.\n");
    }
};


// --- Threshold Node ---
struct ThresholdNode : public Node {
    int thresholdValue = 127;  // Default threshold value (0-255)
    int thresholdMethod = 0;   // 0=Binary, 1=Adaptive, 2=Otsu
    int blockSize = 11;        // For adaptive threshold (must be odd)
    float C = 2.0f;            // Changed from double to float
    GLuint histogramTexId = 0; // For histogram visualization
    std::vector<int> histogram;// Store histogram data

    ThresholdNode(int id, int inputPinId, int outputPinId) : Node(id, "Threshold") {
        // Input pin
        Pin inPin(inputPinId, "Image", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);

        // Output pin
        Pin outPin(outputPinId, "Output", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    ~ThresholdNode() override {
        if (histogramTexId != 0) {
            glDeleteTextures(1, &histogramTexId);
        }
    }

    void UpdateHistogram(const cv::Mat& image) {
        histogram.assign(256, 0);
        
        // Convert to grayscale if needed
        cv::Mat gray;
        if (image.channels() > 1) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image.clone();
        }

        // Calculate histogram
        for (int i = 0; i < gray.rows; i++) {
            for (int j = 0; j < gray.cols; j++) {
                histogram[gray.at<uchar>(i, j)]++;
            }
        }

        // Create histogram visualization
        int histHeight = 100;
        cv::Mat histImage = cv::Mat::zeros(histHeight, 256, CV_8UC4);
        
        // Find maximum for normalization
        int maxVal = *std::max_element(histogram.begin(), histogram.end());
        
        // Draw histogram
        for (int i = 0; i < 256; i++) {
            int height = static_cast<int>((histogram[i] * histHeight) / maxVal);
            cv::line(histImage, 
                    cv::Point(i, histHeight - 1), 
                    cv::Point(i, histHeight - height - 1),
                    cv::Scalar(255, 255, 255, 255));
            
            // Draw threshold line in red
            if (i == thresholdValue && thresholdMethod == 0) {
                cv::line(histImage,
                        cv::Point(i, 0),
                        cv::Point(i, histHeight - 1),
                        cv::Scalar(255, 0, 0, 255));
            }
        }

        // Update OpenGL texture
        if (histogramTexId == 0) {
            glGenTextures(1, &histogramTexId);
        }

        glBindTexture(GL_TEXTURE_2D, histogramTexId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, histImage.cols, histImage.rows, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, histImage.data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void process() override {
        printf("Processing Threshold node %d\n", id);

        // Clear output
        if (!outputPins.empty()) {
            outputPins[0].imageData.release();
        }

        // Get input image
        cv::Mat inputImage = GetInputImageData(inputPins[0]);
        if (inputImage.empty()) {
            printf("  No input image available.\n");
            return;
        }

        // Convert to grayscale if needed
        cv::Mat gray;
        if (inputImage.channels() > 1) {
            cv::cvtColor(inputImage, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = inputImage.clone();
        }

        // Update histogram
        UpdateHistogram(gray);

        // Apply thresholding
        cv::Mat result;
        switch (thresholdMethod) {
            case 0: // Binary
                cv::threshold(gray, result, thresholdValue, 255, cv::THRESH_BINARY);
                break;
            case 1: // Adaptive
                cv::adaptiveThreshold(gray, result, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                                    cv::THRESH_BINARY, blockSize, C);
                break;
            case 2: // Otsu
                cv::threshold(gray, result, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
                break;
        }

        // Convert back to BGR for display
        cv::cvtColor(result, outputPins[0].imageData, cv::COLOR_GRAY2BGR);
        printf("  Thresholding applied successfully.\n");
    }
};


// --- Edge Detection Node ---
struct EdgeDetectionNode : public Node {
    int algorithm = 0;     // 0=Sobel, 1=Canny
    int kernelSize = 3;    // 3, 5, or 7 for Sobel
    int cannyThresh1 = 100;// First threshold for Canny
    int cannyThresh2 = 200;// Second threshold for Canny
    bool overlayMode = false;// Overlay edges on original image
    
    EdgeDetectionNode(int id, int inputPinId, int outputPinId) : Node(id, "Edge Detection") {
        Pin inPin(inputPinId, "Image", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);

        Pin outPin(outputPinId, "Output", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    void process() override {
        printf("Processing Edge Detection node %d\n", id);
        
        if (!outputPins.empty()) {
            outputPins[0].imageData.release();
        }

        cv::Mat inputImage = GetInputImageData(inputPins[0]);
        if (inputImage.empty()) {
            printf("  No input image available.\n");
            return;
        }

        // Convert to grayscale if needed
        cv::Mat gray;
        if (inputImage.channels() > 1) {
            cv::cvtColor(inputImage, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = inputImage.clone();
        }

        cv::Mat edges;
        if (algorithm == 0) { // Sobel
            cv::Mat gradX, gradY;
            cv::Sobel(gray, gradX, CV_16S, 1, 0, kernelSize);
            cv::Sobel(gray, gradY, CV_16S, 0, 1, kernelSize);
            
            cv::Mat absGradX, absGradY;
            cv::convertScaleAbs(gradX, absGradX);
            cv::convertScaleAbs(gradY, absGradY);
            
            cv::addWeighted(absGradX, 0.5, absGradY, 0.5, 0, edges);
        } else { // Canny
            cv::Canny(gray, edges, cannyThresh1, cannyThresh2);
        }

        if (overlayMode && inputImage.channels() == 3) {
            cv::Mat overlay = inputImage.clone();
            overlay.setTo(cv::Scalar(0, 255, 0), edges); // Green edges
            cv::addWeighted(overlay, 0.7, inputImage, 0.3, 0, outputPins[0].imageData);
        } else {
            cv::cvtColor(edges, outputPins[0].imageData, cv::COLOR_GRAY2BGR);
        }

        printf("  Edge detection completed successfully.\n");
    }
};


// --- Blend Node ---
struct BlendNode : public Node {
    int blendMode = 0;    // 0=Normal, 1=Multiply, 2=Screen, 3=Overlay, 4=Difference
    float opacity = 1.0f; // 0.0 to 1.0

    BlendNode(int id, int inputPin1Id, int inputPin2Id, int outputPinId) 
        : Node(id, "Blend") {
        // Input pin 1 (Base Image)
        Pin inPin1(inputPin1Id, "Base", PinKind::Input);
        inPin1.node = this;
        inputPins.push_back(inPin1);

        // Input pin 2 (Blend Image)
        Pin inPin2(inputPin2Id, "Blend", PinKind::Input);
        inPin2.node = this;
        inputPins.push_back(inPin2);

        // Output pin
        Pin outPin(outputPinId, "Output", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    cv::Mat applyBlend(const cv::Mat& base, const cv::Mat& blend) {
        cv::Mat result;
        
        switch (blendMode) {
            case 0: { // Normal
                cv::addWeighted(base, 1.0 - opacity, blend, opacity, 0.0, result);
                break;
            }
                
            case 1: { // Multiply
                cv::multiply(base, blend, result, 1.0/255.0);
                if (opacity < 1.0f) {
                    cv::addWeighted(base, 1.0 - opacity, result, opacity, 0.0, result);
                }
                break;
            }
                
            case 2: { // Screen
                cv::Mat invBase, invBlend;
                cv::bitwise_not(base, invBase);
                cv::bitwise_not(blend, invBlend);
                cv::multiply(invBase, invBlend, result, 1.0/255.0);
                cv::bitwise_not(result, result);
                if (opacity < 1.0f) {
                    cv::addWeighted(base, 1.0 - opacity, result, opacity, 0.0, result);
                }
                break;
            }
                
            case 3: { // Overlay
                result = base.clone();
                for(int i = 0; i < base.rows; i++) {
                    for(int j = 0; j < base.cols; j++) {
                        for(int c = 0; c < 3; c++) {
                            float baseVal = base.at<cv::Vec3b>(i,j)[c] / 255.0f;
                            float blendVal = blend.at<cv::Vec3b>(i,j)[c] / 255.0f;
                            float val;
                            if(baseVal < 0.5f)
                                val = 2.0f * baseVal * blendVal;
                            else
                                val = 1.0f - 2.0f * (1.0f - baseVal) * (1.0f - blendVal);
                            result.at<cv::Vec3b>(i,j)[c] = cv::saturate_cast<uchar>(val * 255.0f);
                        }
                    }
                }
                if (opacity < 1.0f) {
                    cv::addWeighted(base, 1.0 - opacity, result, opacity, 0.0, result);
                }
                break;
            }
                
            case 4: { // Difference
                cv::absdiff(base, blend, result);
                if (opacity < 1.0f) {
                    cv::addWeighted(base, 1.0 - opacity, result, opacity, 0.0, result);
                }
                break;
            }
        }
        return result;
    }

    void process() override {
        printf("Processing Blend node %d\n", id);

        // Clear output
        if (!outputPins.empty()) {
            outputPins[0].imageData.release();
        }

        // Check for both input images
        if (inputPins.size() < 2) {
            printf("  Error: Blend node requires two inputs\n");
            return;
        }

        cv::Mat baseImage = GetInputImageData(inputPins[0]);
        cv::Mat blendImage = GetInputImageData(inputPins[1]);

        if (baseImage.empty() || blendImage.empty()) {
            printf("  Error: One or both input images missing\n");
            return;
        }

        // Ensure both images are the same size
        if (baseImage.size() != blendImage.size()) {
            cv::resize(blendImage, blendImage, baseImage.size());
        }

        // Ensure both images are BGR
        if (baseImage.channels() == 1) {
            cv::cvtColor(baseImage, baseImage, cv::COLOR_GRAY2BGR);
        }
        if (blendImage.channels() == 1) {
            cv::cvtColor(blendImage, blendImage, cv::COLOR_GRAY2BGR);
        }

        // Apply blend
        outputPins[0].imageData = applyBlend(baseImage, blendImage);
        printf("  Blend applied successfully.\n");
    }
};