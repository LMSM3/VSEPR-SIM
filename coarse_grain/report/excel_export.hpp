#pragma once
/**
 * excel_export.hpp — Excel-Compatible OOXML (.xlsx) Export
 *
 * Generates an Office Open XML spreadsheet from SnapshotGraphCollector data.
 * The output can be opened directly in Microsoft Excel, LibreOffice Calc,
 * or any OOXML-compatible application.
 *
 * Format: Minimal OOXML (xlsx = ZIP of XML parts).
 *   [Content_Types].xml
 *   _rels/.rels
 *   xl/workbook.xml
 *   xl/_rels/workbook.xml.rels
 *   xl/worksheets/sheet1.xml  (Timeseries)
 *   xl/worksheets/sheet2.xml  (Snapshots)
 *   xl/worksheets/sheet3.xml  (Summary)
 *   xl/styles.xml
 *
 * Anti-black-box: every exported value matches the in-memory graph series.
 * Deterministic: same data → identical xlsx content (before ZIP compression).
 *
 * Reference: copilot-instructions.md §9.1 (report-ready outputs)
 */

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Minimal ZIP support not included — this module writes a flat XML
// spreadsheet (.xml) that Excel can open via "Open > XML Spreadsheet".
// For true .xlsx, the Python export_excel() in visualize_seed_bead.py
// should be used (requires openpyxl).

