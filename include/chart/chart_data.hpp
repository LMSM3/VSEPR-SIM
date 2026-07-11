#pragma once
/*
 * chart_data.hpp — Portable data containers for chart and document automation.
 *
 * Provides structured data types with built-in CSV, JSON, and LaTeX export.
 * Every container is header-only, deterministic, and traces to inspectable fields.
 *
 * Containers:
 *   Series           — Named 1-D data column (x-values, y-values, labels)
 *   DataTable         — Named 2-D table (rows × typed columns, CSV/LaTeX out)
 *   ChartSpec         — Chart description (type, series refs, axis labels, title)
 *   FigureManifest    — Collection of ChartSpecs for batch figure generation
 *   TimeseriesRecord  — Step-indexed time-series with provenance
 *   PropertyCard      — Key-value property bag with units
 *   ExportConfig      — Output path, DPI, format selection
 *
 * Design rules:
 *   - No hidden state: every field is public and inspectable
 *   - Deterministic: same inputs → identical export strings
 *   - Anti-black-box: export methods are transparent string builders
 *
 * Usage (C++20):
 *   #include "chart/chart_data.hpp"
 *   using namespace vsepr::chart;
 *
 *   DataTable dt("stress_strain");
 *   dt.add_column("strain_pct", ColumnType::Float);
 *   dt.add_column("stress_MPa", ColumnType::Float);
 *   dt.add_row({{"strain_pct","0.0"},{"stress_MPa","0.0"}});
 *   dt.add_row({{"strain_pct","1.5"},{"stress_MPa","310.2"}});
 *
 *   // Export
 *   std::string csv   = dt.to_csv();
 *   std::string json  = dt.to_json();
 *   std::string latex = dt.to_latex_table("Stress-Strain Data", "tab:ss");
 *
 * VSEPR-SIM 3.0.0
 */

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <functional>
#include <numeric>

namespace vsepr {
namespace chart {

// ═══════════════════════════════════════════════════════════════════════
// Forward declarations
// ═══════════════════════════════════════════════════════════════════════

struct Series;
struct DataTable;
struct ChartSpec;
struct FigureManifest;
struct TimeseriesRecord;
struct PropertyCard;
struct ExportConfig;

// ═══════════════════════════════════════════════════════════════════════
// Enumerations
// ═══════════════════════════════════════════════════════════════════════

enum class ColumnType : uint8_t {
    Int    = 0,
    Float  = 1,
    String = 2,
};

inline const char* column_type_name(ColumnType t) {
    switch (t) {
        case ColumnType::Int:    return "int";
        case ColumnType::Float:  return "float";
        case ColumnType::String: return "string";
    }
    return "unknown";
}

enum class ChartType : uint8_t {
    Line          = 0,
    Scatter       = 1,
    Bar           = 2,
    BarH          = 3,
    Histogram     = 4,
    HeatMap       = 5,
    Radar         = 6,
    BoxDiagram    = 7,
    StackedArea   = 8,
    PieChart      = 9,
};

inline const char* chart_type_name(ChartType t) {
    switch (t) {
        case ChartType::Line:       return "line";
        case ChartType::Scatter:    return "scatter";
        case ChartType::Bar:        return "bar";
        case ChartType::BarH:       return "barh";
        case ChartType::Histogram:  return "histogram";
        case ChartType::HeatMap:    return "heatmap";
        case ChartType::Radar:      return "radar";
        case ChartType::BoxDiagram: return "box";
        case ChartType::StackedArea:return "stacked_area";
        case ChartType::PieChart:   return "pie";
    }
    return "unknown";
}

enum class ExportFormat : uint8_t {
    CSV   = 0,
    JSON  = 1,
    LaTeX = 2,
};

// ═══════════════════════════════════════════════════════════════════════
// Series — Named 1-D data column
// ═══════════════════════════════════════════════════════════════════════

struct Series {
    std::string name;
    std::string unit;
    std::vector<double> values;
    std::vector<std::string> labels;  // optional categorical labels

