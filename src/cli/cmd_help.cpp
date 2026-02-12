/**
 * cmd_help.cpp
 * -----------
 * Help command - display usage and available commands.
 */

#include "cmd_help.hpp"
#include "cli/display.hpp"
#include <iostream>

#ifdef BUILD_VISUALIZATION
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#endif

namespace vsepr {
namespace cli {

int HelpCommand::Execute(const std::vector<std::string>& /*args*/) {
    Display::Banner("VSEPR Molecular Simulation System", "Version 2.0.0");
    Display::BlankLine();
    
    Display::Subheader("About");
    std::cout << R"(
  VSEPR-Sim is a physics-first molecular simulation engine that predicts
  molecular geometry from first principles. Unlike machine learning approaches,
  all predictions emerge from explicit classical mechanics:
  
  • Bond stretching (harmonic potential)
  • Angle bending and VSEPR domain repulsion
  • Torsional barriers
  • Van der Waals nonbonded interactions
  
  The system uses gradient-based geometry optimization (FIRE algorithm) to
  relax molecular structures to their equilibrium configuration.
)";
    Display::BlankLine();
    
    Display::Subheader("Usage");
    std::cout << "  vsepr <command> [subcommand] [options]\n";
    Display::BlankLine();
    
    Display::Subheader("Available Commands");
    std::cout << "  build          Build molecules from chemical formulas\n"
              << "  optimize       Optimize molecular geometries\n"
              << "  energy         Calculate molecular energies\n"
              << "  therm          Analyze thermal properties and bonding\n"
              << "  test           Run validation tests\n"
              << "  help           Show this help message\n"
              << "  version        Show version information\n";
    Display::BlankLine();
    
    Display::Subheader("Quick Start");
    std::cout << "  \033[32m▶\033[0m  \033[1mvsepr build random --watch\033[0m  \033[32m← Try this first!\033[0m\n"
              << "     Generate random molecule with live 3D visualization\n"
              << "\n"
              << "  \033[36m▶\033[0m  \033[1mvsepr build discover --thermal\033[0m  \033[36m← Advanced!\033[0m\n"
              << "     Automated discovery: 100 combinations + HGST + thermal analysis\n"
              << "\n"
              << "  # Build water molecule and optimize geometry\n"
              << "  vsepr build H2O --optimize --output water.xyz\n"
              << "\n"
              << "  # Show help for a specific command\n"
              << "  vsepr build --help\n"
              << "\n"
              << "  # Run tests to verify installation\n"
              << "  vsepr test all\n";
    Display::BlankLine();
    
    Display::Subheader("Documentation");
    std::cout << "  Full documentation available at: docs/\n"
              << "    • QUICKSTART.md       - Get started in 5 minutes\n"
              << "    • ENERGY_MODEL.md     - Physics and energy terms\n"
              << "    • API.md              - Function reference\n";
    Display::BlankLine();
    
    // GPU/Graphics Information
    Display::Subheader("GPU Information");
#ifdef BUILD_VISUALIZATION
    // Initialize GLFW to query GPU info
    if (glfwInit()) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        GLFWwindow* window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
        
        if (window) {
            glfwMakeContextCurrent(window);
            glewExperimental = GL_TRUE;
            GLenum glew_status = glewInit();
            
            // Clear any GLEW init errors
            while (glGetError() != GL_NO_ERROR);
            
            // Try to get GPU info even if GLEW reports an error
            const GLubyte* vendor = glGetString(GL_VENDOR);
            const GLubyte* renderer = glGetString(GL_RENDERER);
            const GLubyte* version = glGetString(GL_VERSION);
            const GLubyte* glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);
            
            if (vendor && renderer && version) {
                std::cout << "  Vendor:    " << (const char*)vendor << "\n";
                std::cout << "  Renderer:  " << (const char*)renderer << "\n";
                std::cout << "  OpenGL:    " << (const char*)version << "\n";
                if (glsl) {
                    std::cout << "  GLSL:      " << (const char*)glsl << "\n";
                }
                
                // Get additional GPU info if available
                if (glew_status == GLEW_OK) {
                    GLint texture_units = 0;
                    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units);
                    if (texture_units > 0) {
                        std::cout << "  Max Texture Units: " << texture_units << "\n";
                    }
                    
                    GLint max_texture_size = 0;
                    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
                    if (max_texture_size > 0) {
                        std::cout << "  Max Texture Size:  " << max_texture_size << "x" << max_texture_size << "\n";
                    }
                }
                std::cout << "  Status: GPU acceleration available\n";
            } else {
                std::cout << "  Status: Unable to query GPU information\n";
            }
            
            glfwDestroyWindow(window);
        } else {
            std::cout << "  Status: Unable to create OpenGL context\n";
        }
        glfwTerminate();
    } else {
        std::cout << "  Status: GLFW initialization failed\n";
    }
#else
    std::cout << "  Status: Visualization support not compiled (BUILD_VIS=OFF)\n";
    std::cout << "  Rebuild with BUILD_VIS=ON to enable GPU acceleration\n";
#endif
    Display::BlankLine();
    
    std::cout << "Run 'vsepr <command> --help' for command-specific help.\n";
    Display::BlankLine();
    
    return 0;
}

std::string HelpCommand::Name() const {
    return "help";
}

std::string HelpCommand::Description() const {
    return "Show help information";
}

std::string HelpCommand::Help() const {
    return "Display usage information and available commands.";
}

}} // namespace vsepr::cli
