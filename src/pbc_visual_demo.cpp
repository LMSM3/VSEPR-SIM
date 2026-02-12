/*
 * PBC Visual Demo - Interactive visualization of Periodic Boundary Conditions
 * Demonstrates:
 *   - Particle wrapping across boundaries
 *   - Minimum Image Convention (MIC)
 *   - FCC crystal lattice
 *   - Gas vs Solid phase
 */

#include "box/pbc.hpp"
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <random>

using namespace vsepr;

struct Particle {
    double x, y, z;
    double vx, vy, vz;
    float r, g, b;  // Color
};

enum DemoMode {
    MODE_GAS,           // Random gas particles
    MODE_BOUNDARY,      // Two particles across boundary
    MODE_FCC_LATTICE,   // Crystal structure
    MODE_WRAPPING       // Particles wrapping around
};

class PBCVisualDemo {
private:
    GLFWwindow* window_;
    BoxOrtho box_;
    std::vector<Particle> particles_;
    DemoMode mode_;
    double time_;
    bool paused_;
    bool show_box_;
    
public:
    PBCVisualDemo(double box_size = 20.0) 
        : window_(nullptr),
          box_(box_size, box_size, box_size),
          mode_(MODE_GAS),
          time_(0.0),
          paused_(false),
          show_box_(true)
    {
        init_window();
        setup_gas_phase();
    }
    