    Series() = default;
    explicit Series(const std::string& n, const std::string& u = "")
        : name(n), unit(u) {}

    void push(double v) { values.push_back(v); }
    void push(double v, const std::string& lbl) {
        values.push_back(v);
        labels.push_back(lbl);
    }

    size_t size() const { return values.size(); }
    bool   empty() const { return values.empty(); }

    // ── Statistics ──
    double min_val() const {
        if (values.empty()) return 0.0;
        return *std::min_element(values.begin(), values.end());
    }
    double max_val() const {
        if (values.empty()) return 0.0;
        return *std::max_element(values.begin(), values.end());
    }
    double mean() const {
        if (values.empty()) return 0.0;
        return std::accumulate(values.begin(), values.end(), 0.0)
               / static_cast<double>(values.size());
    }
    double stddev() const {
        if (values.size() < 2) return 0.0;
        double m = mean();
        double acc = 0.0;
        for (double v : values) acc += (v - m) * (v - m);
        return std::sqrt(acc / static_cast<double>(values.size() - 1));
    }

    // ── Export ──
    std::string to_csv_column() const {
        std::ostringstream out;
        out << name;
        if (!unit.empty()) out << " (" << unit << ")";
        out << '\n';
        out << std::setprecision(12);
        for (double v : values) out << v << '\n';
        return out.str();
    }
};

// ═══════════════════════════════════════════════════════════════════════
// DataTable — Named 2-D table (rows × typed columns)
// ═══════════════════════════════════════════════════════════════════════

struct DataTable {
    std::string name;
    std::vector<std::string>  col_names;
    std::vector<ColumnType>   col_types;
    std::vector<std::string>  col_units;

    // Row storage: each row is a vector of string values
    std::vector<std::vector<std::string>> rows;

    DataTable() = default;
    explicit DataTable(const std::string& n) : name(n) {}

    // ── Schema ──
    void add_column(const std::string& col_name,
                    ColumnType type = ColumnType::Float,
                    const std::string& unit = "") {
        col_names.push_back(col_name);
        col_types.push_back(type);
        col_units.push_back(unit);
    }

    size_t num_cols() const { return col_names.size(); }
    size_t num_rows() const { return rows.size(); }

    // ── Row insertion ──
    void add_row(const std::vector<std::string>& row) {
        assert(row.size() == col_names.size());
        rows.push_back(row);
    }

    void add_row_map(const std::map<std::string, std::string>& kv) {
        std::vector<std::string> row(col_names.size());
        for (size_t i = 0; i < col_names.size(); ++i) {
            auto it = kv.find(col_names[i]);
            if (it != kv.end()) row[i] = it->second;
        }
        rows.push_back(row);
    }

    // ── Cell access ──
    const std::string& cell(size_t row, size_t col) const {
        return rows[row][col];
    }
    double cell_float(size_t row, size_t col) const {
        return std::stod(rows[row][col]);
    }
    int cell_int(size_t row, size_t col) const {
        return std::stoi(rows[row][col]);
    }

    // ── Column extraction as Series ──
    Series column_as_series(size_t col) const {
        Series s(col_names[col], col_units[col]);
        for (const auto& row : rows) {
            s.push(std::stod(row[col]));
        }
        return s;
    }

