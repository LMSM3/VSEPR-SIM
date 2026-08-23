#pragma once
/**
 * ctl_parser.hpp  —  VSIM-CTL script parser
 * ==========================================
 * WO-VSIM-CTL-02  |  v5.1.x
 *
 * Converts raw CTL script text into an ExecGraph.
 *
 * Supported surface:
 *
 *   namespace.command(positional_arg)
 *   namespace.command(key = value, key2 = value2)
 *   namespace.command(key = "string value")
 *   control_keyword(...)       — const / let / for / if / match etc.
 *   # comment lines
 *   blank lines
 *
 * Variables (const / let) are stored in a symbol table and expanded
 * into arg strings at parse time (${var} substitution).
 *
 * The parser does NOT perform semantic validation — that is the
 * responsibility of CtlValidator.
 *
 * ParseResult::ok == false means the script is syntactically broken.
 */

#include "ctl_types.hpp"
#include <sstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

namespace vsim::ctl {

// ============================================================================
// ParseResult
// ============================================================================

struct ParseResult {
	ExecGraph   graph;
	bool        ok    = true;
	std::string error;
	int         error_line = 0;
};

// ============================================================================
// CtlParser
// ============================================================================

class CtlParser {
public:
	static ParseResult parse(const std::string& script_text) {
		CtlParser p;
		p.result_.graph.script_text = script_text;
		p.parse_lines(script_text);
		return p.result_;
	}

private:
	ParseResult result_;
	std::unordered_map<std::string, std::string> symbols_;

	// -----------------------------------------------------------------------
	static std::string trim(const std::string& s) {
		std::size_t a = s.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) return {};
		std::size_t b = s.find_last_not_of(" \t\r\n");
		return s.substr(a, b - a + 1);
	}

	// Expand ${var} references
	std::string expand(const std::string& s) const {
		std::string out;
		out.reserve(s.size());
		for (std::size_t i = 0; i < s.size(); ) {
			if (s[i] == '$' && i + 1 < s.size() && s[i+1] == '{') {
				std::size_t end = s.find('}', i + 2);
				if (end != std::string::npos) {
					std::string key = s.substr(i + 2, end - i - 2);
					auto it = symbols_.find(key);
					out += (it != symbols_.end()) ? it->second : ("${" + key + "}");
					i = end + 1;
					continue;
				}
			}
			out += s[i++];
		}
		return out;
	}

	// -----------------------------------------------------------------------
	// Parse a quoted string literal — returns content (without quotes)
	// -----------------------------------------------------------------------
	static std::string parse_quoted(const std::string& s, std::size_t& pos) {
		// pos is on the opening '"'
		std::string out;
		++pos; // skip '"'
		while (pos < s.size() && s[pos] != '"') {
			if (s[pos] == '\\' && pos + 1 < s.size()) {
				++pos;
				switch (s[pos]) {
					case 'n': out += '\n'; break;
					case 't': out += '\t'; break;
					default:  out += s[pos]; break;
				}
			} else {
				out += s[pos];
			}
			++pos;
		}
		if (pos < s.size()) ++pos; // skip closing '"'
		return out;
	}

	// -----------------------------------------------------------------------
	// Parse argument list inside parentheses
	// Returns false on syntax error.
	// -----------------------------------------------------------------------
	bool parse_args(const std::string& arg_body, CtlArgMap& out, int /*line_no*/) {
		// arg_body is the content between '(' and ')'
		// Forms:
		//   positional_str
		//   "quoted string"
		//   key = value          (standalone '=', not part of ==, !=, <=, >=)
		//   key = "quoted"
		//   key = 1.0
		//   key = true/false
		std::string body = trim(arg_body);
		if (body.empty()) return true;

		// Split by ',' respecting quotes
		std::vector<std::string> parts;
		{
			std::string cur;
			bool in_q = false;
			for (char c : body) {
				if (c == '"') in_q = !in_q;
				if (!in_q && c == ',') {
					parts.push_back(trim(cur));
					cur.clear();
				} else {
					cur += c;
				}
			}
			if (!trim(cur).empty()) parts.push_back(trim(cur));
		}

		int positional_idx = 0;
		for (auto& part : parts) {
			// Check for key = value where '=' is a standalone assignment
			// (not part of ==, !=, <=, >=)
			auto eq = find_assign_eq(part);
			std::string key, val_str;
			if (eq != std::string::npos) {
				key     = trim(part.substr(0, eq));
				val_str = trim(part.substr(eq + 1));
			} else {
				// positional
				key = "__pos" + std::to_string(positional_idx++);
				val_str = trim(part);
			}

			CtlArg arg;
			arg.key = key;

			// Determine value type
			if (!val_str.empty() && val_str.front() == '"') {
				// quoted string
				std::size_t p = 0;
				std::string q = parse_quoted(val_str, p);
				arg.value = expand(q);
			} else if (val_str == "true") {
				arg.value = true;
			} else if (val_str == "false") {
				arg.value = false;
			} else {
				// Try integer then double then string
				bool is_num = !val_str.empty();
				bool has_dot = false;
				std::size_t start = 0;
				if (val_str[0] == '-' || val_str[0] == '+') start = 1;
				for (std::size_t i = start; i < val_str.size() && is_num; ++i) {
					if (val_str[i] == '.') { has_dot = true; continue; }
					if (val_str[i] == 'e' || val_str[i] == 'E') {
						// scientific notation: allow optional sign after 'e'
						if (i + 1 < val_str.size() &&
							(val_str[i+1] == '+' || val_str[i+1] == '-')) { ++i; }
						has_dot = true; // treat as double
						continue;
					}
					if (!std::isdigit(static_cast<unsigned char>(val_str[i]))) is_num = false;
				}
				if (is_num && !val_str.empty()) {
					if (has_dot)
						arg.value = std::stod(val_str);
					else
						arg.value = static_cast<int64_t>(std::stoll(val_str));
				} else {
					// bare word / variable ref — expand and treat as string
					arg.value = expand(val_str);
				}
			}
			out[key] = arg;
		}
		return true;
	}

