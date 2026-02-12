#pragma once
/**
 * display.hpp
 * -----------
 * Professional CLI display utilities with color coding and formatting.
 */

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace vsepr {
namespace cli {

// ANSI Color Codes
namespace Color {
    const std::string MAGENTA = "\033[0;35m";
    const std::string CYAN    = "\033[0;36m";
    const std::string GREEN   = "\033[0;32m";
    const std::string YELLOW  = "\033[1;33m";
    const std::string RED     = "\033[0;31m";
    const std::string WHITE   = "\033[0;37m";
    const std::string RESET   = "\033[0m";
    const std::string BOLD    = "\033[1m";
}

// Display Functions
class Display {
public:
    // Headers and Banners
    static void Header(const std::string& text) {
        std::cout << Color::MAGENTA
                  << "╔════════════════════════════════════════════════════════════════╗\n"
                  << "║  " << std::setw(60) << std::left << text << "  ║\n"
                  << "╚════════════════════════════════════════════════════════════════╝\n"
                  << Color::RESET;
    }
    
    static void Subheader(const std::string& text) {
        std::cout << Color::CYAN << Color::BOLD
                  << "┌─ " << text << "\n"
                  << Color::RESET;
    }
    
    static void Banner(const std::string& title, const std::string& subtitle = "") {
        std::cout << Color::MAGENTA
                  << "╔════════════════════════════════════════════════════════════════╗\n"
                  << "║                                                                ║\n"
                  << "║  " << std::setw(60) << std::left << title << "  ║\n";
        if (!subtitle.empty()) {
            std::cout << "║  " << std::setw(60) << std::left << subtitle << "  ║\n";
        }
        std::cout << "║                                                                ║\n"
                  << "╚════════════════════════════════════════════════════════════════╝\n"
                  << Color::RESET;
    }
    
    // Status Messages
    static void Success(const std::string& message) {
        std::cout << Color::GREEN << "✓ " << Color::RESET << message << "\n";
    }
    
    static void Error(const std::string& message) {
        std::cerr << Color::RED << "✗ " << Color::RESET << message << "\n";
    }
    
    static void Warning(const std::string& message) {
        std::cout << Color::YELLOW << "⚠ " << Color::RESET << message << "\n";
    }
    
    static void Info(const std::string& message) {
        std::cout << Color::CYAN << "ℹ " << Color::RESET << message << "\n";
    }
    
    static void Step(const std::string& message) {
        std::cout << Color::WHITE << "▶ " << Color::RESET << message << "\n";
    }
    
    // Progress and Status
    static void Progress(const std::string& label, int current, int total) {
        std::cout << Color::CYAN << label << ": " << Color::RESET
                  << current << "/" << total;
        if (total > 0) {
            int percent = (current * 100) / total;
            std::cout << " (" << percent << "%)";
        }
        std::cout << "\r" << std::flush;
    }
    
    static void ProgressDone() {
        std::cout << "\n";
    }
    
    // Key-Value Display
    static void KeyValue(const std::string& key, const std::string& value, 
                         int keyWidth = 20) {
        std::cout << Color::CYAN << "  " << std::setw(keyWidth) << std::left << key 
                  << Color::RESET << value << "\n";
    }
    
    static void KeyValue(const std::string& key, const std::string& value,
                         const std::string& unit, int keyWidth = 20) {
        std::string val = value;
        if (!unit.empty()) val += " " + unit;
        KeyValue(key, val, keyWidth);
    }
    
    static void KeyValue(const std::string& key, double value, 
                         const std::string& unit = "", int keyWidth = 20) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << value;
        if (!unit.empty()) oss << " " << unit;
        KeyValue(key, oss.str(), keyWidth);
    }
    
    // Separators
    static void Separator() {
        std::cout << Color::CYAN << "────────────────────────────────────────────────────────────────\n"
                  << Color::RESET;
    }
    
    static void BlankLine() {
        std::cout << "\n";
    }
    
    // Lists
    static void ListItem(const std::string& item, bool active = false) {
        if (active) {
            std::cout << Color::YELLOW << "  ▶ " << Color::BOLD << item << Color::RESET << "\n";
        } else {
            std::cout << "    " << item << "\n";
        }
    }
    
    // Tables
    class Table {
        std::vector<std::string> headers_;
        std::vector<int> widths_;
        
    public:
        Table(const std::vector<std::string>& cols, const std::vector<int>& colWidths)
            : headers_(cols), widths_(colWidths) {}
        
        void PrintHeader() {
            std::cout << Color::CYAN;
            for (size_t i = 0; i < headers_.size(); ++i) {
                std::cout << "  " << std::setw(widths_[i]) << std::left << headers_[i];
            }
            std::cout << Color::RESET << "\n";
            std::cout << Color::CYAN;
            for (int w : widths_) {
                for (int j = 0; j < w; ++j) std::cout << "-";
                std::cout << "  ";
            }
            std::cout << Color::RESET << "\n";
        }
        
        void PrintRow(const std::vector<std::string>& values) {
            for (size_t i = 0; i < values.size() && i < widths_.size(); ++i) {
                std::cout << "  " << std::setw(widths_[i]) << std::left << values[i];
            }
            std::cout << "\n";
        }
    };
};

}} // namespace vsepr::cli