    // ── CSV export ──
    std::string to_csv(char delim = ',') const {
        std::ostringstream out;
        // Header
        for (size_t i = 0; i < col_names.size(); ++i) {
            if (i > 0) out << delim;
            out << col_names[i];
        }
        out << '\n';
        // Rows
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) out << delim;
                out << row[i];
            }
            out << '\n';
        }
        return out.str();
    }

    // ── JSON export ──
    std::string to_json(int indent = 2) const {
        std::string pad(indent, ' ');
        std::string pad2(indent * 2, ' ');
        std::string pad3(indent * 3, ' ');

        std::ostringstream out;
        out << "{\n";
        out << pad << "\"table\": \"" << name << "\",\n";

        // Schema
        out << pad << "\"columns\": [\n";
        for (size_t i = 0; i < col_names.size(); ++i) {
            out << pad2 << "{\"name\": \"" << col_names[i]
                << "\", \"type\": \"" << column_type_name(col_types[i]) << "\"";
            if (!col_units[i].empty())
                out << ", \"unit\": \"" << col_units[i] << "\"";
            out << "}";
            if (i + 1 < col_names.size()) out << ",";
            out << "\n";
        }
        out << pad << "],\n";

        // Rows
        out << pad << "\"rows\": [\n";
        for (size_t r = 0; r < rows.size(); ++r) {
            out << pad2 << "[";
            for (size_t c = 0; c < rows[r].size(); ++c) {
                bool is_str = col_types[c] == ColumnType::String;
                if (is_str) out << "\"";
                out << rows[r][c];
                if (is_str) out << "\"";
                if (c + 1 < rows[r].size()) out << ", ";
            }
            out << "]";
            if (r + 1 < rows.size()) out << ",";
            out << "\n";
        }
        out << pad << "]\n";
        out << "}\n";
        return out.str();
    }

    // ── LaTeX table export ──
    std::string to_latex_table(const std::string& caption = "",
                               const std::string& label = "") const {
        std::ostringstream out;
        out << "\\begin{table}[H]\n";
        out << "\\centering\n";
        if (!caption.empty())
            out << "\\caption{" << caption << "}\n";
        if (!label.empty())
            out << "\\label{" << label << "}\n";

        // Column spec
        out << "\\begin{tabular}{";
        for (size_t i = 0; i < col_names.size(); ++i) {
            if (col_types[i] == ColumnType::String)
                out << "l";
            else
                out << "r";
        }
        out << "}\n\\toprule\n";

        // Header
        for (size_t i = 0; i < col_names.size(); ++i) {
            if (i > 0) out << " & ";
            out << "\\textbf{" << latex_escape(col_names[i]) << "}";
            if (!col_units[i].empty())
                out << " (" << latex_escape(col_units[i]) << ")";
        }
        out << " \\\\\n\\midrule\n";

        // Rows
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) out << " & ";
                out << latex_escape(row[i]);
            }
            out << " \\\\\n";
        }

        out << "\\bottomrule\n";
        out << "\\end{tabular}\n";
        out << "\\end{table}\n";
        return out.str();
    }

    // ── Write to file ──
    bool write_csv(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;
        f << to_csv();
        return true;
    }

    bool write_json(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;
        f << to_json();
        return true;
    }

private:
    static std::string latex_escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '_':  out += "\\_"; break;
                case '%':  out += "\\%"; break;
                case '&':  out += "\\&"; break;
                case '#':  out += "\\#"; break;
                case '{':  out += "\\{"; break;
                case '}':  out += "\\}"; break;
                case '~':  out += "\\textasciitilde{}"; break;
                case '^':  out += "\\textasciicircum{}"; break;
                default:   out += c; break;
            }
        }
        return out;
    }
};

// ═══════════════════════════════════════════════════════════════════════
// ChartSpec — Describes a single chart for Python rendering
// ═══════════════════════════════════════════════════════════════════════

struct ChartSpec {
    std::string  id;           // unique figure id, e.g. "stress_strain_Fe"
    ChartType    type = ChartType::Line;
    std::string  title;
    std::string  xlabel;
    std::string  ylabel;

    // Data references: column indices into an associated DataTable
    int          x_col = 0;
    std::vector<int> y_cols;

    // Visual
    double       fig_width  = 10.0;
    double       fig_height = 7.0;
    int          dpi        = 300;
    bool         grid       = true;
    bool         legend     = true;
    std::string  colormap   = "tab10";

    // Optional
    std::string  x_scale;      // "log", "linear"
    std::string  y_scale;
    double       x_min = NAN, x_max = NAN;
    double       y_min = NAN, y_max = NAN;

    // Series labels
    std::vector<std::string> series_labels;
    std::vector<std::string> series_colors;

