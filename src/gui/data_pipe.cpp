/**
 * VSEPR-Sim GUI Data Piping System - Implementation
 * Pipe network singleton implementation
 */

#include "gui/data_pipe.hpp"

namespace vsepr {
namespace gui {

// PipeNetwork singleton implementation
PipeNetwork& PipeNetwork::instance() {
    static PipeNetwork instance;
    return instance;
}

} // namespace gui
} // namespace vsepr
