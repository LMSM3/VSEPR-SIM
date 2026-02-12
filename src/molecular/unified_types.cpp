/**
 * VSEPR-Sim Unified Molecular Data Types - Implementation
 */

#include "molecular/unified_types.hpp"
#include "sim/molecule.hpp"

namespace vsepr {
namespace molecular {

MolecularMetadata to_metadata(const Molecule& mol, 
                              const std::string& id,
                              const std::string& formula) {
    MolecularMetadata meta;
    meta.id = id;
    meta.formula = formula;
    meta.atom_count = static_cast<int>(mol.num_atoms());
    meta.bond_count = static_cast<int>(mol.num_bonds());
    // Note: energy would be computed by energy model, not stored in Molecule
    return meta;
}


} // namespace molecular
} // namespace vsepr
