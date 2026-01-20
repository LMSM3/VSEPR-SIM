#include "io_api.h"
#include "xyz_format.h"
#include <string>
#include <memory>

static std::string last_error;

extern "C" {

int io_read_xyz(const char* filename, void** molecule_out) {
    if (!filename || !molecule_out) {
        last_error = "Invalid arguments";
        return -1;
    }
    
    try {
        auto* mol = new vsepr::io::XYZMolecule();
        if (!vsepr::io::read_xyz(filename, *mol)) {
            delete mol;
            last_error = "Failed to read XYZ file";
            return -2;
        }
        *molecule_out = mol;
        return 0;
    } catch (const std::exception& e) {
        last_error = std::string("Exception: ") + e.what();
        return -3;
    }
}

int io_write_xyz(const char* filename, const void* molecule) {
    if (!filename || !molecule) {
        last_error = "Invalid arguments";
        return -1;
    }
    
    try {
        const auto* mol = static_cast<const vsepr::io::XYZMolecule*>(molecule);
        if (!vsepr::io::write_xyz(filename, *mol)) {
            last_error = "Failed to write XYZ file";
            return -2;
        }
        return 0;
    } catch (const std::exception& e) {
        last_error = std::string("Exception: ") + e.what();
        return -3;
    }
}

int io_free_molecule(void* molecule) {
    if (!molecule) {
        return -1;
    }
    
    try {
        delete static_cast<vsepr::io::XYZMolecule*>(molecule);
        return 0;
    } catch (...) {
        last_error = "Failed to free molecule";
        return -2;
    }
}

const char* io_get_last_error() {
    return last_error.c_str();
}

} // extern "C"
