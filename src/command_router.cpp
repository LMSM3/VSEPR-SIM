#include <string>
#include <map>
#include <functional>
#include <vector>

// TODO: Implement command router for visualization system
// This routes commands from the visualization UI to appropriate handlers

namespace vsepr {
namespace command {

using CommandHandler = std::function<void(const std::vector<std::string>&)>;

class CommandRouter {
public:
    CommandRouter() = default;
    
    void register_command(const std::string& name, CommandHandler handler) {
        handlers_[name] = handler;
    }
    
    bool route(const std::string& command, const std::vector<std::string>& args) {
        auto it = handlers_.find(command);
        if (it != handlers_.end()) {
            it->second(args);
            return true;
        }
        return false;
    }
    
private:
    std::map<std::string, CommandHandler> handlers_;
};

// Global router instance
static CommandRouter global_router;

CommandRouter& get_router() {
    return global_router;
}

} // namespace command
} // namespace vsepr