    // ── Export as JSON spec for Python consumer ──
    std::string to_json() const {
        std::ostringstream out;
        out << "{\n";
        out << "  \"id\": \"" << id << "\",\n";
        out << "  \"type\": \"" << chart_type_name(type) << "\",\n";
        out << "  \"title\": \"" << title << "\",\n";
        out << "  \"xlabel\": \"" << xlabel << "\",\n";
        out << "  \"ylabel\": \"" << ylabel << "\",\n";
        out << "  \"x_col\": " << x_col << ",\n";
        out << "  \"y_cols\": [";
        for (size_t i = 0; i < y_cols.size(); ++i) {
            if (i > 0) out << ", ";
            out << y_cols[i];
        }
        out << "],\n";
        out << "  \"fig_width\": " << fig_width << ",\n";
        out << "  \"fig_height\": " << fig_height << ",\n";
        out << "  \"dpi\": " << dpi << ",\n";
        out << "  \"grid\": " << (grid ? "true" : "false") << ",\n";
        out << "  \"legend\": " << (legend ? "true" : "false") << ",\n";
        out << "  \"colormap\": \"" << colormap << "\",\n";

        if (!x_scale.empty())
            out << "  \"x_scale\": \"" << x_scale << "\",\n";
        if (!y_scale.empty())
            out << "  \"y_scale\": \"" << y_scale << "\",\n";
        if (!std::isnan(x_min)) out << "  \"x_min\": " << x_min << ",\n";
        if (!std::isnan(x_max)) out << "  \"x_max\": " << x_max << ",\n";
        if (!std::isnan(y_min)) out << "  \"y_min\": " << y_min << ",\n";
        if (!std::isnan(y_max)) out << "  \"y_max\": " << y_max << ",\n";

        out << "  \"series_labels\": [";
        for (size_t i = 0; i < series_labels.size(); ++i) {
            if (i > 0) out << ", ";
            out << "\"" << series_labels[i] << "\"";
        }
        out << "],\n";

        out << "  \"series_colors\": [";
        for (size_t i = 0; i < series_colors.size(); ++i) {
            if (i > 0) out << ", ";
            out << "\"" << series_colors[i] << "\"";
        }
        out << "]\n";
        out << "}\n";
        return out.str();
    }
};

// ═══════════════════════════════════════════════════════════════════════
// FigureManifest — Batch of chart specs + associated data
// ═══════════════════════════════════════════════════════════════════════

struct FigureManifest {
    std::string   name;
    std::string   output_dir;
    DataTable     data;
    std::vector<ChartSpec> charts;

    FigureManifest() = default;
    explicit FigureManifest(const std::string& n, const std::string& dir = ".")
        : name(n), output_dir(dir) {}

    void add_chart(const ChartSpec& spec) { charts.push_back(spec); }

    // Write manifest.json + data.csv for Python consumer
    bool write_all() const {
        bool ok = true;
        std::string base = output_dir + "/" + name;

        // Data CSV
        {
            std::ofstream f(base + "_data.csv");
            if (f) f << data.to_csv();
            else ok = false;
        }

        // Manifest JSON
        {
            std::ofstream f(base + "_manifest.json");
            if (!f) return false;
            f << "{\n";
            f << "  \"name\": \"" << name << "\",\n";
            f << "  \"data_file\": \"" << name << "_data.csv\",\n";
            f << "  \"charts\": [\n";
            for (size_t i = 0; i < charts.size(); ++i) {
                f << "    " << charts[i].to_json();
                if (i + 1 < charts.size()) f << ",";
                f << "\n";
            }
            f << "  ]\n";
            f << "}\n";
        }
        return ok;
    }
};

// ═══════════════════════════════════════════════════════════════════════
// TimeseriesRecord — Step-indexed time-series with provenance
// ═══════════════════════════════════════════════════════════════════════

struct TimeseriesRecord {
    std::string name;
    std::string source;       // provenance: which module produced this
    std::string x_label;
    std::string y_label;
    std::string x_unit;
    std::string y_unit;

    std::vector<double> x;    // independent variable (step, time, strain)
    std::vector<double> y;    // dependent variable

