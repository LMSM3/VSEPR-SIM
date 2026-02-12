// Card Catalog Viewer - ImGui application
// Interactive browser for simulation runs

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "catalog/card_catalog.hpp"
#include <iostream>
#include <string>

class CardViewer {
public:
    CardViewer() : catalog_("out/catalog") {
        selected_card_ = nullptr;
        current_group_ = catalog::CardGroup::TopPicks;
        show_3d_view_ = false;
    }
    
    bool init() {
        // Initialize GLFW
        if (!glfwInit()) {
            return false;
        }
        
        // OpenGL 3.3
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        // Create window
        window_ = glfwCreateWindow(1600, 900, "VSEPR Card Catalog", nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);  // VSync
        
        // Initialize GLEW
        if (glewInit() != GLEW_OK) {
            return false;
        }
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        
        // Setup style
        ImGui::StyleColorsDark();
        
        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        // Load catalog
        if (!catalog_.load()) {
            std::cout << "Warning: No catalog found. Run discovery pipeline first.\n";
        }
        
        return true;
    }
    
    void run() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            
            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Render UI
            render_ui();
            
            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window_, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(window_);
        }
    }
    
    void shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
    
private:
    GLFWwindow* window_;
    catalog::CardCatalog catalog_;
    const catalog::RunCard* selected_card_;
    catalog::CardGroup current_group_;
    bool show_3d_view_;
    char search_buffer_[256] = {};
    
    void render_ui() {
        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Refresh Catalog", "F5")) {
                    catalog_.load();
                }
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    glfwSetWindowShouldClose(window_, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Top Picks")) {
                    current_group_ = catalog::CardGroup::TopPicks;
                }
                if (ImGui::MenuItem("Small (≤10)")) {
                    current_group_ = catalog::CardGroup::Small;
                }
                if (ImGui::MenuItem("Medium (11-50)")) {
                    current_group_ = catalog::CardGroup::Medium;
                }
                if (ImGui::MenuItem("Large (51-200)")) {
                    current_group_ = catalog::CardGroup::Large;
                }
                if (ImGui::MenuItem("Heavy (201+)")) {
                    current_group_ = catalog::CardGroup::Heavy;
                }
                if (ImGui::MenuItem("Exploded/Invalid")) {
                    current_group_ = catalog::CardGroup::Exploded;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        
        // Sidebar (card list)
        ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 880), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Cards", nullptr, ImGuiWindowFlags_NoMove)) {
            // Search bar
            ImGui::InputText("Search", search_buffer_, sizeof(search_buffer_));
            ImGui::Separator();
            
            // Get cards for current group
            auto cards = catalog_.get_group(current_group_);
            
            // Filter by search if needed
            if (strlen(search_buffer_) > 0) {
                cards = catalog_.search(search_buffer_);
            }
            
            ImGui::Text("%zu cards", cards.size());
            ImGui::Separator();
            
            // Display cards
            for (const auto& card : cards) {
                render_card(card);
            }
        }
        ImGui::End();
        
        // Details panel (selected card)
        if (selected_card_) {
            ImGui::SetNextWindowPos(ImVec2(410, 20), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(1180, 880), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Details", nullptr, ImGuiWindowFlags_NoMove)) {
                render_details(*selected_card_);
            }
            ImGui::End();
        }
    }
    
    void render_card(const catalog::RunCard& card) {
        // Card as selectable with colored background
        ImVec4 color(0.2f, 0.2f, 0.2f, 1.0f);
        
        // Color by health
        if (card.health == "converged") {
            color = ImVec4(0.2f, 0.6f, 0.2f, 1.0f);  // Green
        } else if (card.health == "bounded") {
            color = ImVec4(0.5f, 0.5f, 0.2f, 1.0f);  // Yellow
        } else if (card.health == "exploded") {
            color = ImVec4(0.6f, 0.2f, 0.2f, 1.0f);  // Red
        }
        
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.0f));
        
        bool selected = (selected_card_ == &card);
        if (ImGui::Selectable(card.title.c_str(), selected, 0, ImVec2(0, 60))) {
            selected_card_ = &card;
        }
        
        ImGui::PopStyleColor(2);
        
        // Card details on same line (hover tooltip)
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Formula: %s", card.formula.c_str());
            ImGui::Text("Size: %d atoms", card.size);
            ImGui::Text("Score: %.1f", card.score);
            ImGui::Text("Energy/atom: %.3f", card.energy_per_atom);
            ImGui::Text("Max force: %.4f", card.max_force);
            if (card.is_novel) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "🎉 NOVEL");
            }
            ImGui::EndTooltip();
        }
        
        // Show inline info
        ImGui::SameLine(250);
        ImGui::Text("%.0f", card.score);
        
        ImGui::Text("  %d atoms | %s", card.size, card.health.c_str());
    }
    
    void render_details(const catalog::RunCard& card) {
        ImGui::Text("Run ID: %s", card.run_id.c_str());
        ImGui::Text("Formula: %s", card.formula.c_str());
        ImGui::Text("Domain: %s", card.domain.c_str());
        ImGui::Text("Size: %d atoms", card.size);
        ImGui::Text("Model: %s", card.model.c_str());
        ImGui::Text("Health: %s", card.health.c_str());
        ImGui::Separator();
        
        ImGui::Text("Priority Score: %.1f / 100", card.score);
        ImGui::ProgressBar(card.score / 100.0f, ImVec2(-1, 0));

        // Score breakdown
        if (ImGui::TreeNode("Score Breakdown")) {
            ImGui::Text("Size preference (wN): %.3f", card.score_breakdown.wN);
            ImGui::Text("Charge neutrality (wQ): %.3f", card.score_breakdown.wQ);
            ImGui::Text("Metal richness (wM): %.3f", card.score_breakdown.wM);
            ImGui::Text("Element diversity (wD): %.3f", card.score_breakdown.wD);
            ImGui::Text("Stability gate (wS): %.3f", card.score_breakdown.wS);
            ImGui::Text("Classification bonus (wC): %.3f", card.score_breakdown.wC);

            // Show classifications
            if (!card.score_breakdown.classifications.empty()) {
                ImGui::Text("Classifications:");
                for (const auto& cls : card.score_breakdown.classifications) {
                    ImGui::BulletText("%s", cls.c_str());
                }
            }

            ImGui::Text("Computational cost: %.3f", card.score_breakdown.cost);
            ImGui::Text("Scientific value: %.3f", card.score_breakdown.value);
            ImGui::TreePop();
        }
        ImGui::Separator();
        
        ImGui::Text("Metrics:");
        ImGui::BulletText("Energy/atom: %.3f kcal/mol", card.energy_per_atom);
        ImGui::BulletText("Max force: %.4f kcal/mol/Å", card.max_force);
        ImGui::BulletText("Iterations: %d", card.iterations);
        ImGui::Separator();
        
        ImGui::Text("Validation:");
        ImGui::BulletText("Known: %s", card.is_known ? "Yes" : "No");
        ImGui::BulletText("Novel: %s", card.is_novel ? "🎉 Yes!" : "No");
        ImGui::BulletText("Confidence: %.0f%%", card.confidence * 100);
        ImGui::Separator();
        
        ImGui::Text("Tags:");
        for (const auto& tag : card.tags) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[%s]", tag.c_str());
        }
        ImGui::Separator();
        
        // Action buttons
        if (ImGui::Button("Open Folder", ImVec2(150, 30))) {
            // TODO: Open file browser to run_dir
            std::cout << "Opening: " << card.run_dir << "\n";
        }
        ImGui::SameLine();
        if (ImGui::Button("View 3D", ImVec2(150, 30))) {
            show_3d_view_ = true;
            // TODO: Launch 3D viewer with card.structure_xyz
        }
        ImGui::SameLine();
        if (ImGui::Button("Re-run", ImVec2(150, 30))) {
            // TODO: Re-run simulation with same parameters
        }
        ImGui::SameLine();
        if (ImGui::Button("Export", ImVec2(150, 30))) {
            // TODO: Export card + data
        }
    }
};

int main() {
    CardViewer viewer;
    
    if (!viewer.init()) {
        std::cerr << "Failed to initialize viewer\n";
        return 1;
    }
    
    std::cout << "Card Catalog Viewer started\n";
    std::cout << "Use File > Refresh to reload catalog\n";
    
    viewer.run();
    viewer.shutdown();
    
    return 0;
}
