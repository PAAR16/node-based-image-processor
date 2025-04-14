#pragma once

#include <vector>
#include <map>
#include <string>
#include <imgui.h>
#include <opencv2/opencv.hpp>
#include <optional>
#include <set>
#include <algorithm>
#include <unordered_map> // NEW: Added for unordered_map

// Forward Declarations (to avoid circular includes)
struct Node;

// --- Pin/Attribute Structure ---
// Represents an input or output point on a node
enum class PinKind {
    Input,
    Output
};

struct Pin {
    int id;         // Unique ID for ImNodes
    Node* node;     // Node this pin belongs to
    PinKind kind;
    std::string name; // Display name (e.g., "Image In", "Result Out")
    cv::Mat imageData; // NEW: Storage for image data

    // Constructor (optional but helpful)
    Pin(int id_ = 0, const char* name_ = "Pin", PinKind kind_ = PinKind::Input) :
        id(id_), node(nullptr), kind(kind_), name(name_) {}
};


// --- Link Structure ---
// Represents a connection between two pins
struct Link {
    int id;             // Unique ID for ImNodes
    int startPinId;
    int endPinId;

    // Constructor (optional)
    Link(int id_ = 0, int start_ = 0, int end_ = 0) :
        id(id_), startPinId(start_), endPinId(end_) {}
};


// --- Node Structure (Base) ---
// Base structure for all node types
struct Node {
    int id;                 // Unique ID for ImNodes
    std::string name;
    std::vector<Pin> inputPins;
    std::vector<Pin> outputPins;
    ImVec2 graphPosition; // Store node position on the canvas (needed for saving/loading)
    bool isDirty = true; // NEW: Flag to indicate if the node needs processing

    // We need a way to process the node's function
    // Virtual function to be overridden by derived node types
    virtual void process() {
        // Base implementation does nothing
        printf("Processing node: %s (ID: %d) - Base Implementation\n", name.c_str(), id);
    }

    bool needsProcessing() const {
        if (isDirty) return true;
        
        // Check if any input has changed
        for (const Pin& pin : inputPins) {
            if (pin.imageData.empty()) return true;
            
            // Compare with cached input state
            auto it = lastInputState.find(pin.id);
            if (it == lastInputState.end()) return true;
            
            const cv::Mat& cached = it->second; // Make it const
            // Use matrix comparison instead of equals
            if (!cached.data || !cv::countNonZero(pin.imageData == cached)) {
                return true;
            }
        }
        return false;
    }

    void updateInputCache() {
        lastInputState.clear();
        for (const Pin& pin : inputPins) {
            if (!pin.imageData.empty()) {
                lastInputState[pin.id] = pin.imageData.clone();
            }
        }
    }

    // Virtual destructor is important for base classes with virtual functions
    virtual ~Node() = default;

protected:
    // Protected constructor for base class
    Node(int id_ = 0, const char* name_ = "Node") : id(id_), name(name_), graphPosition(0,0) {}

    // Helper to add pins during construction of derived nodes
    void addPin(int pinId, const char* pinName, PinKind kind) {
        Pin newPin(pinId, pinName, kind);
        newPin.node = this; // Link pin back to this node
        if (kind == PinKind::Input) {
            inputPins.push_back(newPin);
        } else {
            outputPins.push_back(newPin);
        }
    }

    std::unordered_map<int, cv::Mat> lastInputState; // NEW: Cache for input state
};


// --- Graph Structure ---
// Holds all the nodes and links
class NodeGraph {
public:
    std::vector<Node*> nodes;
    std::vector<Link> links;
    int nextNodeId = 1;
    int nextPinId = 1;
    int nextLinkId = 1; // Add missing link ID counter

    std::vector<Node*> getExecutionOrder() {
        std::vector<Node*> executionOrder;
        std::set<Node*> visited;
        std::set<Node*> processing;

        // Find output nodes (nodes with no output connections)
        for (Node* node : nodes) {
            bool hasOutputConnection = false;
            for (const Link& link : links) {
                for (const Pin& pin : node->outputPins) {
                    if (link.startPinId == pin.id) {
                        hasOutputConnection = true;
                        break;
                    }
                }
            }
            if (!hasOutputConnection) {
                topologicalSort(node, visited, processing, executionOrder);
            }
        }

        std::reverse(executionOrder.begin(), executionOrder.end());
        return executionOrder;
    }