    TimeseriesRecord() = default;
    TimeseriesRecord(const std::string& n, const std::string& src,
                     const std::string& xl, const std::string& yl,
                     const std::string& xu = "", const std::string& yu = "")
        : name(n), source(src), x_label(xl), y_label(yl),
          x_unit(xu), y_unit(yu) {}

    void push(double xv, double yv) {
        x.push_back(xv);
        y.push_back(yv);
    }

    size_t size() const { return x.size(); }

    // ── CSV ──
    std::string to_csv() const {
        std::ostringstream out;
        out << "# " << name << " | source: " << source << "\n";
        out << x_label;
        if (!x_unit.empty()) out << " (" << x_unit << ")";
        out << "," << y_label;
        if (!y_unit.empty()) out << " (" << y_unit << ")";
        out << "\n";
        out << std::setprecision(12);
        for (size_t i = 0; i < x.size(); ++i)
            out << x[i] << "," << y[i] << "\n";
        return out.str();
    }

    // ── JSON ──
    std::string to_json() const {
        std::ostringstream out;
        out << std::setprecision(12);
        out << "{\n";
        out << "  \"name\": \"" << name << "\",\n";
        out << "  \"source\": \"" << source << "\",\n";
        out << "  \"x_label\": \"" << x_label << "\",\n";
        out << "  \"y_label\": \"" << y_label << "\",\n";
        out << "  \"x_unit\": \"" << x_unit << "\",\n";
        out << "  \"y_unit\": \"" << y_unit << "\",\n";
        out << "  \"x\": [";
        for (size_t i = 0; i < x.size(); ++i) {
            if (i > 0) out << ", ";
            out << x[i];
        }
        out << "],\n  \"y\": [";
        for (size_t i = 0; i < y.size(); ++i) {
            if (i > 0) out << ", ";
            out << y[i];
        }
        out << "]\n}\n";
        return out.str();
    }
};

// ═══════════════════════════════════════════════════════════════════════
// PropertyCard — Key-value property bag with units
// ═══════════════════════════════════════════════════════════════════════

struct PropertyCard {
    struct Entry {
        std::string key;
        std::string value;
        std::string unit;
    };

    std::string name;
    std::vector<Entry> entries;

    PropertyCard() = default;
    explicit PropertyCard(const std::string& n) : name(n) {}

    void add(const std::string& k, const std::string& v,
             const std::string& u = "") {
        entries.push_back({k, v, u});
    }
    void add(const std::string& k, double v,
             const std::string& u = "", int prec = 6) {
        std::ostringstream os;
        os << std::setprecision(prec) << v;
        entries.push_back({k, os.str(), u});
    }
    void add(const std::string& k, int v, const std::string& u = "") {
        entries.push_back({k, std::to_string(v), u});
    }

    // ── LaTeX description list ──
    std::string to_latex() const {
        std::ostringstream out;
        out << "\\begin{description}[style=nextline]\n";
        for (const auto& e : entries) {
            out << "  \\item[" << e.key << "]";
            out << " " << e.value;
            if (!e.unit.empty()) out << " " << e.unit;
            out << "\n";
        }
        out << "\\end{description}\n";
        return out.str();
    }

    // ── JSON ──
    std::string to_json() const {
        std::ostringstream out;
        out << "{\n";
        out << "  \"card\": \"" << name << "\",\n";
        out << "  \"properties\": {\n";
        for (size_t i = 0; i < entries.size(); ++i) {
            out << "    \"" << entries[i].key << "\": {\"value\": \""
                << entries[i].value << "\"";
            if (!entries[i].unit.empty())
                out << ", \"unit\": \"" << entries[i].unit << "\"";
            out << "}";
            if (i + 1 < entries.size()) out << ",";
            out << "\n";
        }
        out << "  }\n}\n";
        return out.str();
    }

    // ── CSV ──
    std::string to_csv() const {
        std::ostringstream out;
        out << "property,value,unit\n";
        for (const auto& e : entries)
            out << e.key << "," << e.value << "," << e.unit << "\n";
        return out.str();
    }
};

