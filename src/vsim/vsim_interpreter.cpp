/**
 * src/vsim/vsim_interpreter.cpp
 * ================================
 * VsimInterpreter eval() and exec() implementation.
 *
 * Expression grammar supported:
 *
 *   expr      ::= call | field_access | atom
 *   call      ::= name "(" arg_list ")"
 *                 | name "." name "(" arg_list ")"
 *   field_access ::= expr "." ("x" | "y" | "z")
 *   arg_list  ::= expr ("," expr)*   |  (empty)
 *   atom      ::= integer | float | quoted_string | variable_name
 *
 * exec() processes lines of the form:
 *   name = expr
 *   (blank and comment lines ignored)
 *
 * WO-VSEPR-SIM-57D  |  beta-8
 */

#include "vsim/vsim_interpreter.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace vsim {

// ── String utilities ──────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
	auto a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return {};
	auto b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

// ── Field access helper ───────────────────────────────────────────────────────

// Resolve value.x / value.y / value.z for XYZVec3 and Int3
static std::optional<Value> try_field_access(const Value& val, const std::string& field) {
	if (value_is_xyz(val)) {
		const XYZVec3& v = std::get<XYZVec3>(val);
		if (field == "x") return Value(v.x);
		if (field == "y") return Value(v.y);
		if (field == "z") return Value(v.z);
		throw VsimRuntimeError("XYZVec3 has no field '" + field + "'. Valid: x, y, z.");
	}
	if (value_is_int3(val)) {
		const Int3& i = std::get<Int3>(val);
		if (field == "x") return Value(static_cast<int64_t>(i.x));
		if (field == "y") return Value(static_cast<int64_t>(i.y));
		if (field == "z") return Value(static_cast<int64_t>(i.z));
		throw VsimRuntimeError("Int3 has no field '" + field + "'. Valid: x, y, z.");
	}
	return std::nullopt;  // not a field-access-able type
}

// ── Argument list splitter ────────────────────────────────────────────────────
// Splits a raw args string on top-level commas (skipping nested parens).

std::vector<Value> VsimInterpreter::parse_args(const std::string& args_str) {
	std::vector<Value> result;
	std::string s = trim(args_str);
	if (s.empty()) return result;

	int depth = 0;
	std::string cur;
	for (char c : s) {
		if (c == '(' || c == '[') { ++depth; cur += c; }
		else if (c == ')' || c == ']') { --depth; cur += c; }
		else if (c == ',' && depth == 0) {
			result.push_back(eval(trim(cur)));
			cur.clear();
		} else {
			cur += c;
		}
	}
	if (!cur.empty()) result.push_back(eval(trim(cur)));
	return result;
}

// ── eval_atom ─────────────────────────────────────────────────────────────────
// Parses a single non-call token: number, quoted string, or variable lookup.

Value VsimInterpreter::eval_atom(const std::string& token) {
	if (token.empty())
		throw VsimRuntimeError("Empty expression token.");

	// Quoted string
	if (token.size() >= 2 && token.front() == '"' && token.back() == '"')
		return token.substr(1, token.size() - 2);

	// Boolean literals
	if (token == "true")  return true;
	if (token == "false") return false;

	// Numeric — try integer first, then float
	{
		bool has_dot = (token.find('.') != std::string::npos);
		bool has_e   = (token.find('e') != std::string::npos ||
						token.find('E') != std::string::npos);
		if (has_dot || has_e) {
			try { return std::stod(token); } catch (...) {}
		} else {
			try { return static_cast<int64_t>(std::stoll(token)); } catch (...) {}
		}
	}

	// Variable lookup
	auto it = scope.find(token);
	if (it != scope.end()) return it->second;

	throw VsimRuntimeError("Unknown variable or literal: '" + token + "'");
}

// ── eval_call ─────────────────────────────────────────────────────────────────
// Dispatches a builtin call.  fn_name is the full dotted name ("pbc.wrap").

