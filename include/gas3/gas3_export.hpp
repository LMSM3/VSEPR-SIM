/**
 * gas3_export.hpp
 * ---------------
 * Export Engine for Gas3 Module — V4.0.1 Visual Suite.
 *
 * Produces:
 *   - CSV files (machine-readable, archival)
 *   - HTML data site (tabbed, 3D Plotly surfaces, 2D diagnostics)
 *   - Run manifests (JSON)
 *
 * Default theme: light (inverted from prior dark). Clean, publishable.
 * Tabs: Overview | 2D Charts | 3D Surfaces | Data | Fitting | Diagnostics
 * 3D: Plotly.js CDN — PvT surface, Z(T,P) surface, quality scatter.
 *
 * Anti-black-box: every file is traceable to a run manifest.
 */

#pragma once

#include "gas3_state_record.hpp"
#include "gas3_quality.hpp"
#include "gas3_sweep.hpp"
#include "gas3_fitting.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <map>
#include <set>
#include <algorithm>

namespace vsepr {
namespace gas3 {

// ============================================================================
// Run manifest
// ============================================================================

struct RunManifest {
    std::string run_id;
    std::string timestamp;
    std::string description;
    size_t total_records    = 0;
    size_t linear_records   = 0;
    size_t random_records   = 0;
    size_t adaptive_records = 0;
    std::vector<std::string> species_list;
    std::vector<std::string> models_used;
    SweepStats linear_stats;
    SweepStats random_stats;
    SweepStats adaptive_stats;

    std::string to_json() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "{\n";
        ss << "  \"run_id\": \"" << run_id << "\",\n";
        ss << "  \"timestamp\": \"" << timestamp << "\",\n";
        ss << "  \"description\": \"" << description << "\",\n";
        ss << "  \"total_records\": " << total_records << ",\n";
        ss << "  \"linear_records\": " << linear_records << ",\n";
        ss << "  \"random_records\": " << random_records << ",\n";
        ss << "  \"adaptive_records\": " << adaptive_records << ",\n";
        ss << "  \"convergence_rate_pct\": " << linear_stats.convergence_rate() << ",\n";
        ss << "  \"avg_quality\": " << linear_stats.avg_quality << ",\n";
        ss << "  \"species\": [";
        for (size_t i = 0; i < species_list.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << species_list[i] << "\"";
        }
        ss << "],\n";
        ss << "  \"models\": [";
        for (size_t i = 0; i < models_used.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << models_used[i] << "\"";
        }
        ss << "]\n";
        ss << "}";
        return ss.str();
    }
};

// ============================================================================
// CSV export
// ============================================================================

inline bool write_csv(const std::string& path,
                      const std::vector<GasStateRecord>& records) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << GasStateRecord::csv_header() << "\n";
    for (const auto& r : records) {
        f << r.to_csv_row() << "\n";
    }
    return true;
}

inline bool write_failures_csv(const std::string& path,
                               const std::vector<GasStateRecord>& records) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << GasStateRecord::csv_header() << "\n";
    for (const auto& r : records) {
        if (r.quality_tier == QualityTier::Q0 || r.quality_tier == QualityTier::Q1)
            f << r.to_csv_row() << "\n";
    }
    return true;
}

// ============================================================================
// HTML style (light default, inverted toggle)
// ============================================================================

namespace detail {

inline void emit_css(std::ofstream& f) {
    f << R"CSS(
:root {
  --bg: #ffffff; --fg: #1a1a2e; --bg2: #f4f4f8; --border: #d0d0d8;
  --accent: #1e40af; --accent2: #7c3aed; --accent3: #059669;
  --q0: #dc2626; --q1: #d97706; --q2: #ca8a04; --q3: #16a34a; --q4: #2563eb;
  --q0bg: #fef2f2; --q1bg: #fffbeb; --q2bg: #fefce8; --q3bg: #f0fdf4; --q4bg: #eff6ff;
  --card-bg: #ffffff; --card-border: #e0e0e8; --hover: #eef2ff;
  --muted: #64748b; --code-bg: #f1f5f9;
}
body.dark {
  --bg: #0f172a; --fg: #e2e8f0; --bg2: #1e293b; --border: #334155;
  --accent: #60a5fa; --accent2: #a78bfa; --accent3: #34d399;
  --q0bg: #450a0a; --q1bg: #451a03; --q2bg: #422006; --q3bg: #052e16; --q4bg: #172554;
  --card-bg: #1e293b; --card-border: #334155; --hover: #1e3a5f;
  --muted: #94a3b8; --code-bg: #1e293b;
}
*, *::before, *::after { box-sizing: border-box; }
body { font-family: 'Inter', 'Segoe UI', system-ui, sans-serif; max-width: 1400px;
       margin: 0 auto; padding: 24px; background: var(--bg); color: var(--fg);
       transition: background 0.3s, color 0.3s; line-height: 1.5; }
h1 { color: var(--accent); border-bottom: 2px solid var(--border);
     padding-bottom: 12px; font-size: 1.75rem; font-weight: 700; margin-bottom: 8px; }
h2 { color: var(--accent2); margin-top: 28px; font-size: 1.25rem; }
h3 { color: var(--accent3); font-size: 1.05rem; }
a { color: var(--accent); }

/* Tabs */
.tab-bar { display: flex; gap: 0; border-bottom: 2px solid var(--border);
           margin: 20px 0 0 0; flex-wrap: wrap; }
.tab-btn { padding: 10px 20px; cursor: pointer; background: transparent; border: none;
           color: var(--muted); font-size: 14px; font-weight: 600;
           border-bottom: 3px solid transparent; transition: all 0.2s; }
.tab-btn:hover { color: var(--accent); background: var(--bg2); }
.tab-btn.active { color: var(--accent); border-bottom-color: var(--accent); }
.tab-panel { display: none; padding: 20px 0; animation: fadeIn 0.3s; }
.tab-panel.active { display: block; }
@keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }

/* Cards */
.stat-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
             gap: 12px; margin: 16px 0; }
