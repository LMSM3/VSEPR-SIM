/**
 * multiscale_demo.cpp
 * 
 * Demonstration of Multiscale Bridge with GPU Resource Management
 * 
 * Shows:
 * 1. GPU conflict prevention
 * 2. Molecular → FEA property transfer
 * 3. Safe scale transitions
 * 4. User confirmation workflow
 * 
 * Date: January 18, 2026
 */

#include "multiscale/molecular_fea_bridge.hpp"
#include "sim/molecule.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace vsepr;
using namespace vsepr::multiscale;

void print_header(const std::string& title) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(57) << title << "║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

void demo_1_gpu_conflict_prevention() {
    print_header("DEMO 1: GPU Conflict Prevention");
    
    MolecularFEABridge bridge;
    
    std::cout << "Attempting to activate BOTH scales simultaneously...\n\n";
    
    // Activate molecular scale
    std::cout << "Step 1: Activate molecular scale\n";
    if (bridge.activate_molecular_scale()) {
        std::cout << "✓ Molecular scale activated successfully\n\n";
        
        // Try to activate FEA scale (should fail)
        std::cout << "Step 2: Try to activate FEA scale (should fail)\n";
        if (!bridge.activate_fea_scale()) {
            std::cout << "✓ FEA scale activation blocked (as expected)\n\n";
        }
        
        // Deactivate molecular
        std::cout << "Step 3: Deactivate molecular scale\n";
        bridge.deactivate_molecular_scale();
        
        // Now FEA should work
        std::cout << "Step 4: Try FEA scale again (should succeed)\n";
        if (bridge.activate_fea_scale()) {
            std::cout << "✓ FEA scale activated successfully\n\n";
            bridge.deactivate_fea_scale();
        }
    }
}

void demo_2_property_extraction() {
    print_header("DEMO 2: Molecular → FEA Property Extraction");
    
    MolecularFEABridge bridge;
    
    // Create a simple molecule (water)
    std::cout << "Creating water molecule (H2O)...\n";
    Molecule water;
    // Note: In real code, build from formula or load from file
    // For demo, we'll create empty molecule
    
    std::cout << "\nExtracting continuum properties...\n";
    auto props = bridge.extract_properties(water);
    
    props.print();
    
    std::cout << "Exporting to FEA format...\n";
    props.export_to_fea("water_material.fea");
}

void demo_3_safe_transition() {
    print_header("DEMO 3: Safe Scale Transition Workflow");
    
    MolecularFEABridge bridge;
    
    std::cout << "Demonstrating safe transition: Molecular → FEA\n\n";
    
    // Step 1: Activate molecular
    std::cout << "═══ PHASE 1: Molecular Dynamics ═══\n\n";
    if (bridge.activate_molecular_scale()) {
        std::cout << "Running molecular simulation...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "✓ Molecular simulation complete\n\n";
        
        // Step 2: Show status
        bridge.print_gpu_status();
        
        // Step 3: Deactivate molecular
        std::cout << "═══ PHASE 2: Transition ═══\n\n";
        std::cout << "Deactivating molecular scale...\n";
        bridge.deactivate_molecular_scale();
        
        // Step 4: Activate FEA
        std::cout << "\n═══ PHASE 3: Physical Scale FEA ═══\n\n";
        if (bridge.activate_fea_scale()) {
            std::cout << "Running FEA simulation...\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "✓ FEA simulation complete\n\n";
            
            bridge.deactivate_fea_scale();
        }
    }
    
    std::cout << "\n═══ FINAL STATUS ═══\n";
    bridge.print_gpu_status();
}

void demo_4_gpu_status_monitoring() {
    print_header("DEMO 4: GPU Status Monitoring");
    
    auto& gpu = GPUResourceManager::instance();
    
    std::cout << "Initial GPU status:\n";
    gpu.print_status();
    
    std::cout << "Requesting molecular scale...\n";
    if (gpu.request_activation(GPUScaleType::MOLECULAR, "Test Molecular")) {
        std::cout << "✓ Request granted\n\n";
        
        std::cout << "Current status (before confirmation):\n";
        gpu.print_status();
        
        std::cout << "Confirming activation...\n";
        if (gpu.confirm_activation(GPUScaleType::MOLECULAR)) {
            std::cout << "\nCurrent status (after confirmation):\n";
            gpu.print_status();
            
            std::cout << "Deactivating...\n";
            gpu.deactivate_scale();
            
            std::cout << "Final status:\n";
            gpu.print_status();
        }
    }
}

void demo_5_automatic_mode() {
    print_header("DEMO 5: Automated Multiscale Workflow");
    
    std::cout << "This demo shows automated workflow without user prompts\n";
    std::cout << "(In production, use programmatic confirmation)\n\n";
    
    MolecularFEABridge bridge;
    auto& gpu = GPUResourceManager::instance();
    
    // Programmatic activation (bypass user input)
    std::cout << "Step 1: Request molecular scale\n";
    if (gpu.request_activation(GPUScaleType::MOLECULAR, "Automated Molecular")) {
        gpu.confirm_activation(GPUScaleType::MOLECULAR);  // Auto-confirm
        
        std::cout << "Step 2: Simulate molecular dynamics\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::cout << "Step 3: Extract properties\n";
        Molecule mol;
        auto props = bridge.extract_properties(mol);
        
        std::cout << "Step 4: Deactivate molecular\n";
        gpu.deactivate_scale();
        
        std::cout << "Step 5: Request FEA scale\n";
        if (gpu.request_activation(GPUScaleType::PHYSICAL_FEA, "Automated FEA")) {
            gpu.confirm_activation(GPUScaleType::PHYSICAL_FEA);  // Auto-confirm
            
            std::cout << "Step 6: Run FEA with extracted properties\n";
            props.print();
            
            std::cout << "Step 7: Deactivate FEA\n";
            gpu.deactivate_scale();
        }
    }
    
    std::cout << "\n✓ Automated workflow complete\n";
}

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                           ║\n";
    std::cout << "║     MULTISCALE BRIDGE DEMONSTRATION                       ║\n";
    std::cout << "║     GPU Resource Management + MD ↔ FEA                   ║\n";
    std::cout << "║                                                           ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    if (argc > 1) {
        std::string demo = argv[1];
        
        if (demo == "1" || demo == "conflict") {
            demo_1_gpu_conflict_prevention();
        } else if (demo == "2" || demo == "extract") {
            demo_2_property_extraction();
        } else if (demo == "3" || demo == "transition") {
            demo_3_safe_transition();
        } else if (demo == "4" || demo == "status") {
            demo_4_gpu_status_monitoring();
        } else if (demo == "5" || demo == "auto") {
            demo_5_automatic_mode();
        } else {
            std::cout << "Unknown demo: " << demo << "\n";
            std::cout << "Usage: multiscale_demo [1|2|3|4|5]\n";
            return 1;
        }
    } else {
        // Run all demos
        std::cout << "Running all demonstrations...\n\n";
        
        demo_1_gpu_conflict_prevention();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        demo_2_property_extraction();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        demo_3_safe_transition();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        demo_4_gpu_status_monitoring();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        demo_5_automatic_mode();
    }
    
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ALL DEMONSTRATIONS COMPLETE                              ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    return 0;
}
