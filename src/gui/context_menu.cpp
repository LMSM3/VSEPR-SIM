/**
 * VSEPR-Sim GUI Context Menu Implementation
 * Version: 2.3.1
 */

#include "gui/context_menu.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace vsepr {
namespace gui {

// MenuItem factory methods
MenuItem MenuItem::Action(const std::string& label, 
                         std::function<void()> action,
                         const std::string& shortcut) {
    MenuItem item;
    item.label = label;
    item.type = MenuItemType::ACTION;
    item.action = action;
    item.shortcut = shortcut;
    item.enabled = true;
    return item;
}

MenuItem MenuItem::Info(const std::string& label, const std::string& value) {
    MenuItem item;
    item.label = label + ": " + value;
    item.type = MenuItemType::INFO;
    item.enabled = false;  // Info is read-only
    return item;
}

MenuItem MenuItem::Separator() {
    MenuItem item;
    item.type = MenuItemType::SEPARATOR;
    item.enabled = false;
    return item;
}

MenuItem MenuItem::Toggle(const std::string& label, 
                         bool checked,
                         std::function<void(bool)> onChange) {
    MenuItem item;
    item.label = label;
    item.type = MenuItemType::TOGGLE;
    item.checked = checked;
    item.action = [onChange, checked]() { onChange(!checked); };
    item.enabled = true;
    return item;
}

// ContextMenu implementation
ContextMenu& ContextMenu::addAction(const std::string& label, 
                                   std::function<void()> action,
                                   const std::string& shortcut) {
    items_.push_back(MenuItem::Action(label, action, shortcut));
    return *this;
}

ContextMenu& ContextMenu::addInfo(const std::string& label, const std::string& value) {
    items_.push_back(MenuItem::Info(label, value));
    return *this;
}

ContextMenu& ContextMenu::addSeparator() {
    items_.push_back(MenuItem::Separator());
    return *this;
}

ContextMenu& ContextMenu::addToggle(const std::string& label, 
                                   bool checked,
                                   std::function<void(bool)> onChange) {
    items_.push_back(MenuItem::Toggle(label, checked, onChange));
    return *this;
}

ContextMenu& ContextMenu::addSubmenu(const std::string& label, 
                                    const std::vector<MenuItem>& items) {
    MenuItem submenu_item;
    submenu_item.label = label;
    submenu_item.type = MenuItemType::SUBMENU;
    submenu_item.submenu = items;
    submenu_item.enabled = true;
    items_.push_back(submenu_item);
    return *this;
}

// MoleculeContextMenu implementation
ContextMenu MoleculeContextMenu::build(const std::string& molecule_id,
                                      const std::string& formula,
                                      double energy,
                                      int atom_count,
                                      int bond_count) {
    ContextMenu menu;
    
    // Information section
    menu.addInfo("ID", molecule_id);
    menu.addInfo("Formula", formula);
    
    std::ostringstream energy_str;
    energy_str << std::fixed << std::setprecision(2) << energy << " kcal/mol";
    menu.addInfo("Energy", energy_str.str());
    
    menu.addInfo("Atoms", std::to_string(atom_count));
    menu.addInfo("Bonds", std::to_string(bond_count));
    
    menu.addSeparator();
    
    // Actions
    menu.addAction("View Details", [molecule_id]() {
        std::cout << "Viewing details for: " << molecule_id << std::endl;
    }, "Ctrl+I");
    
    menu.addAction("Optimize Geometry", [molecule_id]() {
        std::cout << "Optimizing: " << molecule_id << std::endl;
    }, "Ctrl+O");
    
    menu.addAction("Export XYZ", [molecule_id]() {
        std::cout << "Exporting: " << molecule_id << ".xyz" << std::endl;
    }, "Ctrl+E");
    
    menu.addSeparator();
    
    // Visualization options
    std::vector<MenuItem> viz_options;
    viz_options.push_back(MenuItem::Toggle("Show Bonds", true, [](bool checked) {
        std::cout << "Show Bonds: " << (checked ? "ON" : "OFF") << std::endl;
    }));
    viz_options.push_back(MenuItem::Toggle("Show Labels", false, [](bool checked) {
        std::cout << "Show Labels: " << (checked ? "ON" : "OFF") << std::endl;
    }));
    viz_options.push_back(MenuItem::Toggle("Show Charges", false, [](bool checked) {
        std::cout << "Show Charges: " << (checked ? "ON" : "OFF") << std::endl;
    }));
    
    menu.addSubmenu("Visualization", viz_options);
    
    menu.addSeparator();
    
    menu.addAction("Copy Formula", [formula]() {
        std::cout << "Copied: " << formula << std::endl;
    });
    
    return menu;
}

// AtomContextMenu implementation
ContextMenu AtomContextMenu::build(int atom_index,
                                  const std::string& element,
                                  double x, double y, double z,
                                  double charge) {
    ContextMenu menu;
    
    std::ostringstream title;
    title << "Atom #" << atom_index << " (" << element << ")";
    menu.addInfo("Atom", title.str());
    
    menu.addSeparator();
    
    std::ostringstream pos;
    pos << std::fixed << std::setprecision(3);
    pos << "(" << x << ", " << y << ", " << z << ")";
    menu.addInfo("Position", pos.str());
    
    std::ostringstream charge_str;
    charge_str << std::fixed << std::setprecision(3) << charge;
    menu.addInfo("Charge", charge_str.str());
    
    menu.addSeparator();
    
    menu.addAction("Select Atom", [atom_index]() {
        std::cout << "Selected atom: " << atom_index << std::endl;
    });
    
    menu.addAction("Hide Atom", [atom_index]() {
        std::cout << "Hidden atom: " << atom_index << std::endl;
    });
    
    menu.addAction("Center View", [atom_index]() {
        std::cout << "Centered on atom: " << atom_index << std::endl;
    });
    
    menu.addSeparator();
    
    menu.addAction("Copy Coordinates", [x, y, z]() {
        std::cout << "Copied: " << x << ", " << y << ", " << z << std::endl;
    });
    
    return menu;
}

// BondContextMenu implementation
ContextMenu BondContextMenu::build(int bond_index,
                                  int atom1, int atom2,
                                  double order,
                                  double length) {
    ContextMenu menu;
    
    std::ostringstream title;
    title << "Bond #" << bond_index << " (" << atom1 << "-" << atom2 << ")";
    menu.addInfo("Bond", title.str());
    
    menu.addSeparator();
    
    std::ostringstream order_str;
    order_str << std::fixed << std::setprecision(1) << order;
    menu.addInfo("Order", order_str.str());
    
    std::ostringstream length_str;
    length_str << std::fixed << std::setprecision(3) << length << " Å";
    menu.addInfo("Length", length_str.str());
    
    menu.addSeparator();
    
    menu.addAction("Select Bond", [bond_index]() {
        std::cout << "Selected bond: " << bond_index << std::endl;
    });
    
    menu.addAction("Break Bond", [bond_index]() {
        std::cout << "Breaking bond: " << bond_index << std::endl;
    });
    
    menu.addAction("Highlight Path", [bond_index]() {
        std::cout << "Highlighting path through bond: " << bond_index << std::endl;
    });
    
    return menu;
}

// PlotContextMenu implementation
ContextMenu PlotContextMenu::build(const std::string& plot_type,
                                  bool show_grid,
                                  bool show_legend,
                                  const std::string& export_path) {
    ContextMenu menu;
    
    menu.addInfo("Plot Type", plot_type);
    
    menu.addSeparator();
    
    menu.addToggle("Show Grid", show_grid, [](bool checked) {
        std::cout << "Grid: " << (checked ? "ON" : "OFF") << std::endl;
    });
    
    menu.addToggle("Show Legend", show_legend, [](bool checked) {
        std::cout << "Legend: " << (checked ? "ON" : "OFF") << std::endl;
    });
    
    menu.addSeparator();
    
    menu.addAction("Reset Zoom", []() {
        std::cout << "Reset zoom" << std::endl;
    }, "R");
    
    menu.addAction("Auto Scale", []() {
        std::cout << "Auto scale" << std::endl;
    }, "A");
    
    menu.addSeparator();
    
    std::vector<MenuItem> export_options;
    export_options.push_back(MenuItem::Action("Export as PNG", [export_path]() {
        std::cout << "Exporting to: " << export_path << ".png" << std::endl;
    }));
    export_options.push_back(MenuItem::Action("Export as SVG", [export_path]() {
        std::cout << "Exporting to: " << export_path << ".svg" << std::endl;
    }));
    export_options.push_back(MenuItem::Action("Export Data (CSV)", [export_path]() {
        std::cout << "Exporting data to: " << export_path << ".csv" << std::endl;
    }));
    
    menu.addSubmenu("Export", export_options);
    
    menu.addSeparator();
    
    menu.addAction("Copy to Clipboard", []() {
        std::cout << "Plot copied to clipboard" << std::endl;
    }, "Ctrl+C");
    
    return menu;
}

// ContextMenuManager implementation
ContextMenuManager& ContextMenuManager::instance() {
    static ContextMenuManager instance;
    return instance;
}

void ContextMenuManager::show(const ContextMenu& menu, int x, int y) {
    std::cout << "\n╔════════ Context Menu at (" << x << ", " << y << ") ════════╗\n";
    
    for (const auto& item : menu.items()) {
        switch (item.type) {
            case MenuItemType::SEPARATOR:
                std::cout << "├────────────────────────────────────────────────┤\n";
                break;
                
            case MenuItemType::INFO:
                std::cout << "│ " << item.label << "\n";
                break;
                
            case MenuItemType::ACTION:
                std::cout << "│ [•] " << item.label;
                if (!item.shortcut.empty()) {
                    std::cout << " (" << item.shortcut << ")";
                }
                std::cout << "\n";
                break;
                
            case MenuItemType::TOGGLE:
                std::cout << "│ [" << (item.checked ? "✓" : " ") << "] " << item.label << "\n";
                break;
                
            case MenuItemType::SUBMENU:
                std::cout << "│ [→] " << item.label << "\n";
                break;
        }
    }
    
    std::cout << "╚════════════════════════════════════════════════╝\n\n";
}

void ContextMenuManager::registerProvider(const std::string& object_type, MenuProvider provider) {
    providers_[object_type] = provider;
}

ContextMenu ContextMenuManager::getMenuFor(const std::string& object_type, void* object) {
    auto it = providers_.find(object_type);
    if (it != providers_.end()) {
        return it->second(object);
    }
    return ContextMenu();
}

} // namespace gui
} // namespace vsepr
