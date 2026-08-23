#pragma once
// ============================================================================
// console_render.hpp  --  shared [console] narration renderer  (WO-84 Preflight)
// ============================================================================
// print_console already exists as a parser primitive (WO-85A).  Day 84 promotes
// it to the standard narration primitive for VSIM scripts, so its rendering must
// be reusable across the broader CLI surface (classify, validate, run, ...)
// instead of being copy-pasted per command.
//
// This is deliberately ONE tiny helper pair, not an output subsystem:
//   * render_console_block(...)            -- styled "[console]" block emitter
//   * collect_console_prints_lightweight() -- file scan without a full parse
//
// The parser itself is NOT reimplemented here; the lightweight collector mirrors
// the parser's acceptance rule so commands that never build a full VsimDocument
// (or want a cheap pre-scan) can still surface narration.
// ============================================================================

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace vsim {

struct ConsolePrint;  // full definition in vsim/vsim_document.hpp

// Render a styled "[console]" block from already-extracted messages.
// Layout matches the historical `vsepr classify` surface exactly:
//   "  [console]"
//   "  > <message>"   (one per line)
//   ""                (trailing blank line)
// Emits nothing at all when `messages` is empty.
void render_console_block(const std::vector<std::string>& messages,
						  std::ostream& os,
						  bool color);

// Convenience overload: pull the message text out of parsed ConsolePrint
// directives (VsimDocument::console_prints) and render them.
void render_console_block(const std::vector<ConsolePrint>& prints,
						  std::ostream& os,
						  bool color);

// Lightweight file scanner.  Collects `print_console "<message>"` payloads from
// a .vsim script WITHOUT constructing a full VsimDocument.  Behaviour mirrors
// the parser directive rule:
//   * quote-aware `#` comment stripping
//   * the token must be followed by whitespace, a quote, or end-of-line so
//     key assignments like `print_console_mode = ...` are ignored
//   * a single surrounding double-quote pair is stripped
//   * declaration order preserved
std::vector<std::string>
collect_console_prints_lightweight(const std::filesystem::path& path);

} // namespace vsim
