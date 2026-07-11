/*
 * test_chart_data.cpp — Compile-and-run validation for chart_data.hpp
 *
 * Verifies that all containers (Series, DataTable, ChartSpec, FigureManifest,
 * TimeseriesRecord, PropertyCard, ExportConfig) compile, construct, populate,
 * and export to CSV / JSON / LaTeX without error.
 *
 * Build:
 *   g++ -std=c++20 -I../../include -o test_chart_data test_chart_data.cpp
 *   ./test_chart_data
 */

#include "chart/chart_data.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace vsepr::chart;

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        std::cout << "  [PASS] " << name << "\n"; \
        pass_count++; \
    } else { \
        std::cout << "  [FAIL] " << name << "\n"; \
        fail_count++; \
    } \
} while(0)

// ── Series ──────────────────────────────────────────────────────────────

void test_series() {
    Series s("temperature", "K");
    s.push(300.0);
    s.push(350.0);
    s.push(400.0);

    TEST("series_size", s.size() == 3);
    TEST("series_min", s.min_val() == 300.0);
    TEST("series_max", s.max_val() == 400.0);
    TEST("series_mean", std::abs(s.mean() - 350.0) < 1e-12);
    TEST("series_stddev", s.stddev() > 0.0);
    TEST("series_csv", s.to_csv_column().find("temperature") != std::string::npos);
}

// ── DataTable ───────────────────────────────────────────────────────────

void test_datatable() {
    DataTable dt("stress_strain");
    dt.add_column("strain_pct", ColumnType::Float, "%");
    dt.add_column("stress_MPa", ColumnType::Float, "MPa");
    dt.add_column("material",   ColumnType::String);

    dt.add_row({"0.0", "0.0", "Fe"});
    dt.add_row({"1.5", "310.2", "Fe"});
    dt.add_row({"5.0", "450.0", "Fe"});

    TEST("dt_cols", dt.num_cols() == 3);
    TEST("dt_rows", dt.num_rows() == 3);
    TEST("dt_cell", dt.cell(1, 1) == "310.2");
    TEST("dt_cell_float", std::abs(dt.cell_float(2, 1) - 450.0) < 1e-12);

    // CSV
    std::string csv = dt.to_csv();
    TEST("dt_csv_header", csv.find("strain_pct") != std::string::npos);
    TEST("dt_csv_data", csv.find("310.2") != std::string::npos);

    // JSON
    std::string json = dt.to_json();
    TEST("dt_json_table", json.find("\"stress_strain\"") != std::string::npos);
    TEST("dt_json_cols", json.find("\"columns\"") != std::string::npos);
    TEST("dt_json_rows", json.find("\"rows\"") != std::string::npos);

    // LaTeX
    std::string tex = dt.to_latex_table("Stress-Strain Data", "tab:ss");
    TEST("dt_latex_begin", tex.find("\\begin{table}") != std::string::npos);
    TEST("dt_latex_booktabs", tex.find("\\toprule") != std::string::npos);
    TEST("dt_latex_caption", tex.find("Stress-Strain Data") != std::string::npos);

    // Column as Series
    Series col = dt.column_as_series(1);
    TEST("dt_series_name", col.name == "stress_MPa");
    TEST("dt_series_size", col.size() == 3);

    // Map insertion
    dt.add_row_map({{"strain_pct", "10.0"}, {"stress_MPa", "500.0"}, {"material", "Fe"}});
    TEST("dt_map_insert", dt.num_rows() == 4);
}

// ── ChartSpec ───────────────────────────────────────────────────────────

void test_chartspec() {
    ChartSpec cs;
    cs.id = "stress_strain_Fe";
    cs.type = ChartType::Line;
    cs.title = "Stress vs Strain (Fe)";
    cs.xlabel = "Engineering Strain (%)";
    cs.ylabel = "Stress (MPa)";
    cs.x_col = 0;
    cs.y_cols = {1};s
    cs.series_labels = {"Fe"};
    cs.series_colors = {"#e74c3c"};

    std::string json = cs.to_json();
    TEST("cs_json_id", json.find("\"stress_strain_Fe\"") != std::string::npos);
    TEST("cs_json_type", json.find("\"line\"") != std::string::npos);
    TEST("cs_json_title", json.find("Stress vs Strain") != std::string::npos);
    TEST("cs_json_cols", json.find("\"y_cols\": [1]") != std::string::npos);
}

// ── FigureManifest ──────────────────────────────────────────────────────

