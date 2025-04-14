# Technical Documentation

## Architecture Overview

### Core Components

1. **Node System** (`NodeGraph.h`)
   - Base `Node` class with virtual `process()`
   - Pin system for inputs/outputs
   - Link system for connections
   - Graph execution engine with caching

2. **Node Types** (`Nodes.h`)
   - Specialized node implementations
   - Image processing operations
   - Parameter management
   - Preview generation

3. **Application Core** (`main.cpp`)
   - ImGui/GLFW window management
   - Node editor canvas
   - Properties panel
   - Event handling

### Data Flow

1. **Image Processing Pipeline**
   ```
   Input Node → Processing Nodes → Output Node
   ```

2. **Node Execution**
   - Automatic dependency resolution
   - Cache invalidation on parameter changes
   - Preview generation for visual feedback

### Implementation Details

1. **Node Base Class**
   ```cpp
   struct Node {
       int id;
       std::string name;
       std::vector<Pin> inputPins;
       std::vector<Pin> outputPins;
       virtual void process() = 0;
   };
   ```

2. **Pin System**
   ```cpp
   struct Pin {
       int id;
       Node* node;
       PinKind kind;
       std::string name;
       cv::Mat imageData;
   };
   ```

3. **Link System**
   ```cpp
   struct Link {
       int id;
       int startPinId;
       int endPinId;
   };
   ```

## Performance Considerations

1. **Image Processing**
   - OpenCV operations on demand
   - Result caching until inputs change
   - Preview generation at display resolution

2. **Memory Management**
   - RAII for resource cleanup
   - OpenCV Mat reference counting
   - OpenGL texture management

3. **UI Responsiveness**
   - Asynchronous image loading
   - Efficient preview updates
   - Cached kernel previews

## Error Handling

1. **Connection Validation**
   - Type compatibility checking
   - Cycle detection
   - Single input enforcement

2. **Resource Management**
   - Image loading validation
   - Memory allocation checks
   - OpenGL error handling

## Future Improvements

1. **Planned Features**
   - Undo/redo system
   - Node grouping
   - Custom node creation
   - Project save/load

2. **Optimizations**
   - Multi-threading support
   - GPU acceleration
   - Memory pooling
   - Larger image handling