	// Find the position of a standalone '=' that is an assignment operator.
	// Returns npos if no such '=' exists (i.e., all '=' are part of ==, !=, <=, >=).
	static std::size_t find_assign_eq(const std::string& s) {
		bool in_q = false;
		for (std::size_t i = 0; i < s.size(); ++i) {
			if (s[i] == '"') { in_q = !in_q; continue; }
			if (in_q) continue;
			if (s[i] == '=') {
				// Reject if preceded by !, <, >, =
				if (i > 0 && (s[i-1] == '!' || s[i-1] == '<' ||
							   s[i-1] == '>' || s[i-1] == '='))
					continue;
				// Reject if followed by =
				if (i + 1 < s.size() && s[i+1] == '=')
					continue;
				return i;
			}
		}
		return std::string::npos;
	}

	// -----------------------------------------------------------------------
	// Parse one non-blank, non-comment line
	// -----------------------------------------------------------------------
	void parse_line(const std::string& raw, int line_no) {
		std::string line = trim(raw);
		if (line.empty() || line[0] == '#') return;

		// Find opening paren
		auto paren_open  = line.find('(');
		auto paren_close = line.rfind(')');

		std::string head     = paren_open  != std::string::npos ? trim(line.substr(0, paren_open)) : line;
		std::string arg_body = (paren_open != std::string::npos && paren_close != std::string::npos)
							   ? line.substr(paren_open + 1, paren_close - paren_open - 1)
							   : "";

		// Handle const / let: store into symbol table
		// Forms: const name = "value"  |  let name = expr
		if (head == "const" || head == "let" ||
			head.substr(0, 6) == "const " || head.substr(0, 4) == "let ")
		{
			// Determine the keyword length ("const" = 5, "let" = 3)
			std::size_t kw_len = (head[0] == 'c') ? 5 : 3;
			// Extract the keyword (no more than kw_len chars)
			std::string keyword = head.substr(0, kw_len);

			// Parse inline:  const name = value   (no parens version)
			// or             const(name = value)
			std::string body;
			if (!arg_body.empty()) {
				body = trim(arg_body);
			} else {
				// No parens — rest of line after keyword
				body = trim(line.substr(kw_len));
			}

			auto eq = find_assign_eq(body);
			if (eq == std::string::npos) {
				result_.ok = false;
				result_.error = "line " + std::to_string(line_no) + ": malformed " + keyword;
				result_.error_line = line_no;
				return;
			}
			std::string var_name = trim(body.substr(0, eq));
			std::string var_val  = trim(body.substr(eq + 1));
			// strip quotes if present
			if (!var_val.empty() && var_val.front() == '"') {
				std::size_t p = 0;
				var_val = parse_quoted(var_val, p);
			}
			symbols_[var_name] = expand(var_val);

			// Store a control command for the exec graph too
			CtlCommand cmd;
			cmd.ns          = CtlNamespace::Control;
			cmd.op          = keyword;
			cmd.sub_op      = keyword;
			cmd.source_line = line_no;
			result_.graph.commands.push_back(std::move(cmd));
			return;
		}

		// Split head into namespace and sub_op
		// e.g. "kernel.channel.enable"  →  ns="kernel", sub_op="channel.enable"
		auto dot = head.find('.');
		std::string ns_str  = dot != std::string::npos ? head.substr(0, dot) : head;
		std::string sub_op  = dot != std::string::npos ? head.substr(dot + 1) : head;

		CtlNamespace ns = parse_namespace(ns_str);

		CtlCommand cmd;
		cmd.ns          = ns;
		cmd.op          = head;
		cmd.sub_op      = sub_op;
		cmd.source_line = line_no;

		if (!parse_args(arg_body, cmd.args, line_no)) {
			// error already set
			return;
		}

		// Expand positional args: promote __pos0 to a canonical "name" key
		// if there's exactly one positional and no named args
		if (cmd.args.size() == 1 && cmd.args.count("__pos0")) {
			CtlArg a = cmd.args.at("__pos0");
			a.key = "name";
			cmd.args.clear();
			cmd.args["name"] = a;
		}

		result_.graph.commands.push_back(std::move(cmd));
	}

	// -----------------------------------------------------------------------
	void parse_lines(const std::string& text) {
		std::istringstream ss(text);
		std::string line;
		int n = 0;
		while (std::getline(ss, line)) {
			++n;
			if (!result_.ok) break;
			parse_line(line, n);
		}
	}
};

} // namespace vsim::ctl