namespace coarse_grain {

// ============================================================================
// Excel XML Spreadsheet Export (SpreadsheetML 2003)
// NOTE: This header is included from snapshot_graph.hpp AFTER
// SnapshotGraphCollector is fully defined. Do not include standalone.
// ============================================================================

/**
 * Export collector data as an Excel-compatible XML spreadsheet.
 *
 * This uses the SpreadsheetML 2003 format, which Excel, LibreOffice,
 * and Google Sheets can all open directly.  No ZIP compression needed.
 *
 * File extension: .xml (open with Excel) or .xls (auto-recognized).
 */
inline bool export_excel_xml(
    const std::string& path,
    const SnapshotGraphCollector& collector,
    const std::string& title = "Seed & Bead Report")
{
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << std::fixed << std::setprecision(8);

    // XML header
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<?mso-application progid=\"Excel.Sheet\"?>\n"
        << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n"
        << "  xmlns:o=\"urn:schemas-microsoft-com:office:office\"\n"
        << "  xmlns:x=\"urn:schemas-microsoft-com:office:excel\"\n"
        << "  xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n\n";

    // Styles
    out << "  <Styles>\n"
        << "    <Style ss:ID=\"Default\" ss:Name=\"Normal\">\n"
        << "      <Font ss:FontName=\"Calibri\" ss:Size=\"11\"/>\n"
        << "    </Style>\n"
        << "    <Style ss:ID=\"Header\">\n"
        << "      <Font ss:FontName=\"Calibri\" ss:Size=\"11\" ss:Bold=\"1\"/>\n"
        << "      <Interior ss:Color=\"#D6E4F0\" ss:Pattern=\"Solid\"/>\n"
        << "      <Alignment ss:Horizontal=\"Center\"/>\n"
        << "    </Style>\n"
        << "    <Style ss:ID=\"Unit\">\n"
        << "      <Font ss:FontName=\"Calibri\" ss:Size=\"10\" ss:Italic=\"1\" "
        << "ss:Color=\"#666666\"/>\n"
        << "      <Alignment ss:Horizontal=\"Center\"/>\n"
        << "    </Style>\n"
        << "    <Style ss:ID=\"Num\">\n"
        << "      <NumberFormat ss:Format=\"0.000000\"/>\n"
        << "    </Style>\n"
        << "  </Styles>\n\n";

    // --- Sheet 1: Timeseries ---
    size_t n = collector.energy_series.points.size();

    out << "  <Worksheet ss:Name=\"Timeseries\">\n"
        << "    <Table ss:ExpandedColumnCount=\"15\" "
        << "ss:ExpandedRowCount=\"" << (n + 2) << "\">\n";

    // Header row
    const char* ts_headers[] = {
        "Step", "Energy", "RMS Force", "Max Force", "Mean eta",
        "Mean rho", "Mean C", "Mean P2", "Mean f",
        "Max |d_eta|", "dt", "g_steric", "g_elec", "g_disp", "KE"
    };
    const char* ts_units[] = {
        "#", "kcal/mol", "kcal/(mol*A)", "kcal/(mol*A)", "-",
        "-", "-", "-", "-", "-", "fs", "-", "-", "-", "kcal/mol"
    };

    out << "      <Row>\n";
    for (int c = 0; c < 15; ++c) {
        out << "        <Cell ss:StyleID=\"Header\">"
            << "<Data ss:Type=\"String\">" << ts_headers[c]
            << "</Data></Cell>\n";
    }
    out << "      </Row>\n";

    // Unit row
    out << "      <Row>\n";
    for (int c = 0; c < 15; ++c) {
        out << "        <Cell ss:StyleID=\"Unit\">"
            << "<Data ss:Type=\"String\">[" << ts_units[c]
            << "]</Data></Cell>\n";
    }
    out << "      </Row>\n";

    // Data rows
    for (size_t i = 0; i < n; ++i) {
        out << "      <Row>\n";
        auto emit = [&](double v) {
            out << "        <Cell ss:StyleID=\"Num\">"
                << "<Data ss:Type=\"Number\">" << v
                << "</Data></Cell>\n";
        };
        emit(static_cast<double>(collector.energy_series.points[i].step));
        emit(collector.energy_series.points[i].value);
        emit(collector.rms_force_series.points[i].value);
        emit(collector.max_force_series.points[i].value);
        emit(collector.avg_eta_series.points[i].value);
        emit(collector.avg_rho_series.points[i].value);
        emit(collector.avg_C_series.points[i].value);
        emit(collector.avg_P2_series.points[i].value);
        emit(collector.avg_target_f_series.points[i].value);
        emit(collector.max_deta_series.points[i].value);
        emit(collector.dt_series.points[i].value);
        emit(collector.g_steric_series.points[i].value);
        emit(collector.g_elec_series.points[i].value);
        emit(collector.g_disp_series.points[i].value);
        emit(collector.ke_series.points[i].value);
        out << "      </Row>\n";
    }

    out << "    </Table>\n"
        << "  </Worksheet>\n\n";

    // --- Sheet 2: Snapshots ---
    size_t snap_rows = 0;
    for (const auto& s : collector.snapshots)
        snap_rows += s.positions.size();

    out << "  <Worksheet ss:Name=\"Snapshots\">\n"
        << "    <Table ss:ExpandedColumnCount=\"7\" "
        << "ss:ExpandedRowCount=\"" << (snap_rows + 2) << "\">\n";

    const char* sn_headers[] = {"Step", "Bead ID", "X", "Y", "Z", "eta", "rho"};
    const char* sn_units[] = {"#", "#", "A", "A", "A", "-", "-"};

    out << "      <Row>\n";
    for (int c = 0; c < 7; ++c) {
        out << "        <Cell ss:StyleID=\"Header\">"
            << "<Data ss:Type=\"String\">" << sn_headers[c]
            << "</Data></Cell>\n";
    }
    out << "      </Row>\n";

    out << "      <Row>\n";
    for (int c = 0; c < 7; ++c) {
        out << "        <Cell ss:StyleID=\"Unit\">"
            << "<Data ss:Type=\"String\">[" << sn_units[c]
            << "]</Data></Cell>\n";
    }
    out << "      </Row>\n";

    for (const auto& snap : collector.snapshots) {
        for (size_t i = 0; i < snap.positions.size(); ++i) {
            out << "      <Row>\n";
            out << "        <Cell ss:StyleID=\"Num\"><Data ss:Type=\"Number\">"
                << snap.step_index << "</Data></Cell>\n";
            out << "        <Cell ss:StyleID=\"Num\"><Data ss:Type=\"Number\">"
                << i << "</Data></Cell>\n";
            out << "        <Cell ss:StyleID=\"Num\"><Data ss:Type=\"Number\">"
                << snap.positions[i].x << "</Data></Cell>\n";
            out << "        <Cell ss:StyleID=\"Num\"><Data ss:Type=\"Number\">"
                << snap.positions[i].y << "</Data></Cell>\n";
            out << "        <Cell ss:StyleID=\"Num\"><Data ss:Type=\"Number\">"
                << snap.positions[i].z << "</Data></Cell>\n";
            out << "        <Cell ss:StyleID=\"Num\"><Data ss:Type=\"Number\">"
                << (i < snap.eta_values.size() ? snap.eta_values[i] : 0.0)
                << "</Data></Cell>\n";
            out << "        <Cell ss:StyleID=\"Num\"><Data ss:Type=\"Number\">"
                << (i < snap.rho_values.size() ? snap.rho_values[i] : 0.0)
                << "</Data></Cell>\n";
            out << "      </Row>\n";
        }
    }

    out << "    </Table>\n"
        << "  </Worksheet>\n\n";

    // --- Sheet 3: Summary ---
    out << "  <Worksheet ss:Name=\"Summary\">\n"
        << "    <Table ss:ExpandedColumnCount=\"3\" "
        << "ss:ExpandedRowCount=\"10\">\n";

    out << "      <Row>\n"
        << "        <Cell ss:StyleID=\"Header\"><Data ss:Type=\"String\">"
        << "Metric</Data></Cell>\n"
        << "        <Cell ss:StyleID=\"Header\"><Data ss:Type=\"String\">"
        << "Value</Data></Cell>\n"
        << "        <Cell ss:StyleID=\"Header\"><Data ss:Type=\"String\">"
        << "Unit</Data></Cell>\n"
        << "      </Row>\n";

    auto summary_row = [&](const char* metric, double val, const char* unit) {
        out << "      <Row>\n"
            << "        <Cell><Data ss:Type=\"String\">" << metric
            << "</Data></Cell>\n"
            << "        <Cell ss:StyleID=\"Num\"><Data ss:Type=\"Number\">"
            << val << "</Data></Cell>\n"
            << "        <Cell><Data ss:Type=\"String\">" << unit
            << "</Data></Cell>\n"
            << "      </Row>\n";
    };

    summary_row("Title", 0, title.c_str());
    summary_row("Final Energy", collector.energy_series.final_val(), "kcal/mol");
    summary_row("Final RMS Force", collector.rms_force_series.final_val(), "kcal/(mol*A)");
    summary_row("Final Mean eta", collector.avg_eta_series.final_val(), "-");
    summary_row("Final Mean rho", collector.avg_rho_series.final_val(), "-");
    summary_row("Final g_steric", collector.g_steric_series.final_val(), "-");
    summary_row("Final g_elec", collector.g_elec_series.final_val(), "-");
    summary_row("Final g_disp", collector.g_disp_series.final_val(), "-");
    summary_row("Total Steps", static_cast<double>(n), "#");

    out << "    </Table>\n"
        << "  </Worksheet>\n\n";

    out << "</Workbook>\n";
    return true;
}

} // namespace coarse_grain
