/**
 * VSEPR-Sim GUI Context Menu System
 * Right-click information and actions
 * Version: 2.3.1
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>

namespace vsepr {
namespace gui {

// Context menu item types
enum class MenuItemType {
    ACTION,      // Clickable action
    SUBMENU,     // Opens submenu
    SEPARATOR,   // Visual separator
    INFO,        // Read-only information display
    TOGGLE       // Checkbox toggle
};

// Context menu item
struct MenuItem {
    std::string label;
    MenuItemType type;
    std::function<void()> action;
    std::vector<MenuItem> submenu;
    bool enabled = true;
    bool checked = false;  // For TOGGLE type
    std::string shortcut;
    std::string tooltip;
    
    // Factory methods
    static MenuItem Action(const std::string& label, 
                          std::function<void()> action,
                          const std::string& shortcut = "");
    
    static MenuItem Info(const std::string& label, const std::string& value);
    static MenuItem Separator();
    static MenuItem Toggle(const std::string& label, 
                          bool checked,
                          std::function<void(bool)> onChange);
};

// Context menu builder
class ContextMenu {
public:
    ContextMenu() = default;
    
    // Add menu items
    ContextMenu& addAction(const std::string& label, 
                          std::function<void()> action,
                          const std::string& shortcut = "");
    
    ContextMenu& addInfo(const std::string& label, const std::string& value);
    ContextMenu& addSeparator();
    ContextMenu& addToggle(const std::string& label, 
                          bool checked,
                          std::function<void(bool)> onChange);
    
    ContextMenu& addSubmenu(const std::string& label, 
                           const std::vector<MenuItem>& items);
    
    // Get menu items
    const std::vector<MenuItem>& items() const { return items_; }
    
    // Clear all items
    void clear() { items_.clear(); }
    
private:
    std::vector<MenuItem> items_;
};

// Context-specific menu builders
class MoleculeContextMenu {
public:
    static ContextMenu build(const std::string& molecule_id,
                            const std::string& formula,
                            double energy,
                            int atom_count,
                            int bond_count);
};

class AtomContextMenu {
public:
    static ContextMenu build(int atom_index,
                            const std::string& element,
                            double x, double y, double z,
                            double charge);
};

class BondContextMenu {
public:
    static ContextMenu build(int bond_index,
                            int atom1, int atom2,
                            double order,
                            double length);
};

class PlotContextMenu {
public:
    static ContextMenu build(const std::string& plot_type,
                            bool show_grid,
                            bool show_legend,
                            const std::string& export_path);
};

// Context menu manager
class ContextMenuManager {
public:
    static ContextMenuManager& instance();
    
    // Show context menu at position
    void show(const ContextMenu& menu, int x, int y);
    
    // Register context menu provider for object type
    using MenuProvider = std::function<ContextMenu(void*)>;
    void registerProvider(const std::string& object_type, MenuProvider provider);
    
    // Get menu for object
    ContextMenu getMenuFor(const std::string& object_type, void* object);
    
private:
    ContextMenuManager() = default;
    std::map<std::string, MenuProvider> providers_;
};

} // namespace gui
} // namespace vsepr
