/**
 * Native OpenGL Molecule Viewer - GLEW/GLFW Implementation
 * 
 * Triple Output System: Native GL Path
 * Reads XYZ files and renders stick-and-ball molecular structures
 * 
 * Compile:
 *   g++ -std=c++17 gl_molecule_viewer.cpp -o gl_viewer.exe -lglfw3 -lglew32 -lopengl32
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <map>

// OpenGL headers
#ifdef _WIN32
    #include <GL/glew.h>
    #include <GLFW/glfw3.h>
#else
    #include <GL/glew.h>
    #include <GLFW/glfw3.h>
#endif

// Atom structure
struct Atom {
    std::string symbol;
    float x, y, z;
    float color[3];  // RGB
    float radius;
};

// Element properties (CPK colors)
std::map<std::string, std::pair<float[3], float>> elementData = {
    {"H",  {{1.0f, 1.0f, 1.0f}, 0.4f}},   // White
    {"C",  {{0.5f, 0.5f, 0.5f}, 0.7f}},   // Gray
    {"N",  {{0.2f, 0.2f, 1.0f}, 0.65f}},  // Blue
    {"O",  {{1.0f, 0.2f, 0.2f}, 0.6f}},   // Red
    {"F",  {{0.0f, 1.0f, 0.0f}, 0.5f}},   // Green
    {"S",  {{1.0f, 1.0f, 0.0f}, 0.75f}},  // Yellow
    {"P",  {{1.0f, 0.0f, 1.0f}, 0.8f}},   // Magenta
    {"Cl", {{0.0f, 1.0f, 0.0f}, 0.7f}},   // Green
    {"Xe", {{0.0f, 1.0f, 1.0f}, 0.9f}}    // Cyan
};

// Load XYZ file
std::vector<Atom> loadXYZ(const std::string& filename) {
    std::vector<Atom> atoms;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << filename << std::endl;
        return atoms;
    }
    
    int numAtoms;
    std::string comment;
    file >> numAtoms;
    std::getline(file, comment); // Rest of first line
    std::getline(file, comment); // Comment line
    
    for (int i = 0; i < numAtoms; ++i) {
        Atom atom;
        file >> atom.symbol >> atom.x >> atom.y >> atom.z;
        
        // Assign element properties
        auto it = elementData.find(atom.symbol);
        if (it != elementData.end()) {
            atom.color[0] = it->second.first[0];
            atom.color[1] = it->second.first[1];
            atom.color[2] = it->second.first[2];
            atom.radius = it->second.second;
        } else {
            atom.color[0] = atom.color[1] = atom.color[2] = 0.8f; // Default gray
            atom.radius = 0.5f;
        }
        
        atoms.push_back(atom);
    }
    
    std::cout << "Loaded " << atoms.size() << " atoms from " << filename << std::endl;
    return atoms;
}

// Simple sphere rendering (using glutSolidSphere alternative - icosphere)
void drawSphere(float x, float y, float z, float radius, float color[3]) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glColor3fv(color);
    
    // Simple icosphere approximation (subdivided octahedron)
    const int slices = 16;
    const int stacks = 16;
    
    for (int i = 0; i < stacks; ++i) {
        float lat0 = M_PI * (-0.5f + (float)i / stacks);
        float lat1 = M_PI * (-0.5f + (float)(i + 1) / stacks);
        float z0 = radius * sinf(lat0);
        float z1 = radius * sinf(lat1);
        float r0 = radius * cosf(lat0);
        float r1 = radius * cosf(lat1);
        
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            float lng = 2 * M_PI * (float)j / slices;
            float x = cosf(lng);
            float y = sinf(lng);
            
            glNormal3f(x * r0, y * r0, z0);
            glVertex3f(x * r0, y * r0, z0);
            glNormal3f(x * r1, y * r1, z1);
            glVertex3f(x * r1, y * r1, z1);
        }
        glEnd();
    }
    
    glPopMatrix();
}

// Draw cylinder (bond)
void drawCylinder(float x1, float y1, float z1, float x2, float y2, float z2, float radius) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    float length = sqrtf(dx*dx + dy*dy + dz*dz);
    
    glPushMatrix();
    glTranslatef(x1, y1, z1);
    
    // Rotate to align with bond vector
    float angle = acosf(dz / length) * 180.0f / M_PI;
    float ax = -dy;
    float ay = dx;
    glRotatef(angle, ax, ay, 0);
    
    // Draw cylinder
    float color[3] = {0.0f, 1.0f, 0.5f}; // Green bonds
    glColor3fv(color);
    
    const int slices = 12;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= slices; ++i) {
        float theta = 2 * M_PI * (float)i / slices;
        float x = radius * cosf(theta);
        float y = radius * sinf(theta);
        
        glNormal3f(cosf(theta), sinf(theta), 0);
        glVertex3f(x, y, 0);
        glVertex3f(x, y, length);
    }
    glEnd();
    
    glPopMatrix();
}

// Camera controls
float cameraRotX = 0.0f;
float cameraRotY = 0.0f;
float cameraDistance = 10.0f;

void mouse_callback(GLFWwindow* window, int button, int action, int mods) {
    // Handle mouse input
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    cameraDistance *= (yoffset > 0) ? 0.9f : 1.1f;
    cameraDistance = std::max(2.0f, std::min(50.0f, cameraDistance));
}

// Main
int main(int argc, char** argv) {
    std::cout << "═══════════════════════════════════════════════════\n";
    std::cout << "  VSEPR-Sim Native OpenGL Molecule Viewer (GLEW)\n";
    std::cout << "  Triple Output System: Native GL Path\n";
    std::cout << "═══════════════════════════════════════════════════\n\n";
    
    // Check for XYZ file
    std::string filename = "../examples/molecules/c4o8.xyz";
    if (argc > 1) {
        filename = argv[1];
    }
    
    std::cout << "Loading: " << filename << "\n";
    std::vector<Atom> atoms = loadXYZ(filename);
    
    if (atoms.empty()) {
        std::cerr << "No atoms loaded. Exiting.\n";
        return 1;
    }
    
    // Initialize GLFW
    std::cout << "\n[1/4] Initializing GLFW..." << std::endl;
    if (!glfwInit()) {
        std::cerr << "✗ GLFW initialization failed\n";
        std::cerr << "\nFalling back to WebGL renderer...\n";
        std::cerr << "  → Open outputs/molecule_viewer.html\n";
        return 1;
    }
    std::cout << "✓ GLFW initialized\n";
    
    // Create window
    std::cout << "[2/4] Creating OpenGL window..." << std::endl;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "VSEPR-Sim Molecule Viewer", NULL, NULL);
    if (!window) {
        std::cerr << "✗ Window creation failed\n";
        std::cerr << "\nFalling back to WebGL renderer...\n";
        std::cerr << "  → Open outputs/molecule_viewer.html\n";
        glfwTerminate();
        return 1;
    }
    std::cout << "✓ Window created (1280x720)\n";
    
    glfwMakeContextCurrent(window);
    glfwSetScrollCallback(window, scroll_callback);
    
    // Initialize GLEW
    std::cout << "[3/4] Loading OpenGL extensions (GLEW)..." << std::endl;
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "✗ GLEW initialization failed: " << glewGetErrorString(err) << "\n";
        std::cerr << "\nThis is why Triple Output System exists!\n";
        std::cerr << "Falling back to WebGL renderer...\n";
        std::cerr << "  → Open outputs/molecule_viewer.html\n";
        glfwTerminate();
        return 1;
    }
    std::cout << "✓ GLEW initialized\n";
    std::cout << "  OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    
    // OpenGL setup
    std::cout << "[4/4] Configuring OpenGL state..." << std::endl;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    
    float lightPos[] = {10.0f, 10.0f, 10.0f, 1.0f};
    float lightAmb[] = {0.3f, 0.3f, 0.3f, 1.0f};
    float lightDiff[] = {0.8f, 0.8f, 0.8f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiff);
    
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    std::cout << "✓ OpenGL configured\n\n";
    
    std::cout << "═══════════════════════════════════════════════════\n";
    std::cout << "  Rendering " << atoms.size() << " atoms\n";
    std::cout << "  Controls:\n";
    std::cout << "    Scroll = Zoom\n";
    std::cout << "    ESC    = Exit\n";
    std::cout << "═══════════════════════════════════════════════════\n\n";
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Setup camera
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (float)width / height;
        
        // Perspective
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
        float f = 1.0f / tanf(fov * M_PI / 360.0f);
        float mat[16] = {
            f/aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, (farPlane+nearPlane)/(nearPlane-farPlane), -1,
            0, 0, (2*farPlane*nearPlane)/(nearPlane-farPlane), 0
        };
        glMultMatrixf(mat);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0, 0, -cameraDistance);
        glRotatef(cameraRotX, 1, 0, 0);
        glRotatef(cameraRotY, 0, 1, 0);
        
        // Auto-rotate
        cameraRotY += 0.5f;
        
        // Draw atoms
        for (const auto& atom : atoms) {
            drawSphere(atom.x, atom.y, atom.z, atom.radius, 
                      const_cast<float*>(atom.color));
        }
        
        // Draw bonds (distance-based)
        float bondColor[3] = {0.0f, 1.0f, 0.5f};
        const float bondThreshold = 2.0f;
        const float bondRadius = 0.1f;
        
        for (size_t i = 0; i < atoms.size(); ++i) {
            for (size_t j = i + 1; j < atoms.size(); ++j) {
                float dx = atoms[j].x - atoms[i].x;
                float dy = atoms[j].y - atoms[i].y;
                float dz = atoms[j].z - atoms[i].z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                
                if (dist < bondThreshold) {
                    drawCylinder(atoms[i].x, atoms[i].y, atoms[i].z,
                               atoms[j].x, atoms[j].y, atoms[j].z, bondRadius);
                }
            }
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
    }
    
    std::cout << "\nCleaning up...\n";
    glfwTerminate();
    std::cout << "✓ Native GL renderer closed successfully\n";
    
    return 0;
}
