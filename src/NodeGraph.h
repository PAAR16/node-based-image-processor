#pragma once

#include <vector>
#include <map>
#include <string> // Also include string for std::string usage

#include <imgui.h> // <-- ADD THIS INCLUDE for ImVec2

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
    // We'll add data type information later (e.g., Image, Float)

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

    // We need a way to process the node's function
    // Virtual function to be overridden by derived node types
    virtual void process() {
        // Base implementation does nothing
        printf("Processing node: %s (ID: %d) - Base Implementation\n", name.c_str(), id);
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
};


// --- Graph Structure ---
// Holds all the nodes and links
struct NodeGraph {
    std::vector<Node*> nodes;
    std::vector<Link> links;
    int nextNodeId = 1; // Counter to generate unique IDs
    int nextPinId = 100; // Start pin IDs higher to avoid collision with node IDs
    int nextLinkId = 1000; // Start link IDs even higher

    // We'll add functions here later to add/remove nodes/links,
    // find nodes/pins by ID, and execute the graph.

    // Destructor to clean up nodes
    ~NodeGraph() {
        for (Node* node : nodes) {
            delete node; // Delete nodes allocated with 'new'
        }
        nodes.clear();
    }
};