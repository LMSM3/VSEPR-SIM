#pragma once
/**
 * pipe_thermal_panel.hpp - ImGui Panel for Pipe Flow & Thermal Module
 *
 * Provides an interactive GUI panel that integrates with the existing
 * vsepr_vis / vsepr_gui infrastructure.  All physics computation
 * delegates to pipe_thermal_engine.hpp; this file handles only
 * presentation and user interaction.
 *
 * Unit conversions happen at the display boundary only.
 */

#ifdef BUILD_VISUALIZATION

#include "include/pipe_thermal/pipe_thermal_engine.hpp"
#include "imgui.h"
#include <string>
#include <vector>

namespace pipe_thermal {
namespace gui {

class PipeThermalPanel {
public:
    PipeThermalPanel() {
        result_ = solve_series_network(cfg_);
    }

    void render() {
        if (!visible_) return;

        ImGui::SetNextWindowSize(ImVec2(720, 640), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Pipe Flow & Thermal Analysis", &visible_)) {
            ImGui::End();
            return;
        }

        render_configuration();
        ImGui::Separator();
        render_controls();
        ImGui::Separator();
        render_results();

        if (show_sweep_) {
            ImGui::Separator();
            render_sweep();
        }

        ImGui::End();
    }

    bool& visible() { return visible_; }

private:
    PipeThermalConfig cfg_;
    NetworkResult     result_;
    bool visible_ = true;
    bool auto_solve_ = true;
    bool show_sweep_ = false;
    bool dirty_ = true;

    // Sweep state
    int sweep_type_ = 0;  // 0=velocity, 1=diameter, 2=T_wall
    float sweep_min_ = 0.1f;
    float sweep_max_ = 5.0f;
    int   sweep_points_ = 20;
    std::vector<SweepPoint> sweep_results_;

    // Fluid/material selection indices
    int fluid_idx_ = 0;
    int material_idx_ = 0;

    void solve_if_dirty() {
        if (!dirty_) return;
        if (cfg_.use_network)
            result_ = solve_stochastic_network(cfg_);
        else
            result_ = solve_series_network(cfg_);
        dirty_ = false;
    }

