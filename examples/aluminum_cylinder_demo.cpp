/**
 * aluminum_cylinder_demo.cpp
 * Create and analyze an aluminum cylinder using FEA
 * Compile: g++ -std=c++17 -I. -Iphysical_scale examples/aluminum_cylinder_demo.cpp -o aluminum_cylinder.exe
 * Run: ./aluminum_cylinder.exe
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <fstream>

// Simple mesh structures for cylinder
struct Node {
    double x, y, z;
};

struct Element {
    int n1, n2, n3, n4, n5, n6, n7, n8;  // Hex8 Default // Scale buy 8s 
};

class CylinderMesh {
public:
    std::vector<Node> nodes;
    std::vector<Element> elements;
    
    // Create cylinder: radius, height, circumferential divisions, axial divisions
    void create_cylinder(double R, double H, int n_theta, int n_z) {
        nodes.clear();
        elements.clear();
        
        std::cout << "Creating aluminum cylinder...\n";
        std::cout << "  Radius: " << R << " m\n";
        std::cout << "  Height: " << H << " m\n";
        std::cout << "  Circumferential divisions: " << n_theta << "\n";
        std::cout << "  Axial divisions: " << n_z << "\n";
        
        // Create nodes in a cylindrical grid
        // Inner and outer layers
        double R_inner = R * 0.8;
        
        for (int iz = 0; iz <= n_z; iz++) {
            double z = (H / n_z) * iz;
            
            // Outer layer
            for (int ith = 0; ith < n_theta; ith++) {
                double theta = (2 * M_PI / n_theta) * ith;
                double x = R * cos(theta);
                double y = R * sin(theta);
                nodes.push_back({x, y, z});
            }
            
            // Inner layer (for hollow elements)
            for (int ith = 0; ith < n_theta; ith++) {
                double theta = (2 * M_PI / n_theta) * ith;
                double x = R_inner * cos(theta);
                double y = R_inner * sin(theta);
                nodes.push_back({x, y, z});
            }
        }
        
        // Create elements
        for (int iz = 0; iz < n_z; iz++) {
            for (int ith = 0; ith < n_theta; ith++) {
                int ith_next = (ith + 1) % n_theta;
                
                // Bottom outer nodes
                int n1 = iz * (2 * n_theta) + ith;
                int n2 = iz * (2 * n_theta) + ith_next;
                int n3 = iz * (2 * n_theta) + n_theta + ith;
                int n4 = iz * (2 * n_theta) + n_theta + ith_next;
                
                // Top outer nodes
                int n5 = (iz + 1) * (2 * n_theta) + ith;
                int n6 = (iz + 1) * (2 * n_theta) + ith_next;
                int n7 = (iz + 1) * (2 * n_theta) + n_theta + ith;
                int n8 = (iz + 1) * (2 * n_theta) + n_theta + ith_next;
                
                elements.push_back({n1, n2, n3, n4, n5, n6, n7, n8});
            }
        }
        
        std::cout << "✓ Created " << nodes.size() << " nodes\n";
        std::cout << "✓ Created " << elements.size() << " elements\n";
    }
    
    // Export to VTK format for visualization
    void export_vtk(const std::string& filename) {
        std::ofstream file(filename);
        
        file << "# vtk DataFile Version 2.0\n";
        file << "Aluminum Cylinder\n";
        file << "ASCII\n";
        file << "DATASET UNSTRUCTURED_GRID\n";
        
        // Points
        file << "POINTS " << nodes.size() << " float\n";
        for (const auto& node : nodes) {
            file << std::scientific << node.x << " " << node.y << " " << node.z << "\n";
        }
        
        // Cells
        int cell_count = elements.size();
        int connectivity_size = cell_count * 9;  // 8 nodes + type per element
        
        file << "\nCELLS " << cell_count << " " << connectivity_size << "\n";
        for (const auto& elem : elements) {
            file << "8 " << elem.n1 << " " << elem.n2 << " " << elem.n3 << " " 
                 << elem.n4 << " " << elem.n5 << " " << elem.n6 << " " 
                 << elem.n7 << " " << elem.n8 << "\n";
        }
        
        // Cell types (12 = hexahedron)
        file << "\nCELL_TYPES " << cell_count << "\n";
        for (int i = 0; i < cell_count; i++) {
            file << "12\n";
        }
        
        std::cout << "\n✓ Exported to VTK: " << filename << "\n";
    }
    
    // Export to XYZ format
    void export_xyz(const std::string& filename) {
        std::ofstream file(filename);
        
        file << nodes.size() << "\n";
        file << "Aluminum Cylinder\n";
        
        // For demonstration, use Al for all nodes
        for (const auto& node : nodes) {
            file << "Al " << std::fixed << std::setprecision(6)
                 << node.x << " " << node.y << " " << node.z << "\n";
        }
        
        std::cout << "✓ Exported to XYZ: " << filename << "\n";
    }
    
    // Calculate xyzA prediction (coordinates with analysis metrics)
    void calculate_xyza_prediction() {
        std::cout << "\nCalculating xyzA Prediction...\n";
        
        for (int i = 0; i < nodes.size(); i++) {
            const auto& node = nodes[i];
            
            // Calculate distance from center
            double distance = std::sqrt(node.x * node.x + node.y * node.y + node.z * node.z);
            
            // Calculate angle in xy-plane
            double angle = std::atan2(node.y, node.x);
            
            // Calculate analysis metric (normalized distance)
            double metric = distance / (2.0 * M_PI);
            
            // Store for reference
            if (i < 3) {
                std::cout << "  Node " << i << ": xyz(" << std::fixed << std::setprecision(4)
                         << node.x << ", " << node.y << ", " << node.z << ") "
                         << "A=" << metric << "\n";
            }
        }
        std::cout << "  ✓ Analysis complete for " << nodes.size() << " nodes\n";
    }
    
    // Export to xyzA format (extended XYZ with analysis)
    void export_xyza(const std::string& filename) {
        std::ofstream file(filename);
        
        file << nodes.size() << "\n";
        file << "Aluminum Cylinder with Analysis Metrics\n";
        
        // Export nodes with analysis metrics
        for (int i = 0; i < nodes.size(); i++) {
            const auto& node = nodes[i];
            
            // Calculate distance and angle
            double distance = std::sqrt(node.x * node.x + node.y * node.y + node.z * node.z);
            double angle = std::atan2(node.y, node.x);
            double metric = distance / (2.0 * M_PI);
            
            // xyzA format: element x y z analysis_metric
            file << "Al " << std::fixed << std::setprecision(6)
                 << node.x << " " << node.y << " " << node.z << " "
                 << std::scientific << metric << "\n";
        }
        
        std::cout << "✓ Exported to xyzA: " << filename << "\n";
    }
    
    // Export to OBJ format for 3D visualization
    void export_obj(const std::string& filename) {
        std::ofstream file(filename);
        
        file << "# Aluminum Cylinder\n";
        file << "# Vertices: " << nodes.size() << "\n";
        file << "# Elements: " << elements.size() << "\n\n";
        
        // Write vertices
        for (const auto& node : nodes) {
            file << "v " << std::scientific << node.x << " " << node.y << " " << node.z << "\n";
        }
        
        file << "\n";
        
        // Write element faces (hexahedra as 6 quad faces)
        for (const auto& elem : elements) {
            // Bottom face
            file << "f " << (elem.n1+1) << " " << (elem.n2+1) << " " 
                 << (elem.n4+1) << " " << (elem.n3+1) << "\n";
            // Top face
            file << "f " << (elem.n5+1) << " " << (elem.n8+1) << " " 
                 << (elem.n6+1) << " " << (elem.n7+1) << "\n";
            // Front face
            file << "f " << (elem.n1+1) << " " << (elem.n5+1) << " " 
                 << (elem.n6+1) << " " << (elem.n2+1) << "\n";
            // Back face
            file << "f " << (elem.n3+1) << " " << (elem.n4+1) << " " 
                 << (elem.n8+1) << " " << (elem.n7+1) << "\n";
            // Inner face
            file << "f " << (elem.n3+1) << " " << (elem.n7+1) << " " 
                 << (elem.n8+1) << " " << (elem.n4+1) << "\n";
            // Outer face
            file << "f " << (elem.n1+1) << " " << (elem.n2+1) << " " 
                 << (elem.n6+1) << " " << (elem.n5+1) << "\n";
        }
        
        std::cout << "✓ Exported to OBJ: " << filename << "\n";
    }
};

// Simple material properties structure
struct Material {
    std::string name;
    double E;        // Young's modulus (Pa)
    double nu;       // Poisson's ratio
    double rho;      // Density (kg/m³)
};

class MaterialLibrary {
public:
    static Material aluminum() {
        return {"Aluminum", 69e9, 0.33, 2700.0};
    }
};

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║        Aluminum Cylinder FEA Visualization Demo            ║\n";
    std::cout << "║              VSEPR-Sim Physical Scale Module               ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Create cylinder
    CylinderMesh cylinder;
    
    // Parameters
    double radius = 0.05;          // 5 cm
    double height = 0.20;          // 20 cm
    int n_circumferential = 12;    // 12 divisions around circumference
    int n_axial = 8;               // 8 divisions along height
    
    cylinder.create_cylinder(radius, height, n_circumferential, n_axial);
    
    // Material properties
    Material al = MaterialLibrary::aluminum();
    std::cout << "\nMaterial: " << al.name << "\n";
    std::cout << "  E (Young's modulus):  " << al.E / 1e9 << " GPa\n";
    std::cout << "  ν (Poisson's ratio):  " << al.nu << "\n";
    std::cout << "  ρ (Density):          " << al.rho << " kg/m³\n";
    
    // Calculate properties
    double volume = M_PI * radius * radius * height;
    double mass = volume * al.rho;
    
    std::cout << "\nCylinder Properties:\n";
    std::cout << "  Volume:  " << volume * 1e6 << " cm³\n";
    std::cout << "  Mass:    " << mass * 1000 << " g\n";
    
    // Export in multiple formats
    std::cout << "\nExporting geometry...\n";
    cylinder.export_xyz("outputs/aluminum_cylinder.xyz");
    cylinder.export_obj("outputs/aluminum_cylinder.obj");
    cylinder.export_vtk("outputs/aluminum_cylinder.vtk");
    
    std::cout << "\n✓ Aluminum cylinder demo complete!\n";
    std::cout << "\nYou can now:\n";
    std::cout << "  • View outputs/aluminum_cylinder.obj in a 3D viewer\n";
    std::cout << "  • Import outputs/aluminum_cylinder.vtk into ParaView\n";
    std::cout << "  • Use outputs/aluminum_cylinder.xyz for VSEPR analysis\n";
    std::cout << "\n";
    
    return 0;
}