Value VsimInterpreter::eval_call(const std::string& fn_name,
								 const std::string& args_str)
{
	auto it = builtins_.find(fn_name);
	if (it == builtins_.end()) {
		// Distinguish namespace errors from unknown-function errors
		auto dot = fn_name.find('.');
		if (dot != std::string::npos) {
			std::string ns = fn_name.substr(0, dot);
			// Check if ANY builtin starts with this namespace
			bool ns_known = false;
			for (const auto& [k, _] : builtins_)
				if (k.substr(0, dot) == ns) { ns_known = true; break; }
			if (!ns_known)
				throw VsimRuntimeError(
					"[VSIM ERROR] Unknown namespace '" + ns + "' in call: " + fn_name + "(...)");
		}
		throw VsimRuntimeError(
			"[VSIM ERROR] Unknown builtin function: " + fn_name + "(...)");
	}

	if (runtime_ == nullptr)
		throw VsimRuntimeError(
			"[VSIM ERROR] Interpreter has no runtime context. "
			"Call set_runtime() before executing scripts.");

	std::vector<Value> args = parse_args(args_str);
	return it->second(args, *runtime_);
}

// ── eval ─────────────────────────────────────────────────────────────────────

Value VsimInterpreter::eval(const std::string& expr) {
	std::string s = trim(expr);
	if (s.empty())
		throw VsimRuntimeError("Cannot evaluate empty expression.");

	// ── Field access: expr.field (postfix, evaluate left then resolve field)
	// Only applies if the last token is a bare identifier following a '.'
	// and the dot is not part of a function call.
	// Strategy: find the last '.' that is NOT inside parens and not part
	// of a number, then test if what follows is 'x', 'y', or 'z'.
	{
		int depth = 0;
		int last_dot_pos = -1;
		for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
			char c = s[i];
			if (c == ')') ++depth;
			else if (c == '(') --depth;
			if (c == '.' && depth == 0 && i > 0) {
				// Ensure the character before is not a digit (float literal dot)
				if (!std::isdigit((unsigned char)s[i-1])) {
					last_dot_pos = i;
					break;
				}
			}
		}
		if (last_dot_pos > 0) {
			std::string sub   = trim(s.substr(0, last_dot_pos));
			std::string field = trim(s.substr(last_dot_pos + 1));
			// If field is a plain identifier (not containing '('), attempt field access
			if (!field.empty() && field.find('(') == std::string::npos) {
				// Could be "pbc.wrap(...)": sub = "pbc", field = "wrap(...)"
				// Only do field access if the field has no '(' anywhere
				Value base = eval(sub);
				auto result = try_field_access(base, field);
				if (result) return *result;
				// Not a known field — fall through to call resolution below
			}
		}
	}

	// ── Function call: name(...) or namespace.name(...)
	// Find the outermost '(' that closes at the very end of the string
	auto paren_start = s.find('(');
	if (paren_start != std::string::npos && s.back() == ')') {
		std::string fn_name  = trim(s.substr(0, paren_start));
		std::string args_str = s.substr(paren_start + 1, s.size() - paren_start - 2);
		return eval_call(fn_name, args_str);
	}

	// ── Atom: literal or variable
	return eval_atom(s);
}

// ── exec ──────────────────────────────────────────────────────────────────────

void VsimInterpreter::exec(const std::string& script_block) {
	std::istringstream ss(script_block);
	std::string line;
	while (std::getline(ss, line)) {
		std::string s = trim(line);
		if (s.empty() || s[0] == '#') continue;

		// name = expr
		auto eq = s.find('=');
		if (eq == std::string::npos) continue;  // skip non-assignment lines

		// Avoid misidentifying == as assignment
		if (eq > 0 && (s[eq-1] == '!' || s[eq-1] == '<' || s[eq-1] == '>' || s[eq-1] == '='))
			continue;
		if (eq + 1 < s.size() && s[eq+1] == '=')
			continue;

		std::string var  = trim(s.substr(0, eq));
		std::string expr = trim(s.substr(eq + 1));
		scope[var] = eval(expr);
	}
}

} // namespace vsim