.stat-card { background: var(--card-bg); border: 1px solid var(--card-border);
             border-radius: 8px; padding: 16px; text-align: center;
             box-shadow: 0 1px 3px rgba(0,0,0,0.06); }
.stat-card .value { font-size: 1.5rem; font-weight: 700; color: var(--accent); }
.stat-card .label { font-size: 0.75rem; color: var(--muted); margin-top: 4px; }

/* Quality bar */
.qbar-wrap { display: flex; width: 100%; height: 28px; border-radius: 6px;
             overflow: hidden; box-shadow: 0 1px 2px rgba(0,0,0,0.1); }
.qbar { transition: width 0.4s; }
.qbar-q0 { background: var(--q0); } .qbar-q1 { background: var(--q1); }
.qbar-q2 { background: var(--q2); } .qbar-q3 { background: var(--q3); }
.qbar-q4 { background: var(--q4); }

/* Tables */
table { border-collapse: collapse; width: 100%; margin: 12px 0; font-size: 13px; }
th { background: var(--bg2); color: var(--accent); padding: 8px 6px; text-align: left;
     border: 1px solid var(--border); position: sticky; top: 0; font-weight: 600;
     font-size: 12px; text-transform: uppercase; letter-spacing: 0.5px; }
td { padding: 6px; border: 1px solid var(--border); }
tr:nth-child(even) { background: var(--bg2); }
tr:hover { background: var(--hover); }
.q0 { background: var(--q0bg) !important; }
.q1 { background: var(--q1bg) !important; }
.q2 { background: var(--q2bg) !important; }
.q3 { background: var(--q3bg) !important; }
.q4 { background: var(--q4bg) !important; }

/* Plots */
.plot-container { width: 100%; border: 1px solid var(--border); border-radius: 8px;
                  overflow: hidden; margin: 16px 0; background: var(--card-bg); }

/* Theme toggle */
.theme-toggle { position: fixed; top: 16px; right: 16px; z-index: 100;
                padding: 8px 14px; border-radius: 6px; cursor: pointer;
                background: var(--bg2); color: var(--fg); border: 1px solid var(--border);
                font-size: 13px; font-weight: 600; }

/* Pre */
pre { background: var(--code-bg); padding: 12px; border-radius: 6px;
      border: 1px solid var(--border); overflow-x: auto; font-size: 12px;
      font-family: 'Cascadia Code', 'Fira Code', monospace; }

.footer { margin-top: 40px; padding-top: 20px; border-top: 1px solid var(--border);
          font-size: 11px; color: var(--muted); text-align: center; }
.badge { display: inline-block; padding: 2px 8px; border-radius: 4px;
         font-size: 11px; font-weight: 600; }
.badge-q0 { background: var(--q0bg); color: var(--q0); }
.badge-q1 { background: var(--q1bg); color: var(--q1); }
.badge-q2 { background: var(--q2bg); color: var(--q2); }
.badge-q3 { background: var(--q3bg); color: var(--q3); }
.badge-q4 { background: var(--q4bg); color: var(--q4); }
)CSS";
}

inline void emit_tab_js(std::ofstream& f) {
    f << R"JS(
<script>
function switchTab(tabId) {
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
  document.getElementById('tab-'+tabId).classList.add('active');
  document.getElementById('panel-'+tabId).classList.add('active');
  if (window['initPlot_'+tabId]) { window['initPlot_'+tabId](); window['initPlot_'+tabId] = null; }
}
function toggleTheme() {
  document.body.classList.toggle('dark');
  const btn = document.getElementById('theme-btn');
  btn.textContent = document.body.classList.contains('dark') ? 'Light Mode' : 'Dark Mode';
}
</script>
)JS";
}

inline const char* tier_css_class(QualityTier t) {
    switch (t) {
        case QualityTier::Q0: return "q0";
        case QualityTier::Q1: return "q1";
        case QualityTier::Q2: return "q2";
        case QualityTier::Q3: return "q3";
        case QualityTier::Q4: return "q4";
    }
    return "";
}

inline const char* badge_class(QualityTier t) {
    switch (t) {
        case QualityTier::Q0: return "badge badge-q0";
        case QualityTier::Q1: return "badge badge-q1";
        case QualityTier::Q2: return "badge badge-q2";
        case QualityTier::Q3: return "badge badge-q3";
        case QualityTier::Q4: return "badge badge-q4";
    }
    return "badge";
}

} // namespace detail

// ============================================================================
// HTML report: full tabbed data site with 3D rendering
// ============================================================================

struct ReportFitData {
    FitResult2D z_surface;
    bool has_z_surface = false;
};