    ~PBCVisualDemo() {
        if (window_) {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
    }
    
    void init_window() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        
        window_ = glfwCreateWindow(1200, 900, "PBC Visualization Demo", nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);  // VSync
        
        // Setup OpenGL
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        
        GLfloat light_pos[] = {10.0f, 20.0f, 10.0f, 1.0f};
        GLfloat light_ambient[] = {0.3f, 0.3f, 0.3f, 1.0f};
        GLfloat light_diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
        
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        
        // Setup viewport
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        glViewport(0, 0, width, height);
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)width / height, 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW);
    }
    
    void setup_gas_phase() {
        particles_.clear();
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> pos(2.0, 18.0);
        std::uniform_real_distribution<double> vel(-0.5, 0.5);
        
        // Create 32 gas particles
        for (int i = 0; i < 32; ++i) {
            Particle p;
            p.x = pos(rng);
            p.y = pos(rng);
            p.z = pos(rng);
            p.vx = vel(rng);
            p.vy = vel(rng);
            p.vz = vel(rng);
            p.r = 0.3f;
            p.g = 0.7f;
            p.b = 1.0f;
            particles_.push_back(p);
        }
        std::cout << "GAS PHASE: 32 particles, low density\n";
    }
    
    void setup_boundary_crossing() {
        particles_.clear();
        
        // Two particles close across boundary
        Particle p1, p2;
        p1.x = 19.5; p1.y = 10.0; p1.z = 10.0;
        p1.vx = 0.2; p1.vy = 0.0; p1.vz = 0.0;
        p1.r = 1.0f; p1.g = 0.2f; p1.b = 0.2f;
        
        p2.x = 0.5; p2.y = 10.0; p2.z = 10.0;
        p2.vx = -0.2; p2.vy = 0.0; p2.vz = 0.0;
        p2.r = 0.2f; p2.g = 1.0f; p2.b = 0.2f;
        
        particles_.push_back(p1);
        particles_.push_back(p2);
        
        std::cout << "BOUNDARY CROSSING: 2 particles at x=19.5 and x=0.5\n";
        std::cout << "MIC distance should be 1.0 Å (not 19.0 Å!)\n";
    }
    
    void setup_fcc_lattice() {
        particles_.clear();
        
        // 4×4×4 FCC lattice
        double a = 4.0;  // Lattice constant
        int n = 4;
        
        for (int ix = 0; ix < n; ++ix) {
            for (int iy = 0; iy < n; ++iy) {
                for (int iz = 0; iz < n; ++iz) {
                    double x0 = ix * a + 2.0;
                    double y0 = iy * a + 2.0;
                    double z0 = iz * a + 2.0;
                    
                    // Basis atoms
                    auto add_atom = [&](double dx, double dy, double dz) {
                        Particle p;
                        p.x = x0 + dx;
                        p.y = y0 + dy;
                        p.z = z0 + dz;
                        p.vx = 0.0;
                        p.vy = 0.0;
                        p.vz = 0.0;
                        
                        // Color by layer
                        float hue = (float)iz / n;
                        p.r = 0.5f + 0.5f * std::sin(hue * 6.28f);
                        p.g = 0.5f + 0.5f * std::sin(hue * 6.28f + 2.09f);
                        p.b = 0.5f + 0.5f * std::sin(hue * 6.28f + 4.19f);
                        particles_.push_back(p);
                    };
                    
                    add_atom(0.0, 0.0, 0.0);
                    add_atom(0.5*a, 0.5*a, 0.0);
                    add_atom(0.5*a, 0.0, 0.5*a);
                    add_atom(0.0, 0.5*a, 0.5*a);
                }
            }
        }
        
        std::cout << "FCC LATTICE: " << particles_.size() << " atoms\n";
        std::cout << "Lattice constant: 4.0 Å, high density\n";
    }
    
    void setup_wrapping_demo() {
        particles_.clear();
        
        // Line of particles moving fast to show wrapping
        for (int i = 0; i < 10; ++i) {
            Particle p;
            p.x = 2.0 + i * 1.8;
            p.y = 10.0;
            p.z = 10.0;
            p.vx = 1.5 + i * 0.1;
            p.vy = 0.0;
            p.vz = 0.0;
            
            float t = (float)i / 10.0f;
            p.r = 1.0f - t;
            p.g = t;
            p.b = 0.5f;
            particles_.push_back(p);
        }
        
        std::cout << "WRAPPING DEMO: 10 particles moving fast\n";
        std::cout << "Watch them wrap around the boundary!\n";
    }
    
    void update(double dt) {
        if (paused_) return;
        
        time_ += dt;
        
        // Update particle positions
        for (auto& p : particles_) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.z += p.vz * dt;
            
            // Apply PBC wrapping
            Vec3 pos{p.x, p.y, p.z};
            pos = box_.wrap(pos);
            p.x = pos.x;
            p.y = pos.y;
            p.z = pos.z;
        }
    }
    
    void draw_box_wireframe() {
        if (!show_box_) return;
        
        double Lx = box_.L.x;
        double Ly = box_.L.y;
        double Lz = box_.L.z;
        
        glColor3f(0.5f, 0.5f, 0.5f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        
        // Bottom face
        glVertex3d(0, 0, 0); glVertex3d(Lx, 0, 0);
        glVertex3d(Lx, 0, 0); glVertex3d(Lx, Ly, 0);
        glVertex3d(Lx, Ly, 0); glVertex3d(0, Ly, 0);
        glVertex3d(0, Ly, 0); glVertex3d(0, 0, 0);
        
        // Top face
        glVertex3d(0, 0, Lz); glVertex3d(Lx, 0, Lz);
        glVertex3d(Lx, 0, Lz); glVertex3d(Lx, Ly, Lz);
        glVertex3d(Lx, Ly, Lz); glVertex3d(0, Ly, Lz);
        glVertex3d(0, Ly, Lz); glVertex3d(0, 0, Lz);
        
        // Vertical edges
        glVertex3d(0, 0, 0); glVertex3d(0, 0, Lz);
        glVertex3d(Lx, 0, 0); glVertex3d(Lx, 0, Lz);
        glVertex3d(Lx, Ly, 0); glVertex3d(Lx, Ly, Lz);
        glVertex3d(0, Ly, 0); glVertex3d(0, Ly, Lz);
        
        glEnd();
    }
    
    void draw_particles() {
        for (const auto& p : particles_) {
            glPushMatrix();
            glTranslated(p.x, p.y, p.z);
            glColor3f(p.r, p.g, p.b);
            
            // Draw sphere (simplified as octahedron for speed)
            double r = (mode_ == MODE_FCC_LATTICE) ? 0.8 : 0.5;
            
            GLUquadric* quad = gluNewQuadric();
            gluSphere(quad, r, 16, 16);
            gluDeleteQuadric(quad);
            
            glPopMatrix();
        }
    }
    
    void draw_info_text() {
        // Mode info
        const char* mode_str = "";
        switch (mode_) {
            case MODE_GAS: mode_str = "GAS PHASE - Low density, random motion"; break;
            case MODE_BOUNDARY: mode_str = "BOUNDARY CROSSING - MIC demonstration"; break;
            case MODE_FCC_LATTICE: mode_str = "FCC CRYSTAL - High density solid"; break;
            case MODE_WRAPPING: mode_str = "WRAPPING DEMO - PBC in action"; break;
        }
        
        std::cout << "\r" << mode_str << " | Particles: " << particles_.size() 
                  << " | Time: " << (int)time_ << "s " 
                  << (paused_ ? "[PAUSED]" : "[RUNNING]") << "          " << std::flush;
    }
    
    void handle_keys() {
        if (glfwGetKey(window_, GLFW_KEY_1) == GLFW_PRESS) {
            mode_ = MODE_GAS;
            setup_gas_phase();
            time_ = 0.0;
        }
        if (glfwGetKey(window_, GLFW_KEY_2) == GLFW_PRESS) {
            mode_ = MODE_BOUNDARY;
            setup_boundary_crossing();
            time_ = 0.0;
        }
        if (glfwGetKey(window_, GLFW_KEY_3) == GLFW_PRESS) {
            mode_ = MODE_FCC_LATTICE;
            setup_fcc_lattice();
            time_ = 0.0;
        }
        if (glfwGetKey(window_, GLFW_KEY_4) == GLFW_PRESS) {
            mode_ = MODE_WRAPPING;
            setup_wrapping_demo();
            time_ = 0.0;
        }
        if (glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS) {
            static bool space_was_pressed = false;
            if (!space_was_pressed) {
                paused_ = !paused_;
                space_was_pressed = true;
            }
        } else {
            static bool space_was_pressed = false;
            space_was_pressed = false;
        }
        if (glfwGetKey(window_, GLFW_KEY_B) == GLFW_PRESS) {
            static bool b_was_pressed = false;
            if (!b_was_pressed) {
                show_box_ = !show_box_;
                b_was_pressed = true;
            }
        } else {
            static bool b_was_pressed = false;
            b_was_pressed = false;
        }
    }
    
    void run() {
        double last_time = glfwGetTime();
        double camera_angle = 0.0;
        double camera_distance = 40.0;
        
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  PBC VISUALIZATION DEMO\n";
        std::cout << "========================================\n";
        std::cout << "Controls:\n";
        std::cout << "  1 - Gas Phase (32 particles)\n";
        std::cout << "  2 - Boundary Crossing (2 particles)\n";
        std::cout << "  3 - FCC Crystal (256 atoms)\n";
        std::cout << "  4 - Wrapping Demo (10 particles)\n";
        std::cout << "  SPACE - Pause/Resume\n";
        std::cout << "  B - Toggle box wireframe\n";
        std::cout << "  ESC - Exit\n";
        std::cout << "========================================\n\n";
        
        while (!glfwWindowShouldClose(window_)) {
            double current_time = glfwGetTime();
            double dt = current_time - last_time;
            last_time = current_time;
            
            // Limit dt to prevent huge jumps
            if (dt > 0.1) dt = 0.1;
            
            handle_keys();
            update(dt);
            
            // Setup camera
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glLoadIdentity();
            
            camera_angle += 0.2 * dt;
            double cam_x = camera_distance * std::sin(camera_angle);
            double cam_z = camera_distance * std::cos(camera_angle);
            double cam_y = 15.0;
            
            gluLookAt(cam_x + 10, cam_y, cam_z + 10,  // Eye
                      10, 10, 10,                        // Center (box center)
                      0, 1, 0);                          // Up
            
            // Draw
            draw_box_wireframe();
            draw_particles();
            draw_info_text();
            
            glfwSwapBuffers(window_);
            glfwPollEvents();
        }
        
        std::cout << "\n\nDemo completed.\n";
    }
};

int main() {
    try {
        PBCVisualDemo demo(20.0);
        demo.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
