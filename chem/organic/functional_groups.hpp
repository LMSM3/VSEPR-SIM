// functional_groups.hpp — Functional group detection and classification
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vsepr::chem {

enum class FunctionalGroup : std::uint8_t {
    none = 0,
    hydroxyl,       // -OH
    carboxyl,       // -COOH
    amino,          // -NH2
    amide,          // -CONH-
    carbonyl,       // C=O (ketone/aldehyde)
    ester,          // -COO-
    ether,          // C-O-C
    thiol,          // -SH
    disulfide,      // S-S
    phosphate,      // -PO4
    aromatic_ring,  // benzene / indole / imidazole
    guanidinium,    // Arg sidechain
    imidazole,      // His sidechain
    indole,         // Trp sidechain
    phenol,         // Tyr sidechain
};

inline constexpr std::string_view functional_group_name(FunctionalGroup fg) noexcept {
    switch (fg) {
        case FunctionalGroup::none:           return "none";
        case FunctionalGroup::hydroxyl:       return "hydroxyl (-OH)";
        case FunctionalGroup::carboxyl:       return "carboxyl (-COOH)";
        case FunctionalGroup::amino:          return "amino (-NH2)";
        case FunctionalGroup::amide:          return "amide (-CONH-)";
        case FunctionalGroup::carbonyl:       return "carbonyl (C=O)";
        case FunctionalGroup::ester:          return "ester (-COO-)";
        case FunctionalGroup::ether:          return "ether (C-O-C)";
        case FunctionalGroup::thiol:          return "thiol (-SH)";
        case FunctionalGroup::disulfide:      return "disulfide (S-S)";
        case FunctionalGroup::phosphate:      return "phosphate (-PO4)";
        case FunctionalGroup::aromatic_ring:  return "aromatic ring";
        case FunctionalGroup::guanidinium:    return "guanidinium (Arg)";
        case FunctionalGroup::imidazole:      return "imidazole (His)";
        case FunctionalGroup::indole:         return "indole (Trp)";
        case FunctionalGroup::phenol:         return "phenol (Tyr)";
    }
    return "unknown";
}

struct DetectedGroup {
    FunctionalGroup type {FunctionalGroup::none};
    std::vector<std::int32_t> atom_ids;
    std::string label;
};

} // namespace vsepr::chem
