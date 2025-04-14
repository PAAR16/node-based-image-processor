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
    size_t fileSize = 0;  // Add file size member

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
        fileSize = 0;  // Reset file size

        if (!filePath.empty()) {
            printf("  Attempting to load image: %s\n", filePath.c_str());

            // Get file size
            FILE* fp = fopen(filePath.c_str(), "rb");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                fileSize = ftell(fp);
                fclose(fp);
            }

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
                printf("  Loaded image: %s (%dx%d) Format: %s Size: %.2f MB\n", 
                    filePath.c_str(), imgWidth, imgHeight, imgFormat.c_str(), fileSize / (1024.0 * 1024.0));
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
                printf("  Output node did not receive image data from pin %d.\n");
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
    // Default values as static constants
    static constexpr float DEFAULT_BRIGHTNESS = 0.0f;
    static constexpr float DEFAULT_CONTRAST = 1.0f;
    float brightness = DEFAULT_BRIGHTNESS;
    float contrast = DEFAULT_CONTRAST;

    BrightnessContrastNode(int id, int inputPinId, int outputPinId) : Node(id, "Brightness/Contrast") {
        Pin inPin(inputPinId, "Image In", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);

        Pin outPin(outputPinId, "Image Out", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    void reset() {
        brightness = DEFAULT_BRIGHTNESS;
        contrast = DEFAULT_CONTRAST;
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
    static constexpr int DEFAULT_RADIUS = 3;
    static constexpr float DEFAULT_ANGLE = 0.0f;
    int radius = DEFAULT_RADIUS;
    bool directionalBlur = false;
    float angle = DEFAULT_ANGLE;
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

    void reset() {
        radius = DEFAULT_RADIUS;
        directionalBlur = false;
        angle = DEFAULT_ANGLE;
        process();
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
    static constexpr int DEFAULT_THRESHOLD = 127;
    static constexpr int DEFAULT_BLOCK_SIZE = 11;
    static constexpr float DEFAULT_C = 2.0f;
    
    int thresholdValue = DEFAULT_THRESHOLD;
    int thresholdMethod = 0;   // 0=Binary, 1=Adaptive, 2=Otsu
    int blockSize = DEFAULT_BLOCK_SIZE;
    float C = DEFAULT_C;
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

    void reset() {
        thresholdValue = DEFAULT_THRESHOLD;
        blockSize = DEFAULT_BLOCK_SIZE;
        C = DEFAULT_C;
        process();
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


// --- Noise Generation Node ---
struct NoiseGenerationNode : public Node {
    int noiseType = 0;      // 0=Perlin, 1=Simplex, 2=Worley
    float scale = 50.0f;    // Noise scale
    int octaves = 4;        // Number of octaves (1-8)
    float persistence = 0.5f;// How much each octave contributes
    int width = 512;        // Output image width
    int height = 512;       // Output image height
    bool useAsDisplacement = false; // Displacement map or direct output
    
    NoiseGenerationNode(int id, int outputPinId) : Node(id, "Noise Generator") {
        Pin outPin(outputPinId, "Output", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);
    }

    float perlinNoise(float x, float y) {
        // Basic Perlin noise implementation
        int X = (int)floor(x) & 255;
        int Y = (int)floor(y) & 255;
        x -= floor(x);
        y -= floor(y);
        float u = fade(x);
        float v = fade(y);
        int A = p[X]+Y;
        int B = p[X+1]+Y;
        return lerp(v, lerp(u, grad(p[A], x, y), 
                              grad(p[B], x-1, y)),
                      lerp(u, grad(p[A+1], x, y-1),
                              grad(p[B+1], x-1, y-1)));
    }

    void process() override {
        printf("Processing Noise Generation node %d\n", id);
        
        cv::Mat noiseImage(height, width, CV_8UC3);
        
        for(int y = 0; y < height; y++) {
            for(int x = 0; x < width; x++) {
                float nx = x * scale / width;
                float ny = y * scale / height;
                float value = 0.0f;
                float amplitude = 1.0f;
                float frequency = 1.0f;
                float maxValue = 0.0f;
                
                // Generate octaves
                for(int i = 0; i < octaves; i++) {
                    value += amplitude * perlinNoise(nx * frequency, ny * frequency);
                    maxValue += amplitude;
                    amplitude *= persistence;
                    frequency *= 2.0f;
                }
                
                value = value / maxValue;
                value = (value + 1.0f) * 0.5f; // Normalize to 0-1
                uchar pixelValue = static_cast<uchar>(value * 255);
                
                if (useAsDisplacement) {
                    // Create displacement effect (grayscale)
                    noiseImage.at<cv::Vec3b>(y, x) = cv::Vec3b(pixelValue, pixelValue, pixelValue);
                } else {
                    // Create colored noise
                    noiseImage.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uchar>(value * 255),
                        static_cast<uchar>((1-value) * 255),
                        static_cast<uchar>(value * 128 + 64)
                    );
                }
            }
        }
        
        outputPins[0].imageData = noiseImage;
    }

private:
    static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    static float lerp(float t, float a, float b) { return a + t * (b - a); }
    static float grad(int hash, float x, float y) {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }
    static const int p[512]; // Permutation table (defined elsewhere)
};

// Add this definition outside the class
const int NoiseGenerationNode::p[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
    // Repeat the array to avoid overflow
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};


// --- Convolution Filter Node ---
struct ConvolutionFilterNode : public Node {
    static const int MAX_KERNEL_SIZE = 5;
    int kernelSize = 3;  // 3 or 5
    std::vector<float> kernel;
    int presetIndex = 0;
    GLuint previewTexId = 0;

    ConvolutionFilterNode(int id, int inputPinId, int outputPinId) 
        : Node(id, "Convolution Filter") {
        Pin inPin(inputPinId, "Input", PinKind::Input);
        inPin.node = this;
        inputPins.push_back(inPin);

        Pin outPin(outputPinId, "Output", PinKind::Output);
        outPin.node = this;
        outputPins.push_back(outPin);

        // Initialize kernel with identity matrix
        kernel.resize(MAX_KERNEL_SIZE * MAX_KERNEL_SIZE, 0.0f);
        kernel[MAX_KERNEL_SIZE * MAX_KERNEL_SIZE / 2] = 1.0f;
        
        updatePreset(0); // Initialize with first preset
    }

    ~ConvolutionFilterNode() override {
        if (previewTexId != 0) {
            glDeleteTextures(1, &previewTexId);
        }
    }

    void updatePreset(int index) {
        kernel.assign(MAX_KERNEL_SIZE * MAX_KERNEL_SIZE, 0.0f);
        
        switch(index) {
            case 0: // Identity
                kernel[12] = 1.0f;
                break;
            case 1: // Sharpen
                kernel[7] = -1.0f;
                kernel[11] = -1.0f;
                kernel[12] = 5.0f;
                kernel[13] = -1.0f;
                kernel[17] = -1.0f;
                break;
            case 2: // Emboss
                kernel[6] = -2.0f;
                kernel[7] = -1.0f;
                kernel[11] = -1.0f;
                kernel[12] = 1.0f;
                kernel[13] = 1.0f;
                kernel[17] = 1.0f;
                kernel[18] = 2.0f;
                break;
            case 3: // Edge Enhance
                kernel[7] = 1.0f;
                kernel[11] = 1.0f;
                kernel[12] = -4.0f;
                kernel[13] = 1.0f;
                kernel[17] = 1.0f;
                break;
        }
        updateKernelPreview();
    }

    void updateKernelPreview() {
        int previewSize = 100;
        cv::Mat preview(previewSize, previewSize, CV_8UC4, cv::Scalar(0,0,0,255));
        
        float minVal = *std::min_element(kernel.begin(), kernel.end());
        float maxVal = *std::max_element(kernel.begin(), kernel.end());
        float range = maxVal - minVal;
        
        int cellSize = previewSize / kernelSize;
        for(int i = 0; i < kernelSize; i++) {
            for(int j = 0; j < kernelSize; j++) {
                float value = kernel[i * MAX_KERNEL_SIZE + j];
                uchar intensity = static_cast<uchar>((value - minVal) * 255 / range);
                cv::rectangle(preview, 
                    cv::Point(j * cellSize, i * cellSize),
                    cv::Point((j + 1) * cellSize, (i + 1) * cellSize),
                    cv::Scalar(intensity, intensity, intensity, 255),
                    -1);
            }
        }
        
        // Update OpenGL texture
        if (previewTexId == 0) {
            glGenTextures(1, &previewTexId);
        }
        
        glBindTexture(GL_TEXTURE_2D, previewTexId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, preview.cols, preview.rows, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, preview.data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void process() override {
        printf("Processing Convolution Filter node %d\n", id);

        if (!outputPins.empty()) {
            outputPins[0].imageData.release();
        }

        cv::Mat inputImage = GetInputImageData(inputPins[0]);
        if (inputImage.empty()) {
            printf("  No input image available.\n");
            return;
        }

        // Create kernel matrix
        cv::Mat kernelMat(kernelSize, kernelSize, CV_32F);
        for(int i = 0; i < kernelSize; i++) {
            for(int j = 0; j < kernelSize; j++) {
                kernelMat.at<float>(i,j) = kernel[i * MAX_KERNEL_SIZE + j];
            }
        }

        // Apply convolution
        cv::Mat result;
        cv::filter2D(inputImage, result, -1, kernelMat);
        outputPins[0].imageData = result;

        printf("  Convolution applied successfully.\n");
    }
};