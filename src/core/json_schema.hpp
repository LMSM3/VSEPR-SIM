#pragma once
/*
json_schema.hpp
---------------
JSON I/O for molecule structure and simulation setup.
Pure interface layer - core solver remains JSON-agnostic.

Schema version 1:
{
  "schema": 1,
  "atoms": [{"Z": 8, "lone_pairs": 2}, ...],
  "coords": [x1,y1,z1, x2,y2,z2, ...],
  "bonds": [{"i":0, "j":1, "order":1}, ...],
  "autogen": {"angles": true, "torsions": true},
  "simulation": {
    "temperature": 300.0,
    "energy_terms": {"bonds":true, "angles":true, "nonbonded":true, "torsions":true},
    "nonbonded": {"epsilon": 0.1, "scale_13": 0.5},
    "optimizer": {"max_iterations": 500, "tol_rms_force": 1e-4}
  },
  "task": {"type": "optimize", "output_file": "result.json"}
}
*/

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <map>

namespace vsepr {

// Minimal JSON writer (no external dependencies)
class JSONWriter {
public:
    JSONWriter() : indent_(0) {}
    
    std::string str() const { return ss_.str(); }
    
    void begin_object() {
        ss_ << "{\n";
        indent_++;
    }
    
    void end_object() {
        indent_--;
        ss_ << "\n" << spaces() << "}";
    }
    
    void begin_array() {
        ss_ << "[\n";
        indent_++;
    }
    
    void end_array() {
        indent_--;
        ss_ << "\n" << spaces() << "]";
    }
    
    void key(const std::string& k) {
        ss_ << spaces() << "\"" << k << "\": ";
    }
    
    void value(int v) { ss_ << v; }
    void value(double v) { ss_ << v; }
    void value(bool v) { ss_ << (v ? "true" : "false"); }
    void value(const std::string& v) { ss_ << "\"" << v << "\""; }
    void null() { ss_ << "null"; }
    
    void comma() { ss_ << ",\n"; }
    void comma_inline() { ss_ << ", "; }
    
private:
    std::stringstream ss_;
    int indent_;
    
    std::string spaces() const {
        return std::string(indent_ * 2, ' ');
    }
};

// Minimal JSON parser (simple key-value extraction)
class JSONParser {
public:
    JSONParser(const std::string& json) : json_(json), pos_(0) {}
    
    bool has_key(const std::string& key) const {
        return json_.find("\"" + key + "\"") != std::string::npos;
    }
    
    int get_int(const std::string& key) {
        std::string val = find_value(key);
        return std::stoi(val);
    }
    
    double get_double(const std::string& key) {
        std::string val = find_value(key);
        return std::stod(val);
    }
    
    bool get_bool(const std::string& key) {
        std::string val = find_value(key);
        return val == "true";
    }
    
    std::string get_string(const std::string& key) {
        std::string val = find_value(key);
        // Remove quotes
        if (val.size() >= 2 && val[0] == '"' && val.back() == '"') {
            return val.substr(1, val.size() - 2);
        }
        return val;
    }
    
    std::vector<double> get_double_array(const std::string& key) {
        std::vector<double> result;
        size_t start = json_.find("\"" + key + "\"");
        if (start == std::string::npos) return result;
        
        size_t arr_start = json_.find("[", start);
        size_t arr_end = json_.find("]", arr_start);
        if (arr_start == std::string::npos || arr_end == std::string::npos) return result;
        
        std::string arr_content = json_.substr(arr_start + 1, arr_end - arr_start - 1);
        std::stringstream ss(arr_content);
        std::string token;
        while (std::getline(ss, token, ',')) {
            result.push_back(std::stod(trim(token)));
        }
        return result;
    }
    
private:
    std::string json_;
    size_t pos_;
    
    std::string find_value(const std::string& key) {
        size_t key_pos = json_.find("\"" + key + "\"");
        if (key_pos == std::string::npos) {
            throw std::runtime_error("Key not found: " + key);
        }
        
        size_t colon = json_.find(":", key_pos);
        size_t value_start = colon + 1;
        
        // Skip whitespace
        while (value_start < json_.size() && std::isspace(json_[value_start])) {
            value_start++;
        }
        
        // Find end of value
        size_t value_end = value_start;
        if (json_[value_start] == '"') {
            // String value
            value_end = json_.find("\"", value_start + 1);
            return json_.substr(value_start, value_end - value_start + 1);
        } else if (json_[value_start] == '[' || json_[value_start] == '{') {
            // Array/object - skip for now
            return "";
        } else {
            // Number/bool/null
            while (value_end < json_.size() && 
                   json_[value_end] != ',' && 
                   json_[value_end] != '}' && 
                   json_[value_end] != ']' &&
                   json_[value_end] != '\n') {
                value_end++;
            }
            return trim(json_.substr(value_start, value_end - value_start));
        }
    }
    