    void executeGraph() {
        if (hasCircularDependency()) {
            fprintf(stderr, "Error: Circular dependency detected in node graph\n");
            return;
        }

        std::vector<Node*> executionOrder = getExecutionOrder();
        for (Node* node : executionOrder) {
            if (node->needsProcessing()) {
                printf("Processing node: %s (ID: %d)\n", node->name.c_str(), node->id);
                node->process();
                node->updateInputCache();
            } else {
                printf("Using cached result for node: %s (ID: %d)\n", node->name.c_str(), node->id);
            }
        }
    }

private:
    void topologicalSort(Node* node, std::set<Node*>& visited, 
                        std::set<Node*>& processing,
                        std::vector<Node*>& order) {
        if (processing.find(node) != processing.end()) {
            // Circular dependency found
            return;
        }

        if (visited.find(node) != visited.end()) {
            return;
        }

        processing.insert(node);

        // Process input dependencies first
        for (const Pin& pin : node->inputPins) {
            for (const Link& link : links) {
                if (link.endPinId == pin.id) {
                    Node* sourceNode = FindNodeByPinId(link.startPinId);
                    if (sourceNode) {
                        topologicalSort(sourceNode, visited, processing, order);
                    }
                }
            }
        }

        processing.erase(node);
        visited.insert(node);
        order.push_back(node);
    }

    Node* FindNodeByPinId(int pinId) {
        for (Node* node : nodes) {
            for (const Pin& pin : node->outputPins) {
                if (pin.id == pinId) {
                    return node;
                }
            }
        }
        return nullptr;
    }

    bool hasCircularDependency() {
        std::set<Node*> visited;
        std::set<Node*> processing;

        for (Node* node : nodes) {
            if (detectCycle(node, visited, processing)) {
                return true;
            }
        }
        return false;
    }

    bool detectCycle(Node* node, std::set<Node*>& visited, std::set<Node*>& processing) {
        if (processing.find(node) != processing.end()) {
            return true;
        }

        if (visited.find(node) != visited.end()) {
            return false;
        }

        processing.insert(node);

        for (const Pin& pin : node->inputPins) {
            for (const Link& link : links) {
                if (link.endPinId == pin.id) {
                    Node* sourceNode = FindNodeByPinId(link.startPinId);
                    if (sourceNode && detectCycle(sourceNode, visited, processing)) {
                        return true;
                    }
                }
            }
        }

        processing.erase(node);
        visited.insert(node);
        return false;
    }
};

// --- NEW: Helper Function Declaration ---
std::optional<cv::Mat> GetInputImageData(const NodeGraph& graph, int inputPinId);

// --- NEW: Helper Function Definition ---
std::optional<cv::Mat> GetInputImageData(const NodeGraph& graph, int inputPinId) {
    int sourceOutputPinId = -1;

    for (const auto& link : graph.links) {
        if (link.endPinId == inputPinId) {
            sourceOutputPinId = link.startPinId;
            break;
        }
    }

    if (sourceOutputPinId == -1) {
        return std::nullopt;
    }

    for (const auto& node : graph.nodes) {
        for (const auto& outputPin : node->outputPins) {
            if (outputPin.id == sourceOutputPinId) {
                if (!outputPin.imageData.empty()) {
                    return outputPin.imageData;
                } else {
                    return std::nullopt;
                }
            }
        }
    }

    return std::nullopt;
}

// --- NEW: Overload Function Definition ---
cv::Mat GetInputImageData(const Pin& inputPin) {
    if (inputPin.kind != PinKind::Input || !inputPin.node) {
        return cv::Mat();
    }

    // Get access to the graph through the global variable
    extern NodeGraph g_Graph;

    // Find the connected output pin through links
    for (const auto& link : g_Graph.links) {
        if (link.endPinId == inputPin.id) {
            // Find the source node and its output pin
            for (const auto& node : g_Graph.nodes) {
                for (const auto& outputPin : node->outputPins) {
                    if (outputPin.id == link.startPinId) {
                        return outputPin.imageData;
                    }
                }
            }
        }
    }
    
    return cv::Mat();
}