// ═══════════════════════════════════════════════════════════════════════
// ExportConfig — Output settings for the Python rendering pipeline
// ═══════════════════════════════════════════════════════════════════════

struct ExportConfig {
    std::string output_dir = "docs/figures";
    int         dpi = 300;
    double      fig_width  = 10.0;
    double      fig_height = 7.0;
    bool        tight_layout = true;
    std::string font_family = "serif";
    double      font_size = 11.0;
    std::string background = "white";

    std::string to_json() const {
        std::ostringstream out;
        out << "{\n";
        out << "  \"output_dir\": \"" << output_dir << "\",\n";
        out << "  \"dpi\": " << dpi << ",\n";
        out << "  \"fig_width\": " << fig_width << ",\n";
        out << "  \"fig_height\": " << fig_height << ",\n";
        out << "  \"tight_layout\": " << (tight_layout ? "true" : "false") << ",\n";
        out << "  \"font_family\": \"" << font_family << "\",\n";
        out << "  \"font_size\": " << font_size << ",\n";
        out << "  \"background\": \"" << background << "\"\n";
        out << "}\n";
        return out.str();
    }
};

// ═══════════════════════════════════════════════════════════════════════
// LaTeX snippet generators
// ═══════════════════════════════════════════════════════════════════════

// Generate a \includegraphics line for a figure
inline std::string latex_figure(const std::string& filename,
                                const std::string& caption,
                                const std::string& label,
                                double width = 0.95) {
    std::ostringstream out;
    out << "\\begin{figure}[H]\n";
    out << "\\centering\n";
    out << "\\includegraphics[width=" << width << "\\textwidth]{"
        << filename << "}\n";
    out << "\\caption{" << caption << "}\n";
    out << "\\label{" << label << "}\n";
    out << "\\end{figure}\n";
    return out.str();
}

// Generate a side-by-side subfigure pair
inline std::string latex_subfigure_pair(
    const std::string& fig1, const std::string& cap1,
    const std::string& fig2, const std::string& cap2,
    const std::string& overall_caption,
    const std::string& label,
    double sub_width = 0.48) {
    std::ostringstream out;
    out << "\\begin{figure}[H]\n\\centering\n";
    out << "\\begin{subfigure}[b]{" << sub_width << "\\textwidth}\n";
    out << "\\includegraphics[width=\\textwidth]{" << fig1 << "}\n";
    out << "\\caption{" << cap1 << "}\n";
    out << "\\end{subfigure}\n\\hfill\n";
    out << "\\begin{subfigure}[b]{" << sub_width << "\\textwidth}\n";
    out << "\\includegraphics[width=\\textwidth]{" << fig2 << "}\n";
    out << "\\caption{" << cap2 << "}\n";
    out << "\\end{subfigure}\n";
    out << "\\caption{" << overall_caption << "}\n";
    out << "\\label{" << label << "}\n";
    out << "\\end{figure}\n";
    return out.str();
}

// Generate a 2×2 subfigure grid
inline std::string latex_subfigure_grid(
    const std::vector<std::string>& figs,
    const std::vector<std::string>& caps,
    const std::string& overall_caption,
    const std::string& label,
    int cols = 2, double sub_width = 0.48) {
    std::ostringstream out;
    out << "\\begin{figure}[H]\n\\centering\n";
    for (size_t i = 0; i < figs.size(); ++i) {
        out << "\\begin{subfigure}[b]{" << sub_width << "\\textwidth}\n";
        out << "\\includegraphics[width=\\textwidth]{" << figs[i] << "}\n";
        if (i < caps.size())
            out << "\\caption{" << caps[i] << "}\n";
        out << "\\end{subfigure}\n";
        if ((i + 1) % cols == 0 && i + 1 < figs.size())
            out << "\\\\[0.5cm]\n";
        else if ((i + 1) % cols != 0)
            out << "\\hfill\n";
    }
    out << "\\caption{" << overall_caption << "}\n";
    out << "\\label{" << label << "}\n";
    out << "\\end{figure}\n";
    return out.str();
}

} // namespace chart
} // namespace vsepr