    std::string trim(const std::string& s) {
        size_t start = 0, end = s.size();
        while (start < end && std::isspace(s[start])) start++;
        while (end > start && std::isspace(s[end - 1])) end--;
        return s.substr(start, end - start);
    }
};

// Write molecule to JSON
inline std::string write_molecule_json(const Molecule& mol,
                                       bool autogen_angles = true,
                                       bool autogen_torsions = true) {
    JSONWriter w;
    w.begin_object();
    
    w.key("schema"); w.value(1); w.comma();
    
    // Atoms
    w.key("atoms"); w.begin_array();
    for (size_t i = 0; i < mol.atoms.size(); ++i) {
        if (i > 0) w.comma();
        w.begin_object();
        w.key("Z"); w.value(static_cast<int>(mol.atoms[i].Z));
        if (mol.atoms[i].lone_pairs > 0) {
            w.comma();
            w.key("lone_pairs"); w.value(static_cast<int>(mol.atoms[i].lone_pairs));
        }
        w.end_object();
    }
    w.end_array(); w.comma();
    
    // Coordinates
    w.key("coords"); w.begin_array();
    for (size_t i = 0; i < mol.coords.size(); ++i) {
        if (i > 0) w.comma_inline();
        w.value(mol.coords[i]);
    }
    w.end_array(); w.comma();
    
    // Bonds
    w.key("bonds"); w.begin_array();
    for (size_t i = 0; i < mol.bonds.size(); ++i) {
        if (i > 0) w.comma();
        w.begin_object();
        w.key("i"); w.value(static_cast<int>(mol.bonds[i].i)); w.comma();
        w.key("j"); w.value(static_cast<int>(mol.bonds[i].j)); w.comma();
        w.key("order"); w.value(static_cast<int>(mol.bonds[i].order));
        w.end_object();
    }
    w.end_array(); w.comma();
    
    // Autogen
    w.key("autogen"); w.begin_object();
    w.key("angles"); w.value(autogen_angles); w.comma();
    w.key("torsions"); w.value(autogen_torsions);
    w.end_object();
    
    w.end_object();
    return w.str();
}

// Write full simulation setup to JSON
inline std::string write_simulation_json(const Molecule& mol,
                                        double temperature = 300.0,
                                        const NonbondedParams& nb_params = NonbondedParams(),
                                        const OptimizerSettings& opt_settings = OptimizerSettings(),
                                        const std::string& task_type = "optimize",
                                        const std::string& output_file = "") {
    JSONWriter w;
    w.begin_object();
    
    w.key("schema"); w.value(1); w.comma();
    
    // Atoms
    w.key("atoms"); w.begin_array();
    for (size_t i = 0; i < mol.atoms.size(); ++i) {
        if (i > 0) w.comma();
        w.begin_object();
        w.key("Z"); w.value(static_cast<int>(mol.atoms[i].Z));
        if (mol.atoms[i].lone_pairs > 0) {
            w.comma();
            w.key("lone_pairs"); w.value(static_cast<int>(mol.atoms[i].lone_pairs));
        }
        w.end_object();
    }
    w.end_array(); w.comma();
    
    // Coordinates
    w.key("coords"); w.begin_array();
    for (size_t i = 0; i < mol.coords.size(); ++i) {
        if (i > 0) w.comma_inline();
        w.value(mol.coords[i]);
    }
    w.end_array(); w.comma();
    
    // Bonds
    w.key("bonds"); w.begin_array();
    for (size_t i = 0; i < mol.bonds.size(); ++i) {
        if (i > 0) w.comma();
        w.begin_object();
        w.key("i"); w.value(static_cast<int>(mol.bonds[i].i)); w.comma();
        w.key("j"); w.value(static_cast<int>(mol.bonds[i].j)); w.comma();
        w.key("order"); w.value(static_cast<int>(mol.bonds[i].order));
        w.end_object();
    }
    w.end_array(); w.comma();
    
    // Autogen
    w.key("autogen"); w.begin_object();
    w.key("angles"); w.value(true); w.comma();
    w.key("torsions"); w.value(true);
    w.end_object(); w.comma();
    
    // Simulation
    w.key("simulation"); w.begin_object();
    w.key("temperature"); w.value(temperature); w.comma();
    
    w.key("energy_terms"); w.begin_object();
    w.key("bonds"); w.value(true); w.comma();
    w.key("angles"); w.value(true); w.comma();
    w.key("nonbonded"); w.value(true); w.comma();
    w.key("torsions"); w.value(true);
    w.end_object(); w.comma();
    
    w.key("nonbonded"); w.begin_object();
    w.key("epsilon"); w.value(nb_params.epsilon); w.comma();
    w.key("scale_13"); w.value(nb_params.scale_13);
    w.end_object(); w.comma();
    
    w.key("optimizer"); w.begin_object();
    w.key("max_iterations"); w.value(opt_settings.max_iterations); w.comma();
    w.key("tol_rms_force"); w.value(opt_settings.tol_rms_force);
    w.end_object();
    
    w.end_object(); w.comma();
    
    // Task
    w.key("task"); w.begin_object();
    w.key("type"); w.value(task_type);
    if (!output_file.empty()) {
        w.comma();
        w.key("output_file"); w.value(output_file);
    }
    w.end_object();
    
    w.end_object();
    return w.str();
}

// Read molecule from JSON file
inline Molecule read_molecule_json(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    
    JSONParser parser(json);
    
    // Verify schema version
    if (parser.has_key("schema") && parser.get_int("schema") != 1) {
        throw std::runtime_error("Unsupported schema version");
    }
    
    Molecule mol;
    
    // Parse atoms (manual extraction for now)
    size_t atoms_start = json.find("\"atoms\"");
    size_t arr_start = json.find("[", atoms_start);
    size_t arr_end = json.find("]", arr_start);
    
    std::string atoms_section = json.substr(arr_start + 1, arr_end - arr_start - 1);
    
    // Simple atom parsing
    size_t pos = 0;
    while ((pos = atoms_section.find("{", pos)) != std::string::npos) {
        size_t end = atoms_section.find("}", pos);
        std::string atom_obj = atoms_section.substr(pos, end - pos + 1);
        
        // Extract Z
        size_t z_pos = atom_obj.find("\"Z\"");
        size_t z_val_start = atom_obj.find(":", z_pos) + 1;
        size_t z_val_end = atom_obj.find_first_of(",}", z_val_start);
        int Z = std::stoi(atom_obj.substr(z_val_start, z_val_end - z_val_start));
        
        // Extract lone_pairs if present
        int lone_pairs = 0;
        if (atom_obj.find("\"lone_pairs\"") != std::string::npos) {
            size_t lp_pos = atom_obj.find("\"lone_pairs\"");
            size_t lp_val_start = atom_obj.find(":", lp_pos) + 1;
            size_t lp_val_end = atom_obj.find_first_of(",}", lp_val_start);
            lone_pairs = std::stoi(atom_obj.substr(lp_val_start, lp_val_end - lp_val_start));
        }
        
        mol.atoms.push_back({0, static_cast<uint8_t>(Z), 0.0, static_cast<uint8_t>(lone_pairs), 0});
        pos = end + 1;
    }
    
    // Parse coordinates
    std::vector<double> coords = parser.get_double_array("coords");
    mol.coords = coords;
    
    // Parse bonds
    size_t bonds_start = json.find("\"bonds\"");
    arr_start = json.find("[", bonds_start);
    arr_end = json.find("]", arr_start);
    
    std::string bonds_section = json.substr(arr_start + 1, arr_end - arr_start - 1);
    pos = 0;
    while ((pos = bonds_section.find("{", pos)) != std::string::npos) {
        size_t end = bonds_section.find("}", pos);
        std::string bond_obj = bonds_section.substr(pos, end - pos + 1);
        
        // Extract i, j, order
        auto extract_int = [](const std::string& s, const std::string& key) {
            size_t k_pos = s.find("\"" + key + "\"");
            size_t val_start = s.find(":", k_pos) + 1;
            size_t val_end = s.find_first_of(",}", val_start);
            return std::stoi(s.substr(val_start, val_end - val_start));
        };
        
        int i = extract_int(bond_obj, "i");
        int j = extract_int(bond_obj, "j");
        int order = extract_int(bond_obj, "order");
        
        mol.bonds.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(j), static_cast<uint8_t>(order)});
        pos = end + 1;
    }
    
    return mol;
}

} // namespace vsepr
