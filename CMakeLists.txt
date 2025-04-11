# Minimum CMake version required
cmake_minimum_required(VERSION 3.15)

# Project Name - MUST be after cmake_minimum_required()
project(NodeImageProcessor LANGUAGES CXX)

# Set C++ standard (e.g., C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Optional: Enforce standard C++

# --- Find Required Libraries ---

# Find OpenCV (installed via pacman)
find_package(OpenCV REQUIRED)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found. Make sure it's installed via pacman (mingw-w64-x86_64-opencv). Check CMake output for details.")
endif()

# Find GLFW (installed via pacman)
find_package(glfw3 REQUIRED) # Note: Use 'glfw3' for the find_package command
if(NOT glfw3_FOUND)
    message(FATAL_ERROR "GLFW3 not found. Make sure it's installed via pacman (mingw-w64-x86_64-glfw). Check CMake output for details.")
endif()

# Find OpenGL (system library)
find_package(OpenGL REQUIRED)
if(NOT OpenGL_FOUND)
    message(FATAL_ERROR "OpenGL not found.")
endif()

# --- Add Dear ImGui Source Files ---
# Define the path to the ImGui library source code within your project
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui) # Assumes imgui source is in project_root/libs/imgui (Use v1.88 for this setup)

# List the ImGui source files we need to compile
# Core ImGui files
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp  # Include demo for debugging ImGui itself if needed
)

# ImGui Backend files (GLFW + OpenGL3)
set(IMGUI_BACKEND_SOURCES
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# --- Add ImNodes Source Files --- [Using ImNodes]
set(IMNODES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/imnodes)
set(IMNODES_SOURCES
    ${IMNODES_DIR}/imnodes.cpp
)

# --- Define Executable Target ---
# Add your main application source file(s) and sources from libraries
add_executable(NodeImageProcessor main.cpp
    ${IMGUI_SOURCES}
    ${IMGUI_BACKEND_SOURCES}
    ${IMNODES_SOURCES}       # <-- Use ImNodes sources
)

# --- Include Directories ---
# Tell the target where to find header files (.h files)
target_include_directories(NodeImageProcessor PUBLIC
    ${OpenCV_INCLUDE_DIRS}    # OpenCV headers
    ${IMGUI_DIR}              # ImGui core headers
    ${IMGUI_DIR}/backends     # ImGui backend headers
    ${IMNODES_DIR}            # <-- Use ImNodes include directory
)

# --- Link Libraries ---
# Link the executable against the libraries it needs
target_link_libraries(NodeImageProcessor PUBLIC
    ${OpenCV_LIBS}          # OpenCV libraries (found by find_package)
    glfw                    # GLFW library target provided by find_package(glfw3)
    OpenGL::GL              # Modern OpenGL target provided by find_package(OpenGL)
)

# Platform-specific linking for GLFW on Windows/MinGW
# GLFW needs gdi32 on Windows
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
   target_link_libraries(NodeImageProcessor PUBLIC gdi32)
endif()

# Optional: Define preprocessor directive needed by imgui_impl_opengl3
# Prevents needing GLAD if using system OpenGL headers directly
target_compile_definitions(NodeImageProcessor PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD=0)

# --- Output Messages ---
message(STATUS "CMake configured for NodeImageProcessor")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Using OpenCV: ${OpenCV_VERSION} (Include: ${OpenCV_INCLUDE_DIRS})")
message(STATUS "  Using GLFW3: ${glfw3_VERSION_STRING} (Found: ${glfw3_FOUND})")
message(STATUS "  Using OpenGL: ${OPENGL_FOUND} (Version: ${OPENGL_GL_VERSION_STRING})")
message(STATUS "  Using ImGui from: ${IMGUI_DIR}")
message(STATUS "  Using ImNodes from: ${IMNODES_DIR}") # Updated message