inline bool write_html_report(
    const std::string& path,
    const RunManifest& manifest,
    const std::vector<GasStateRecord>& records,
    const ReportFitData& fits = {})
{
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    f << "<meta charset=\"UTF-8\">\n";
    f << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    f << "<title>VSEPR-SIM Gas3 | " << manifest.run_id << "</title>\n";
    f << "<script src=\"https://cdn.plot.ly/plotly-2.35.2.min.js\"></script>\n";
    f << "<style>\n";
    detail::emit_css(f);
    f << "</style>\n</head>\n<body>\n";

    // Theme toggle
    f << "<button class=\"theme-toggle\" id=\"theme-btn\" onclick=\"toggleTheme()\">Dark Mode</button>\n";

    // Header
    f << "<h1>VSEPR-SIM Gas3 Thermodynamic Report</h1>\n";
    f << "<p style=\"color:var(--muted)\">Run: <strong style=\"color:var(--fg)\">"
      << manifest.run_id << "</strong> &middot; " << manifest.timestamp
      << " &middot; " << manifest.total_records << " states &middot; v4.0.1 Beta</p>\n";

    // --- Tab bar ---
    f << "<div class=\"tab-bar\">\n";
    f << "  <button class=\"tab-btn active\" id=\"tab-overview\" onclick=\"switchTab('overview')\">Overview</button>\n";
    f << "  <button class=\"tab-btn\" id=\"tab-charts\" onclick=\"switchTab('charts')\">2D Charts</button>\n";
    f << "  <button class=\"tab-btn\" id=\"tab-surfaces\" onclick=\"switchTab('surfaces')\">3D Surfaces</button>\n";
    f << "  <button class=\"tab-btn\" id=\"tab-data\" onclick=\"switchTab('data')\">Data Tables</button>\n";
    f << "  <button class=\"tab-btn\" id=\"tab-fitting\" onclick=\"switchTab('fitting')\">Fitting</button>\n";
    f << "  <button class=\"tab-btn\" id=\"tab-diag\" onclick=\"switchTab('diag')\">Diagnostics</button>\n";
    f << "</div>\n";

    // Aggregate stats
    const auto& ls = manifest.linear_stats;
    const auto& rs = manifest.random_stats;
    const auto& as = manifest.adaptive_stats;
    size_t total_q0=0, total_q1=0, total_q2=0, total_q3=0, total_q4=0;
    for (const auto& r : records) {
        switch (r.quality_tier) {
            case QualityTier::Q0: total_q0++; break;
            case QualityTier::Q1: total_q1++; break;
            case QualityTier::Q2: total_q2++; break;
            case QualityTier::Q3: total_q3++; break;
            case QualityTier::Q4: total_q4++; break;
        }
    }
    size_t q_total = total_q0 + total_q1 + total_q2 + total_q3 + total_q4;

    // ========================================================================
    // TAB: Overview
    // ========================================================================
    f << "<div class=\"tab-panel active\" id=\"panel-overview\">\n";

    // Stat cards
    f << "<div class=\"stat-grid\">\n";
    auto card = [&](const std::string& label, const std::string& value) {
        f << "<div class=\"stat-card\"><div class=\"value\">" << value
          << "</div><div class=\"label\">" << label << "</div></div>\n";
    };
    card("Total States", std::to_string(manifest.total_records));
    card("Linear", std::to_string(manifest.linear_records));
    card("Random", std::to_string(manifest.random_records));
    card("Adaptive", std::to_string(manifest.adaptive_records));
    {
        std::ostringstream ss;
        size_t conv = ls.converged + rs.converged + as.converged;
        size_t tot = ls.total_points + rs.total_points + as.total_points;
        double pct = tot > 0 ? 100.0 * conv / tot : 0.0;
        ss << std::fixed << std::setprecision(1) << pct << "%";
        card("Convergence", ss.str());
    }
    card("Species", std::to_string(manifest.species_list.size()));
    card("Q4 Reference", std::to_string(total_q4));
    card("Q0 Failed", std::to_string(total_q0));
    f << "</div>\n";

    // Quality distribution
    f << "<h2>Quality Distribution</h2>\n";
    if (q_total > 0) {
        auto bar_pct = [&](size_t c) { return 100.0 * c / q_total; };
        f << "<div class=\"qbar-wrap\">\n";
        auto emit_bar = [&](const char* cls, size_t c) {
            double p = bar_pct(c);
            if (p > 0.3)
                f << "<div class=\"qbar " << cls << "\" style=\"width:" << std::fixed
                  << std::setprecision(1) << p << "%\" title=\"" << c << "\"></div>\n";
        };
        emit_bar("qbar-q4", total_q4);
        emit_bar("qbar-q3", total_q3);
        emit_bar("qbar-q2", total_q2);
        emit_bar("qbar-q1", total_q1);
        emit_bar("qbar-q0", total_q0);
        f << "</div>\n";
        f << "<p style=\"font-size:12px;color:var(--muted);margin-top:6px\">"
          << "<span class=\"badge badge-q4\">Q4: " << total_q4 << "</span> "
          << "<span class=\"badge badge-q3\">Q3: " << total_q3 << "</span> "
          << "<span class=\"badge badge-q2\">Q2: " << total_q2 << "</span> "
          << "<span class=\"badge badge-q1\">Q1: " << total_q1 << "</span> "
          << "<span class=\"badge badge-q0\">Q0: " << total_q0 << "</span></p>\n";
    }

    // Species coverage
    f << "<h2>Species Coverage</h2>\n";
    std::map<std::string, int> sp_count;
    std::map<std::string, double> sp_avgq;
    std::map<std::string, int> sp_q4;
    for (const auto& r : records) {
        sp_count[r.species]++;
        sp_avgq[r.species] += r.quality_score;
        if (r.quality_tier == QualityTier::Q4) sp_q4[r.species]++;
    }
    f << "<table>\n<tr><th>Species</th><th>Class</th><th>Records</th>"
      << "<th>Q4</th><th>Avg Score</th></tr>\n";
    for (const auto& [sp, cnt] : sp_count) {
        double avg = sp_avgq[sp] / cnt;
        f << "<tr><td><strong>" << sp << "</strong></td>"
          << "<td>" << species_class_name(classify_species(sp)) << "</td>"
          << "<td>" << cnt << "</td>"
          << "<td>" << sp_q4[sp] << "</td>"
          << "<td>" << std::fixed << std::setprecision(1) << avg << "</td></tr>\n";
    }
    f << "</table>\n";

    // Manifest
    f << "<h3>Run Manifest</h3>\n";
    f << "<pre>" << manifest.to_json() << "</pre>\n";
    f << "</div>\n"; // panel-overview

    // ========================================================================
    // TAB: 2D Charts
    // ========================================================================
    f << "<div class=\"tab-panel\" id=\"panel-charts\">\n";
    f << "<h2>2D Diagnostic Charts</h2>\n";
    f << "<div class=\"plot-container\" id=\"plot-z-vs-t\" style=\"height:500px\"></div>\n";
    f << "<div class=\"plot-container\" id=\"plot-z-vs-p\" style=\"height:500px\"></div>\n";
    f << "<div class=\"plot-container\" id=\"plot-vrms-vs-t\" style=\"height:500px\"></div>\n";
    f << "<div class=\"plot-container\" id=\"plot-residual-hist\" style=\"height:400px\"></div>\n";
    f << "<div class=\"plot-container\" id=\"plot-quality-hist\" style=\"height:400px\"></div>\n";

    // Build per-species data for 2D plots (Q2+ only, vdW model, sampled)
    f << "<script>\n";
    f << "function initPlot_charts() {\n";
    f << "var bgcolor = getComputedStyle(document.body).getPropertyValue('--bg').trim();\n";
    f << "var fgcolor = getComputedStyle(document.body).getPropertyValue('--fg').trim();\n";
    f << "var layout2d = {paper_bgcolor:bgcolor,plot_bgcolor:bgcolor,"
      << "font:{color:fgcolor},margin:{l:60,r:30,t:40,b:50}};\n";

    // Collect species set
    std::set<std::string> species_set;
    for (const auto& r : records)
        if (static_cast<int>(r.quality_tier) >= 2) species_set.insert(r.species);

    // Z vs T traces per species (vdW only for clarity)
    f << "// Z vs T\nvar zT_traces = [\n";
    for (const auto& sp : species_set) {
        f << "  {x:[";
        bool first = true;
        for (const auto& r : records) {
            if (r.species == sp && r.model_name == "vdW" &&
                static_cast<int>(r.quality_tier) >= 2 && !std::isnan(r.Z)) {
                if (!first) f << ",";
                f << std::fixed << std::setprecision(1) << r.T_K;
                first = false;
            }
        }
        f << "],y:[";
        first = true;
        for (const auto& r : records) {
            if (r.species == sp && r.model_name == "vdW" &&
                static_cast<int>(r.quality_tier) >= 2 && !std::isnan(r.Z)) {
                if (!first) f << ",";
                f << std::setprecision(6) << r.Z;
                first = false;
            }
        }
        f << "],mode:'markers',name:'" << sp << "',marker:{size:4}},\n";
    }
    f << "];\n";
    f << "Plotly.newPlot('plot-z-vs-t',zT_traces,{...layout2d,"
      << "title:'Z vs Temperature (VdW, Q2+)',xaxis:{title:'T (K)'},yaxis:{title:'Z'}});\n";

    // Z vs P
    f << "var zP_traces = [\n";
    for (const auto& sp : species_set) {
        f << "  {x:[";
        bool first = true;
        for (const auto& r : records) {
            if (r.species == sp && r.model_name == "vdW" &&
                static_cast<int>(r.quality_tier) >= 2 && !std::isnan(r.Z)) {
                if (!first) f << ",";
                f << std::setprecision(2) << r.P_atm();
                first = false;
            }
        }
        f << "],y:[";
        first = true;
        for (const auto& r : records) {
            if (r.species == sp && r.model_name == "vdW" &&
                static_cast<int>(r.quality_tier) >= 2 && !std::isnan(r.Z)) {
                if (!first) f << ",";
                f << std::setprecision(6) << r.Z;
                first = false;
            }
        }
        f << "],mode:'markers',name:'" << sp << "',marker:{size:4}},\n";
    }
    f << "];\n";
    f << "Plotly.newPlot('plot-z-vs-p',zP_traces,{...layout2d,"
      << "title:'Z vs Pressure (VdW, Q2+)',xaxis:{title:'P (atm)',type:'log'},yaxis:{title:'Z'}});\n";

    // v_rms vs T
    f << "var vT_traces = [\n";
    for (const auto& sp : species_set) {
        f << "  {x:[";
        bool first = true;
        for (const auto& r : records) {
            if (r.species == sp && r.model_name == "ideal" &&
                static_cast<int>(r.quality_tier) >= 2 && !std::isnan(r.v_rms)) {
                if (!first) f << ",";
                f << std::setprecision(1) << r.T_K;
                first = false;
            }
        }
        f << "],y:[";
        first = true;
        for (const auto& r : records) {
            if (r.species == sp && r.model_name == "ideal" &&
                static_cast<int>(r.quality_tier) >= 2 && !std::isnan(r.v_rms)) {
                if (!first) f << ",";
                f << std::setprecision(1) << r.v_rms;
                first = false;
            }
        }
        f << "],mode:'lines+markers',name:'" << sp << "',marker:{size:3}},\n";
    }
    f << "];\n";
    f << "Plotly.newPlot('plot-vrms-vs-t',vT_traces,{...layout2d,"
      << "title:'RMS Speed vs Temperature (Ideal)',xaxis:{title:'T (K)'},yaxis:{title:'v_rms (m/s)'}});\n";

    // Residual histogram
    f << "var residuals = [";
    {
        bool first = true;
        for (const auto& r : records) {
            if (!std::isnan(r.residual) && r.residual > 0 && r.residual < 1.0) {
                if (!first) f << ",";
                f << std::scientific << std::setprecision(3) << r.residual;
                first = false;
            }
        }
    }
    f << std::fixed << "];\n";
    f << "Plotly.newPlot('plot-residual-hist',"
      << "[{x:residuals,type:'histogram',nbinsx:50,marker:{color:'#2563eb'}}],"
      << "{...layout2d,title:'Residual Distribution',xaxis:{title:'Residual',type:'log'},"
      << "yaxis:{title:'Count'}});\n";

    // Quality score histogram
    f << "var qscores = [";
    {
        bool first = true;
        for (const auto& r : records) {
            if (!first) f << ",";
            f << std::fixed << std::setprecision(1) << r.quality_score;
            first = false;
        }
    }
    f << "];\n";
    f << "Plotly.newPlot('plot-quality-hist',"
      << "[{x:qscores,type:'histogram',nbinsx:20,"
      << "marker:{color:qscores,colorscale:'RdYlGn',cmin:0,cmax:100}}],"
      << "{...layout2d,title:'Quality Score Distribution',xaxis:{title:'Score'},"
      << "yaxis:{title:'Count'}});\n";

    f << "}\n</script>\n";
    f << "</div>\n"; // panel-charts

    // ========================================================================
    // TAB: 3D Surfaces
    // ========================================================================
    f << "<div class=\"tab-panel\" id=\"panel-surfaces\">\n";
    f << "<h2>3D Thermodynamic Surfaces</h2>\n";
    f << "<div class=\"plot-container\" id=\"plot-pvt-surface\" style=\"height:650px\"></div>\n";
    f << "<div class=\"plot-container\" id=\"plot-z-surface\" style=\"height:650px\"></div>\n";
    f << "<div class=\"plot-container\" id=\"plot-quality-3d\" style=\"height:650px\"></div>\n";

    f << "<script>\n";
    f << "function initPlot_surfaces() {\n";
    f << "var bgcolor = getComputedStyle(document.body).getPropertyValue('--bg').trim();\n";
    f << "var fgcolor = getComputedStyle(document.body).getPropertyValue('--fg').trim();\n";
    f << "var layout3d = {paper_bgcolor:bgcolor,font:{color:fgcolor},"
      << "margin:{l:0,r:0,t:40,b:0}};\n";

    // Build T/P/V/Z arrays from Q2+ vdW records for 3D scatter
    f << "var sT=[],sP=[],sV=[],sZ=[],sQ=[],sCol=[],sSp=[];\n";
    for (const auto& r : records) {
        if (r.model_name == "vdW" && static_cast<int>(r.quality_tier) >= 2 &&
            !std::isnan(r.Z) && !std::isnan(r.V_m3) && r.V_m3 > 0) {
            f << "sT.push(" << std::fixed << std::setprecision(1) << r.T_K << ");";
            f << "sP.push(" << std::setprecision(2) << r.P_atm() << ");";
            f << "sV.push(" << std::setprecision(6) << (r.V_m3 * 1000.0) << ");";
            f << "sZ.push(" << std::setprecision(6) << r.Z << ");";
            f << "sQ.push(" << std::setprecision(1) << r.quality_score << ");";
            f << "sSp.push('" << r.species << "');\n";
        }
    }

    // PvT surface — 3D scatter colored by species
    f << "var pvt_trace = {x:sT,y:sP,z:sV,mode:'markers',type:'scatter3d',"
      << "marker:{size:3,color:sZ,colorscale:'Viridis',showscale:true,"
      << "colorbar:{title:'Z'}},text:sSp,hovertemplate:"
      << "'T=%{x} K<br>P=%{y} atm<br>V=%{z} L<br>%{text}<extra></extra>'};\n";
    f << "Plotly.newPlot('plot-pvt-surface',[pvt_trace],{...layout3d,"
      << "title:'PvT State Space (VdW, Q2+)',"
      << "scene:{xaxis:{title:'T (K)'},yaxis:{title:'P (atm)'},zaxis:{title:'V (L)'}}});\n";

    // Z(T,P) surface — mesh3d or scatter colored by Z
    f << "var z_trace = {x:sT,y:sP,z:sZ,mode:'markers',type:'scatter3d',"
      << "marker:{size:3,color:sZ,colorscale:'RdYlGn',showscale:true,"
      << "colorbar:{title:'Z'}},text:sSp,hovertemplate:"
      << "'T=%{x} K<br>P=%{y} atm<br>Z=%{z}<br>%{text}<extra></extra>'};\n";
    f << "Plotly.newPlot('plot-z-surface',[z_trace],{...layout3d,"
      << "title:'Compressibility Z(T,P) Surface (VdW, Q2+)',"
      << "scene:{xaxis:{title:'T (K)'},yaxis:{title:'P (atm)'},zaxis:{title:'Z'}}});\n";

    // Quality 3D — T,P,Score colored by tier
    f << "var q_trace = {x:sT,y:sP,z:sQ,mode:'markers',type:'scatter3d',"
      << "marker:{size:3,color:sQ,colorscale:'RdYlGn',cmin:0,cmax:100,"
      << "showscale:true,colorbar:{title:'Score'}},text:sSp,hovertemplate:"
      << "'T=%{x} K<br>P=%{y} atm<br>Score=%{z}<br>%{text}<extra></extra>'};\n";
    f << "Plotly.newPlot('plot-quality-3d',[q_trace],{...layout3d,"
      << "title:'Quality Score Landscape',"
      << "scene:{xaxis:{title:'T (K)'},yaxis:{title:'P (atm)'},zaxis:{title:'Score'}}});\n";

    f << "}\n</script>\n";
    f << "</div>\n"; // panel-surfaces

    // ========================================================================
    // TAB: Data Tables
    // ========================================================================
    f << "<div class=\"tab-panel\" id=\"panel-data\">\n";

    // Top Q4
    f << "<h2>Reference States (Q4)</h2>\n";
    f << "<table>\n<tr><th>Species</th><th>Model</th><th>T (K)</th><th>P (atm)</th>"
      << "<th>Z</th><th>v_rms</th><th>Cp</th><th>Cv</th><th>gamma</th>"
      << "<th>Score</th><th>Tier</th></tr>\n";
    { int cnt = 0;
    for (const auto& r : records) {
        if (r.quality_tier == QualityTier::Q4 && cnt < 100) {
            f << "<tr class=\"q4\"><td>" << r.species << "</td>"
              << "<td>" << r.model_name << "</td>"
              << "<td>" << std::fixed << std::setprecision(1) << r.T_K << "</td>"
              << "<td>" << std::setprecision(2) << r.P_atm() << "</td>"
              << "<td>" << std::setprecision(6) << r.Z << "</td>"
              << "<td>" << std::setprecision(0) << r.v_rms << "</td>"
              << "<td>" << std::setprecision(2) << r.Cp_JmolK << "</td>"
              << "<td>" << std::setprecision(2) << r.Cv_JmolK << "</td>"
              << "<td>" << std::setprecision(3) << r.gamma << "</td>"
              << "<td>" << std::setprecision(1) << r.quality_score << "</td>"
              << "<td><span class=\"" << detail::badge_class(r.quality_tier) << "\">"
              << tier_name(r.quality_tier) << "</span></td></tr>\n";
            cnt++;
        }
    }}

    f << "</table>\n";

    // Failure table
    f << "<h2>Problem States (Q0/Q1)</h2>\n";
    f << "<table>\n<tr><th>Species</th><th>Model</th><th>T (K)</th><th>P (atm)</th>"
      << "<th>Z</th><th>Residual</th><th>Iters</th>"
      << "<th>Score</th><th>Tier</th><th>Warnings</th></tr>\n";
    { int cnt = 0;
    for (const auto& r : records) {
        if ((r.quality_tier == QualityTier::Q0 || r.quality_tier == QualityTier::Q1) && cnt < 100) {
            f << "<tr class=\"" << detail::tier_css_class(r.quality_tier) << "\">"
              << "<td>" << r.species << "</td>"
              << "<td>" << r.model_name << "</td>"
              << "<td>" << std::fixed << std::setprecision(1) << r.T_K << "</td>"
              << "<td>" << std::setprecision(2) << r.P_atm() << "</td>"
              << "<td>" << std::setprecision(6) << r.Z << "</td>"
              << "<td>" << std::scientific << std::setprecision(2) << r.residual << "</td>"
              << std::fixed
              << "<td>" << r.iterations << "</td>"
              << "<td>" << std::setprecision(1) << r.quality_score << "</td>"
              << "<td><span class=\"" << detail::badge_class(r.quality_tier) << "\">"
              << tier_name(r.quality_tier) << "</span></td>"
              << "<td style=\"font-size:11px\">" << r.warning_flags << "</td></tr>\n";
            cnt++;
        }
    }}
    f << "</table>\n";

    // Full data (first 500)
    f << "<h2>Full Dataset (first 500)</h2>\n";
    f << "<div style=\"overflow-x:auto;max-height:600px;overflow-y:auto\">\n<table>\n";
    f << "<tr><th>#</th><th>Species</th><th>Model</th><th>Mode</th>"
      << "<th>T(K)</th><th>P(atm)</th><th>V(L)</th><th>Z</th>"
      << "<th>Tr</th><th>Pr</th><th>v_rms</th>"
      << "<th>Cp</th><th>Cv</th><th>gamma</th>"
      << "<th>Score</th><th>Tier</th></tr>\n";
    for (size_t i = 0; i < std::min(records.size(), size_t(500)); ++i) {
        const auto& r = records[i];
        f << "<tr class=\"" << detail::tier_css_class(r.quality_tier) << "\">"
          << "<td>" << i << "</td>"
          << "<td>" << r.species << "</td>"
          << "<td>" << r.model_name << "</td>"
          << "<td>" << sample_mode_name(r.sample_mode) << "</td>"
          << "<td>" << std::fixed << std::setprecision(1) << r.T_K << "</td>"
          << "<td>" << std::setprecision(2) << r.P_atm() << "</td>"
          << "<td>" << std::setprecision(4) << (r.V_m3 * 1000.0) << "</td>"
          << "<td>" << std::setprecision(6) << r.Z << "</td>"
          << "<td>" << std::setprecision(3) << r.Tr << "</td>"
          << "<td>" << std::setprecision(3) << r.Pr << "</td>"
          << "<td>" << std::setprecision(0) << r.v_rms << "</td>"
          << "<td>" << std::setprecision(2) << r.Cp_JmolK << "</td>"
          << "<td>" << std::setprecision(2) << r.Cv_JmolK << "</td>"
          << "<td>" << std::setprecision(3) << r.gamma << "</td>"
          << "<td>" << std::setprecision(1) << r.quality_score << "</td>"
          << "<td><span class=\"" << detail::badge_class(r.quality_tier) << "\">"
          << tier_name(r.quality_tier) << "</span></td></tr>\n";
    }
    f << "</table>\n</div>\n";
    f << "</div>\n"; // panel-data

    // ========================================================================
    // TAB: Fitting (post-rendering)
    // ========================================================================
    f << "<div class=\"tab-panel\" id=\"panel-fitting\">\n";
    f << "<h2>Curve Fitting Results</h2>\n";

    if (fits.has_z_surface) {
        const auto& zf = fits.z_surface;
        f << "<h3>Z(T,P) Surface Fit</h3>\n";
        f << "<div class=\"stat-grid\">\n";
        {
            std::ostringstream ss; ss << std::fixed << std::setprecision(6) << zf.r_squared;
            card("R\xC2\xB2", ss.str());
        }
        {
            std::ostringstream ss; ss << std::fixed << std::setprecision(4) << zf.mape << "%";
            card("MAPE", ss.str());
        }
        {
            std::ostringstream ss; ss << std::scientific << std::setprecision(3) << zf.mae;
            card("MAE", ss.str());
        }
        card("Train", std::to_string(zf.n_train));
        card("Valid", std::to_string(zf.n_valid));
        card("Terms", std::to_string(zf.n_terms));
        f << "</div>\n";

        // Coefficient table
        f << "<table>\n<tr><th>Term</th><th>T^i</th><th>P^j</th><th>Coefficient</th></tr>\n";
        for (size_t k = 0; k < zf.coeffs.size(); ++k) {
            f << "<tr><td>" << k << "</td>"
              << "<td>" << zf.powers[k].first << "</td>"
              << "<td>" << zf.powers[k].second << "</td>"
              << "<td>" << std::scientific << std::setprecision(8) << zf.coeffs[k] << "</td></tr>\n";
        }
        f << std::fixed << "</table>\n";

        // Fit surface plot
        f << "<h3>Fitted vs Measured Z</h3>\n";
        f << "<div class=\"plot-container\" id=\"plot-fit-residual\" style=\"height:500px\"></div>\n";
        f << "<script>\n";
        f << "(function(){\n";
        f << "var measured=[],predicted=[];\n";
        // Emit measured vs predicted for vdW Q3+ records
        for (const auto& r : records) {
            if (r.model_name == "vdW" && static_cast<int>(r.quality_tier) >= 3 &&
                !std::isnan(r.Z)) {
                double z_pred = zf.evaluate(r.T_K, r.P_atm());
                f << "measured.push(" << std::setprecision(6) << r.Z << ");";
                f << "predicted.push(" << std::setprecision(6) << z_pred << ");\n";
            }
        }
        f << "var bgcolor=getComputedStyle(document.body).getPropertyValue('--bg').trim();\n";
        f << "var fgcolor=getComputedStyle(document.body).getPropertyValue('--fg').trim();\n";
        f << "Plotly.newPlot('plot-fit-residual',["
          << "{x:measured,y:predicted,mode:'markers',name:'Data',marker:{size:4,color:'#2563eb'}},"
          << "{x:[Math.min(...measured),Math.max(...measured)],"
          << "y:[Math.min(...measured),Math.max(...measured)],"
          << "mode:'lines',name:'Perfect',line:{color:'#dc2626',dash:'dash'}}"
          << "],{paper_bgcolor:bgcolor,plot_bgcolor:bgcolor,font:{color:fgcolor},"
          << "title:'Measured vs Predicted Z',xaxis:{title:'Measured Z'},"
          << "yaxis:{title:'Predicted Z'},margin:{l:60,r:30,t:40,b:50}});\n";
        f << "})();\n</script>\n";
    } else {
        f << "<p style=\"color:var(--muted)\">No surface fit available. "
          << "Run the full pipeline to generate fit data.</p>\n";
    }
    f << "</div>\n"; // panel-fitting

    // ========================================================================
    // TAB: Diagnostics (post-rendering)
    // ========================================================================
    f << "<div class=\"tab-panel\" id=\"panel-diag\">\n";
    f << "<h2>Solver Diagnostics</h2>\n";

    // Convergence by model
    f << "<h3>Convergence by Model</h3>\n";
    std::map<std::string, size_t> model_total, model_conv;
    std::map<std::string, double> model_avg_iter;
    for (const auto& r : records) {
        model_total[r.model_name]++;
        if (r.converged) model_conv[r.model_name]++;
        model_avg_iter[r.model_name] += r.iterations;
    }
    f << "<table>\n<tr><th>Model</th><th>Total</th><th>Converged</th>"
      << "<th>Rate</th><th>Avg Iterations</th></tr>\n";
    for (const auto& [m, tot] : model_total) {
        double rate = tot > 0 ? 100.0 * model_conv[m] / tot : 0.0;
        double avg_it = tot > 0 ? model_avg_iter[m] / tot : 0.0;
        f << "<tr><td><strong>" << m << "</strong></td>"
          << "<td>" << tot << "</td>"
          << "<td>" << model_conv[m] << "</td>"
          << "<td>" << std::fixed << std::setprecision(1) << rate << "%</td>"
          << "<td>" << std::setprecision(1) << avg_it << "</td></tr>\n";
    }
    f << "</table>\n";

    // Quality by sample mode
    f << "<h3>Quality by Sample Mode</h3>\n";
    std::map<std::string, size_t> mode_total;
    std::map<std::string, double> mode_avg_q;
    for (const auto& r : records) {
        std::string mname = sample_mode_name(r.sample_mode);
        mode_total[mname]++;
        mode_avg_q[mname] += r.quality_score;
    }
    f << "<table>\n<tr><th>Mode</th><th>Records</th><th>Avg Score</th></tr>\n";
    for (const auto& [mode, tot] : mode_total) {
        double avg = tot > 0 ? mode_avg_q[mode] / tot : 0.0;
        f << "<tr><td>" << mode << "</td><td>" << tot << "</td>"
          << "<td>" << std::fixed << std::setprecision(1) << avg << "</td></tr>\n";
    }
    f << "</table>\n";

    // Iteration count distribution plot
    f << "<div class=\"plot-container\" id=\"plot-iter-hist\" style=\"height:400px\"></div>\n";
    f << "<script>\n(function(){\n";
    f << "var iters=[";
    { bool first = true;
    for (const auto& r : records) {
        if (!first) f << ",";
        f << r.iterations;
        first = false;
    }}
    f << "];\n";
    f << "var bgcolor=getComputedStyle(document.body).getPropertyValue('--bg').trim();\n";
    f << "var fgcolor=getComputedStyle(document.body).getPropertyValue('--fg').trim();\n";
    f << "Plotly.newPlot('plot-iter-hist',"
      << "[{x:iters,type:'histogram',nbinsx:20,marker:{color:'#7c3aed'}}],"
      << "{paper_bgcolor:bgcolor,plot_bgcolor:bgcolor,font:{color:fgcolor},"
      << "title:'Iteration Count Distribution',xaxis:{title:'Iterations'},"
      << "yaxis:{title:'Count'},margin:{l:60,r:30,t:40,b:50}});\n";
    f << "})();\n</script>\n";

    // Worst convergence table
    f << "<h3>Worst Convergence (top 25 by iteration count)</h3>\n";
    {
        auto sorted = records;
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.iterations > b.iterations; });
        f << "<table>\n<tr><th>Species</th><th>Model</th><th>T(K)</th><th>P(atm)</th>"
          << "<th>Iters</th><th>Residual</th><th>Z</th><th>Tier</th></tr>\n";
        for (size_t i = 0; i < std::min(sorted.size(), size_t(25)); ++i) {
            const auto& r = sorted[i];
            f << "<tr class=\"" << detail::tier_css_class(r.quality_tier) << "\">"
              << "<td>" << r.species << "</td>"
              << "<td>" << r.model_name << "</td>"
              << "<td>" << std::fixed << std::setprecision(1) << r.T_K << "</td>"
              << "<td>" << std::setprecision(2) << r.P_atm() << "</td>"
              << "<td><strong>" << r.iterations << "</strong></td>"
              << "<td>" << std::scientific << std::setprecision(2) << r.residual << "</td>"
              << std::fixed
              << "<td>" << std::setprecision(6) << r.Z << "</td>"
              << "<td><span class=\"" << detail::badge_class(r.quality_tier) << "\">"
              << tier_name(r.quality_tier) << "</span></td></tr>\n";
        }
        f << "</table>\n";
    }
    f << "</div>\n"; // panel-diag

    // ========================================================================
    // Tab JS + footer
    // ========================================================================
    detail::emit_tab_js(f);

    f << "<div class=\"footer\">VSEPR-SIM Gas3 v4.0.1 Beta &middot; "
      << manifest.timestamp << " &middot; " << manifest.total_records << " states"
      << " &middot; Deterministic. Inspectable. Anti-black-box.</div>\n";
    f << "</body>\n</html>\n";
    return true;
}

// ============================================================================
// Write run manifest JSON
// ============================================================================

inline bool write_manifest(const std::string& path, const RunManifest& manifest) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << manifest.to_json() << "\n";
    return true;
}

} // namespace gas3
} // namespace vsepr