    void render_configuration() {
        if (ImGui::CollapsingHeader("Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {

            // Geometry
            ImGui::Text("Geometry");
            int n_seg = static_cast<int>(cfg_.n_segments);
            if (ImGui::SliderInt("Segments", &n_seg, 1, 50)) {
                cfg_.n_segments = static_cast<uint32_t>(n_seg);
                dirty_ = true;
            }

            float seg_len = static_cast<float>(cfg_.segment_length_m);
            if (ImGui::SliderFloat("Segment Length (m)", &seg_len, 0.1f, 20.0f)) {
                cfg_.segment_length_m = seg_len;
                dirty_ = true;
            }

            float id_mm = static_cast<float>(units::m_to_mm(cfg_.inner_diameter_m));
            float wall = static_cast<float>((cfg_.outer_diameter_m - cfg_.inner_diameter_m) / 2.0);
            if (ImGui::SliderFloat("Inner Diameter (mm)", &id_mm, 5.0f, 200.0f)) {
                cfg_.inner_diameter_m = id_mm / 1000.0;
                cfg_.outer_diameter_m = cfg_.inner_diameter_m + 2.0 * wall;
                dirty_ = true;
            }

            // Flow conditions
            ImGui::Spacing();
            ImGui::Text("Flow Conditions");
            float vel = static_cast<float>(cfg_.inlet_velocity_ms);
            if (ImGui::SliderFloat("Velocity (m/s)", &vel, 0.01f, 10.0f)) {
                cfg_.inlet_velocity_ms = vel;
                dirty_ = true;
            }

            float t_in_c = static_cast<float>(units::K_to_C(cfg_.T_inlet_K));
            if (ImGui::SliderFloat("T_inlet (C)", &t_in_c, 0.0f, 100.0f)) {
                cfg_.T_inlet_K = units::C_to_K(t_in_c);
                dirty_ = true;
            }

            float t_wall_c = static_cast<float>(units::K_to_C(cfg_.T_wall_K));
            if (ImGui::SliderFloat("T_wall (C)", &t_wall_c, 0.0f, 300.0f)) {
                cfg_.T_wall_K = units::C_to_K(t_wall_c);
                dirty_ = true;
            }

            // Fluid selection
            ImGui::Spacing();
            const char* fluids[] = {"Water (20C)", "Water (80C)", "Air (20C)", "Ethylene Glycol", "Engine Oil"};
            if (ImGui::Combo("Fluid", &fluid_idx_, fluids, 5)) {
                switch (fluid_idx_) {
                    case 0: cfg_.fluid = water_20C(); break;
                    case 1: cfg_.fluid = water_80C(); break;
                    case 2: cfg_.fluid = air_20C(); break;
                    case 3: cfg_.fluid = ethylene_glycol(); break;
                    case 4: cfg_.fluid = engine_oil(); break;
                }
                dirty_ = true;
            }

            // Material selection
            const char* materials[] = {"Copper", "Steel", "PVC", "Stainless", "Aluminum"};
            if (ImGui::Combo("Pipe Material", &material_idx_, materials, 5)) {
                switch (material_idx_) {
                    case 0: cfg_.material = copper_pipe(); break;
                    case 1: cfg_.material = steel_pipe(); break;
                    case 2: cfg_.material = pvc_pipe(); break;
                    case 3: cfg_.material = stainless_pipe(); break;
                    case 4: cfg_.material = aluminum_pipe(); break;
                }
                dirty_ = true;
            }

            // Stochastic toggle
            ImGui::Spacing();
            if (ImGui::Checkbox("Stochastic Network", &cfg_.use_network)) {
                dirty_ = true;
            }
            if (cfg_.use_network) {
                int seed = static_cast<int>(cfg_.seed);
                if (ImGui::InputInt("Seed", &seed)) {
                    cfg_.seed = static_cast<uint64_t>(seed);
                    dirty_ = true;
                }
            }
        }
    }

    void render_controls() {
        ImGui::Checkbox("Auto-solve on change", &auto_solve_);
        ImGui::SameLine();
        if (ImGui::Button("Solve Now") || (auto_solve_ && dirty_)) {
            solve_if_dirty();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Show Sweep", &show_sweep_);
    }

    void render_results() {
        if (result_.segments.empty()) return;

        if (ImGui::CollapsingHeader("Network Summary", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "summary_cols");
            ImGui::Text("Total Pressure Drop");
            ImGui::NextColumn();
            ImGui::Text("%.2f kPa (%.0f Pa)",
                        units::Pa_to_kPa(result_.total_pressure_drop_Pa),
                        result_.total_pressure_drop_Pa);
            ImGui::NextColumn();

            ImGui::Text("Total Heat Transfer");
            ImGui::NextColumn();
            ImGui::Text("%.2f W (%.4f kW)",
                        result_.total_heat_transfer_W,
                        units::W_to_kW(result_.total_heat_transfer_W));
            ImGui::NextColumn();

            ImGui::Text("Pumping Power");
            ImGui::NextColumn();
            ImGui::Text("%.4f W", result_.total_pumping_power_W);
            ImGui::NextColumn();

            ImGui::Text("T_outlet");
            ImGui::NextColumn();
            ImGui::Text("%.2f C (%.2f K)",
                        units::K_to_C(result_.T_outlet_K), result_.T_outlet_K);
            ImGui::NextColumn();

            ImGui::Text("Effectiveness");
            ImGui::NextColumn();
            ImGui::Text("%.4f", result_.network_effectiveness);
            ImGui::NextColumn();

            ImGui::Text("Flow Regime");
            ImGui::NextColumn();
            ImGui::Text("%u laminar / %u turbulent",
                        result_.n_laminar, result_.n_turbulent);
            ImGui::NextColumn();

            ImGui::Columns(1);
        }

        if (ImGui::CollapsingHeader("Per-Segment Detail")) {
            ImGui::BeginChild("SegmentTable", ImVec2(0, 250), true,
                              ImGuiWindowFlags_HorizontalScrollbar);

            if (ImGui::BeginTable("segments", 10,
                                  ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Seg");
                ImGui::TableSetupColumn("L(m)");
                ImGui::TableSetupColumn("D(mm)");
                ImGui::TableSetupColumn("Re");
                ImGui::TableSetupColumn("f");
                ImGui::TableSetupColumn("dP(Pa)");
                ImGui::TableSetupColumn("Nu");
                ImGui::TableSetupColumn("h(W/m2K)");
                ImGui::TableSetupColumn("T_in(C)");
                ImGui::TableSetupColumn("T_out(C)");
                ImGui::TableHeadersRow();

                for (const auto& s : result_.segments) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%u", s.segment_id);
                    ImGui::TableNextColumn(); ImGui::Text("%.3f", s.length_m);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", units::m_to_mm(s.inner_diameter_m));
                    ImGui::TableNextColumn(); ImGui::Text("%.0f", s.Re);
                    ImGui::TableNextColumn(); ImGui::Text("%.6f", s.friction_factor);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", s.pressure_drop_Pa);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", s.Nu);
                    ImGui::TableNextColumn(); ImGui::Text("%.1f", s.h_conv);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", units::K_to_C(s.T_fluid_in_K));
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", units::K_to_C(s.T_fluid_out_K));
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }
    }

    void render_sweep() {
        if (ImGui::CollapsingHeader("Parametric Sweep", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* sweep_types[] = {"Velocity", "Diameter", "Wall Temperature"};
            ImGui::Combo("Sweep Parameter", &sweep_type_, sweep_types, 3);

            ImGui::SliderFloat("Min", &sweep_min_, 0.01f, 100.0f);
            ImGui::SliderFloat("Max", &sweep_max_, 0.02f, 500.0f);
            ImGui::SliderInt("Points", &sweep_points_, 5, 100);

            if (ImGui::Button("Run Sweep")) {
                switch (sweep_type_) {
                    case 0:
                        sweep_results_ = sweep_velocity(cfg_, sweep_min_, sweep_max_, sweep_points_);
                        break;
                    case 1:
                        sweep_results_ = sweep_diameter(cfg_, sweep_min_/1000.0, sweep_max_/1000.0, sweep_points_);
                        break;
                    case 2:
                        sweep_results_ = sweep_wall_temperature(cfg_,
                            units::C_to_K(sweep_min_), units::C_to_K(sweep_max_), sweep_points_);
                        break;
                }
            }

            if (!sweep_results_.empty()) {
                if (ImGui::BeginTable("sweep_table", 5,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Parameter");
                    ImGui::TableSetupColumn("dP(kPa)");
                    ImGui::TableSetupColumn("Q(W)");
                    ImGui::TableSetupColumn("T_out(C)");
                    ImGui::TableSetupColumn("Eff");
                    ImGui::TableHeadersRow();

                    for (const auto& p : sweep_results_) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (sweep_type_ == 1)
                            ImGui::Text("%.1f mm", units::m_to_mm(p.parameter_value));
                        else if (sweep_type_ == 2)
                            ImGui::Text("%.1f C", units::K_to_C(p.parameter_value));
                        else
                            ImGui::Text("%.3f m/s", p.parameter_value);

                        ImGui::TableNextColumn(); ImGui::Text("%.2f", units::Pa_to_kPa(p.result.total_pressure_drop_Pa));
                        ImGui::TableNextColumn(); ImGui::Text("%.1f", p.result.total_heat_transfer_W);
                        ImGui::TableNextColumn(); ImGui::Text("%.2f", units::K_to_C(p.result.T_outlet_K));
                        ImGui::TableNextColumn(); ImGui::Text("%.4f", p.result.network_effectiveness);
                    }
                    ImGui::EndTable();
                }

                // Simple ASCII sparkline for heat transfer
                ImGui::Text("Heat Transfer Profile:");
                std::vector<float> q_vals;
                for (const auto& p : sweep_results_)
                    q_vals.push_back(static_cast<float>(p.result.total_heat_transfer_W));
                ImGui::PlotLines("Q(W)", q_vals.data(),
                                 static_cast<int>(q_vals.size()),
                                 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 80));
            }
        }
    }
};

} // namespace gui
} // namespace pipe_thermal

#endif // BUILD_VISUALIZATION