void test_manifest() {
    FigureManifest fm("demo_report", ".");
    fm.data.add_column("x", ColumnType::Float);
    fm.data.add_column("y", ColumnType::Float);
    fm.data.add_row({"0", "0"});
    fm.data.add_row({"1", "1"});

    ChartSpec cs;
    cs.id = "demo_line";
    cs.type = ChartType::Line;
    cs.title = "Demo";
    cs.x_col = 0;
    cs.y_cols = {1};
    fm.add_chart(cs);

    TEST("fm_charts", fm.charts.size() == 1);
    TEST("fm_data", fm.data.num_rows() == 2);
}

// ── TimeseriesRecord ────────────────────────────────────────────────────

void test_timeseries() {
    TimeseriesRecord ts("energy", "formation_engine",
                        "step", "U_total", "", "kcal/mol");
    ts.push(0, -512.3);
    ts.push(100, -515.1);
    ts.push(200, -516.8);

    TEST("ts_size", ts.size() == 3);

    std::string csv = ts.to_csv();
    TEST("ts_csv_header", csv.find("step") != std::string::npos);
    TEST("ts_csv_provenance", csv.find("formation_engine") != std::string::npos);

    std::string json = ts.to_json();
    TEST("ts_json_name", json.find("\"energy\"") != std::string::npos);
    TEST("ts_json_x", json.find("-512.3") != std::string::npos);
}

// ── PropertyCard ────────────────────────────────────────────────────────

void test_propertycard() {
    PropertyCard pc("Fe_properties");
    pc.add("Symbol", "Fe");
    pc.add("Atomic Number", 26);
    pc.add("Density", 7.874, "g/cm3", 4);
    pc.add("Melting Point", 1811.0, "K", 1);

    TEST("pc_entries", pc.entries.size() == 4);

    std::string csv = pc.to_csv();
    TEST("pc_csv", csv.find("Density") != std::string::npos);

    std::string json = pc.to_json();
    TEST("pc_json_card", json.find("\"Fe_properties\"") != std::string::npos);

    std::string tex = pc.to_latex();
    TEST("pc_latex", tex.find("\\begin{description}") != std::string::npos);
}

// ── ExportConfig ────────────────────────────────────────────────────────

void test_exportconfig() {
    ExportConfig ec;
    ec.output_dir = "docs/figures";
    ec.dpi = 300;

    std::string json = ec.to_json();
    TEST("ec_json_dir", json.find("docs/figures") != std::string::npos);
    TEST("ec_json_dpi", json.find("300") != std::string::npos);
}

// ── LaTeX generators ────────────────────────────────────────────────────

void test_latex_generators() {
    std::string fig = latex_figure("demo.png", "A demo", "fig:demo");
    TEST("lfig_include", fig.find("\\includegraphics") != std::string::npos);

    std::string pair = latex_subfigure_pair(
        "a.png", "Left", "b.png", "Right", "Pair", "fig:pair");
    TEST("lpair_subfig", pair.find("\\begin{subfigure}") != std::string::npos);

    std::vector<std::string> figs = {"a.png", "b.png", "c.png", "d.png"};
    std::vector<std::string> caps = {"A", "B", "C", "D"};
    std::string grid = latex_subfigure_grid(figs, caps, "Grid", "fig:grid");
    TEST("lgrid_count", grid.find("\\begin{subfigure}") != std::string::npos);
}

// ── Enumerations ────────────────────────────────────────────────────────

void test_enums() {
    TEST("coltype_int",   std::string(column_type_name(ColumnType::Int)) == "int");
    TEST("coltype_float", std::string(column_type_name(ColumnType::Float)) == "float");
    TEST("coltype_str",   std::string(column_type_name(ColumnType::String)) == "string");

    TEST("chart_line",    std::string(chart_type_name(ChartType::Line)) == "line");
    TEST("chart_scatter", std::string(chart_type_name(ChartType::Scatter)) == "scatter");
    TEST("chart_bar",     std::string(chart_type_name(ChartType::Bar)) == "bar");
    TEST("chart_heatmap", std::string(chart_type_name(ChartType::HeatMap)) == "heatmap");
    TEST("chart_radar",   std::string(chart_type_name(ChartType::Radar)) == "radar");
}

// ════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "=== chart_data.hpp test suite ===\n\n";

    test_series();
    test_datatable();
    test_chartspec();
    test_manifest();
    test_timeseries();
    test_propertycard();
    test_exportconfig();
    test_latex_generators();
    test_enums();

    std::cout << "\n=== " << (fail_count == 0 ? "ALL" : "SOME")
              << " " << pass_count << " TESTS "
              << (fail_count == 0 ? "PASSED" : "HAD FAILURES")
              << " ===" << std::endl;

    if (fail_count > 0) {
        std::cout << fail_count << " test(s) failed.\n";
        return 1;
    }
    return 0;
}
