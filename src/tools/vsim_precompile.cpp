/**
 * src/tools/vsim_precompile.cpp
 * ==============================
 * vsim-precompile  -  .vsim-pre -> .vsim precompiler front-end
 *
 * Transforms a modular .vsim-pre script into a flat .vsim script
 * suitable for the VSIM runtime.
 *
 * Five-phase pipeline:
 *   1. Parse [module.variables]    -> build initial variable table
 *   2. Run asking loop (if interactive && [module.ask].enabled)
 *   3. Evaluate enabled_if conditions
 *   4. Substitute variables into section values
 *   5. Emit flat .vsim
 *
 * Usage:
 *   vsim-precompile input.vsim-pre [output.vsim]
 *   vsim-precompile --non-interactive input.vsim-pre [output.vsim]
 *   vsim-precompile --dry-run input.vsim-pre      (print result, don't write)
 *
 * Exit codes:
 *   0  success, flat .vsim written
 *   1  parse error in .vsim-pre
 *   2  user aborted asking loop (Ctrl+C caught)
 *   3  output file write error
 *
 * Reference: FinalChapter/VSIM_PRECOMPILER_DESIGN.md
 * WO-75D  |  v5.0.0-main
 */

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Value types
// ============================================================================

enum class VarType { Str, Bool, Num, StrArray };

struct VarValue {
	VarType          type   = VarType::Str;
	std::string      s;
	bool             b      = false;
	double           n      = 0.0;
	std::vector<std::string> arr;

	static VarValue from_str(const std::string& v)  { VarValue r; r.type=VarType::Str;  r.s=v; return r; }
	static VarValue from_bool(bool v)                { VarValue r; r.type=VarType::Bool; r.b=v; return r; }
	static VarValue from_num(double v)               { VarValue r; r.type=VarType::Num;  r.n=v; return r; }
	static VarValue from_arr(std::vector<std::string> v){ VarValue r; r.type=VarType::StrArray; r.arr=std::move(v); return r; }

	std::string to_toml() const {
		switch (type) {
		case VarType::Bool:  return b ? "true" : "false";
		case VarType::Num:   {
			std::ostringstream os; os << n;
			return os.str();
		}
		case VarType::StrArray: {
			std::string out = "[";
			for (size_t i = 0; i < arr.size(); ++i) {
				if (i) out += ", ";
				out += "\"" + arr[i] + "\"";
			}
			return out + "]";
		}
		default: return "\"" + s + "\"";
		}
	}

	std::string to_display() const {
		switch (type) {
		case VarType::Bool:     return b ? "true" : "false";
		case VarType::Num:      { std::ostringstream os; os << n; return os.str(); }
		case VarType::StrArray: {
			std::string out;
			for (size_t i = 0; i < arr.size(); ++i) {
				if (i) out += ",";
				out += arr[i];
			}
			return out;
		}
		default: return s;
		}
	}
};

using VarTable = std::map<std::string, VarValue>;

// ============================================================================
// Tiny .vsim-pre parser  (key = value line reader, no full TOML)
// ============================================================================

static std::string trim(const std::string& s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

// Parse a TOML-style string value from: "value" or value
static std::string parse_toml_string(const std::string& raw)
{
	std::string v = trim(raw);
	if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
		return v.substr(1, v.size() - 2);
	return v;
}

// Parse a TOML array of strings: ["a", "b"] or a,b,c
static std::vector<std::string> parse_toml_arr(const std::string& raw)
{
	std::string v = trim(raw);
	std::vector<std::string> result;
	// Strip outer brackets if present
	if (!v.empty() && v.front() == '[') {
		v = v.substr(1);
		if (!v.empty() && v.back() == ']') v.pop_back();
	}
	std::istringstream ss(v);
	std::string tok;
	while (std::getline(ss, tok, ',')) {
		result.push_back(parse_toml_string(trim(tok)));
	}
	return result;
}

static bool parse_toml_bool(const std::string& raw)
{
	std::string v = trim(raw);
	return v == "true" || v == "1" || v == "yes";
}

// Infer VarValue type from a raw string
static VarValue infer_value(const std::string& raw)
{
	std::string v = trim(raw);
	if (v == "true" || v == "false")
		return VarValue::from_bool(parse_toml_bool(v));
	if (!v.empty() && v.front() == '[')
		return VarValue::from_arr(parse_toml_arr(v));
	// Try numeric
	try {
		size_t pos = 0;
		double d = std::stod(v, &pos);
		if (pos == v.size()) return VarValue::from_num(d);
	} catch (...) {}
	return VarValue::from_str(parse_toml_string(v));
}

// ============================================================================
// Question entry
// ============================================================================

struct QuestionEntry {
	std::string name;
	std::string prompt;
	std::string default_raw;
	std::vector<std::string> choices;
	std::vector<std::string> examples;
	std::string enabled_if;
	std::string type_hint;
};

// ============================================================================
// enabled_if evaluation  ("var op literal")
// ============================================================================

static bool eval_enabled_if(const std::string& expr, const VarTable& vars)
{
	if (expr.empty()) return true;
	// Split on == / != / >= / <= / > / <
	auto try_op = [&](const std::string& op) -> std::pair<bool,bool> {
		size_t pos = expr.find(op);
		if (pos == std::string::npos) return {false, false};
		std::string var_name = trim(expr.substr(0, pos));
		std::string literal  = trim(expr.substr(pos + op.size()));
		auto it = vars.find(var_name);
		if (it == vars.end()) return {true, false};
		const VarValue& v = it->second;
		if (op == "==") return {true, v.to_display() == parse_toml_string(literal)};
		if (op == "!=") return {true, v.to_display() != parse_toml_string(literal)};
		if (op == ">" || op == "<" || op == ">=" || op == "<=") {
			double lhs = (v.type == VarType::Num) ? v.n : 0.0;
			double rhs = 0.0;
			try { rhs = std::stod(literal); } catch (...) {}
			if (op == ">")  return {true, lhs >  rhs};
			if (op == "<")  return {true, lhs <  rhs};
			if (op == ">=") return {true, lhs >= rhs};
			if (op == "<=") return {true, lhs <= rhs};
		}
		return {true, false};
	};

	for (const char* op : {"==", "!=", ">=", "<=", ">", "<"}) {
		auto [found, result] = try_op(op);
		if (found) return result;
	}
	return true;
}

// ============================================================================
// Asking loop
// ============================================================================

static void run_asking_loop(
	std::vector<QuestionEntry>& questions,
	VarTable& vars,
	bool interactive,
	bool loop_until_confirmed)
{
	auto ask_once = [&]() {
		int active = 0;
		for (const auto& q : questions)
			if (eval_enabled_if(q.enabled_if, vars)) ++active;

		int idx = 0;
		for (auto& q : questions) {
			if (!eval_enabled_if(q.enabled_if, vars)) continue;
			++idx;

			// Get current default value display
			std::string def_display;
			auto it = vars.find(q.name);
			if (it != vars.end())
				def_display = it->second.to_display();
			else
				def_display = parse_toml_string(q.default_raw);

			// Print prompt
			std::cout << "\n[" << idx << "/" << active << "] "
					  << q.prompt << " [" << def_display << "]";
			if (!q.choices.empty()) {
				std::cout << " (";
				for (size_t i = 0; i < q.choices.size(); ++i) {
					if (i) std::cout << "/";
					std::cout << q.choices[i];
				}
				std::cout << ")";
			}
			std::cout << "\n";
			if (!q.examples.empty()) {
				std::cout << "      Examples: ";
				for (size_t i = 0; i < q.examples.size(); ++i) {
					if (i) std::cout << ", ";
					std::cout << q.examples[i];
				}
				std::cout << "\n";
			}

			if (!interactive) {
				// Non-interactive: use default
				if (it == vars.end())
					vars[q.name] = infer_value(q.default_raw);
				continue;
			}

			std::cout << "> ";
			std::string input;
			if (!std::getline(std::cin, input)) {
				std::cerr << "\n[ABORT] Input stream closed.\n";
				std::exit(2);
			}
			input = trim(input);

			if (input.empty()) {
				// Use default
				if (it == vars.end())
					vars[q.name] = infer_value(q.default_raw);
			} else {
				// Validate choices if declared
				if (!q.choices.empty()) {
					bool valid = std::find(q.choices.begin(), q.choices.end(), input)
								 != q.choices.end();
					if (!valid) {
						std::cout << "  [invalid choice; using default]\n";
						if (it == vars.end())
							vars[q.name] = infer_value(q.default_raw);
						continue;
					}
				}
				vars[q.name] = infer_value(input);
			}
		}
	};

	if (!interactive) {
		ask_once();
		return;
	}

	while (true) {
		ask_once();

		// Print summary
		std::cout << "\n--------------------------------------------------\nSummary:\n";
		for (const auto& [k, v] : vars)
			std::cout << "  " << k << " = " << v.to_toml() << "\n";

		if (!loop_until_confirmed) break;

		std::cout << "\nConfirm? [true] (true/false)\n> ";
		std::string ans;
		if (!std::getline(std::cin, ans)) {
			std::cerr << "\n[ABORT] Input stream closed.\n";
			std::exit(2);
		}
		ans = trim(ans);
		if (ans.empty() || ans == "true" || ans == "yes") break;
		std::cout << "\n[Re-running questions]\n";
	}
}

// ============================================================================
// Variable substitution in a line
// ============================================================================

static std::string substitute(const std::string& line, const VarTable& vars)
{
	std::string result = line;
	for (const auto& [k, v] : vars) {
		// Replace bare name on RHS of assignment: key = VAR_NAME
		size_t eq = result.find('=');
		if (eq != std::string::npos) {
			std::string rhs = trim(result.substr(eq + 1));
			if (rhs == k) {
				return result.substr(0, eq + 1) + " " + v.to_toml();
			}
		}
	}
	return result;
}

// ============================================================================
// Module section translator
// [module.X] -> [X]  (e.g. [module.material] -> [material])
// ============================================================================

static std::string translate_section_header(const std::string& raw)
{
	// Patterns:
	//   [module.variables]  -> skip entirely (handled separately)
	//   [module.ask]        -> skip
	//   [module.material]   -> [material]
	//   [module.run]        -> [run]
	//   [[module.simulation.molecule]] -> [[simulation.molecule]]
	//   [language]          -> skip (metadata only)
	std::string s = trim(raw);
	if (s == "[language]" || s == "[module.variables]" || s == "[module.ask]")
		return "";  // skip
	if (s.size() > 2 && s.front() == '[') {
		bool double_bracket = (s.size() > 3 && s[1] == '[');
		size_t inner_start = double_bracket ? 2 : 1;
		size_t inner_end   = double_bracket ? s.size() - 2 : s.size() - 1;
		std::string inner  = s.substr(inner_start, inner_end - inner_start);
		// Strip "module." prefix
		const std::string pfx = "module.";
		if (inner.rfind(pfx, 0) == 0)
			inner = inner.substr(pfx.size());
		if (double_bracket) return "[[" + inner + "]]";
		return "[" + inner + "]";
	}
	return s;
}

// ============================================================================
// .vsim-pre file parsing
// ============================================================================

struct PreFile {
	VarTable                 variables;
	bool                     ask_enabled     = false;
	bool                     interactive     = true;
	bool                     loop_confirmed  = true;
	std::vector<QuestionEntry> questions;
	// All lines for the output body (non-variable/ask sections)
	std::vector<std::string> body_lines;
};

enum class ParseSection {
	None, Variables, Ask, Question, Body
};

static PreFile parse_pre_file(const std::string& path)
{
	std::ifstream f(path);
	if (!f) {
		std::cerr << "[ERROR] Cannot open: " << path << "\n";
		std::exit(1);
	}

	PreFile result;
	ParseSection sec   = ParseSection::None;
	std::string  line;
	QuestionEntry cur_q;
	bool in_q = false;

	// Emit current pending question
	auto flush_q = [&]() {
		if (in_q && !cur_q.name.empty()) {
			result.questions.push_back(cur_q);
			cur_q = {};
			in_q = false;
		}
	};

	while (std::getline(f, line)) {
		std::string tl = trim(line);

		// Skip empty lines and comments in body
		if (tl.empty()) {
			if (sec == ParseSection::Body)
				result.body_lines.push_back(line);
			continue;
		}
		if (tl.front() == '#') {
			if (sec == ParseSection::Body)
				result.body_lines.push_back(line);
			continue;
		}

		// Section headers
		if (tl == "[module.variables]") { sec = ParseSection::Variables; flush_q(); continue; }
		if (tl == "[module.ask]")       { sec = ParseSection::Ask;       flush_q(); continue; }
		if (tl == "[language]")         { sec = ParseSection::None;      flush_q(); continue; }

		// Question block: "? varname"
		if (tl.front() == '?' && sec == ParseSection::Ask) {
			flush_q();
			sec = ParseSection::Question;
			in_q = true;
			cur_q.name = trim(tl.substr(1));
			continue;
		}

		// Module section header (including body)
		if (tl.front() == '[') {
			flush_q();
			sec = ParseSection::Body;
			std::string translated = translate_section_header(tl);
			if (!translated.empty())
				result.body_lines.push_back(translated);
			continue;
		}

		// Key = value
		size_t eq = tl.find('=');
		if (eq == std::string::npos) {
			if (sec == ParseSection::Body) result.body_lines.push_back(line);
			continue;
		}
		std::string key = trim(tl.substr(0, eq));
		std::string val = trim(tl.substr(eq + 1));
		// Strip inline comment
		size_t hash = val.find('#');
		if (hash != std::string::npos) val = trim(val.substr(0, hash));

		switch (sec) {
		case ParseSection::Variables:
			result.variables[key] = infer_value(val);
			break;
		case ParseSection::Ask:
			if (key == "enabled")              result.ask_enabled    = parse_toml_bool(val);
			else if (key == "interactive")     result.interactive    = parse_toml_bool(val);
			else if (key == "loop_until_confirmed") result.loop_confirmed = parse_toml_bool(val);
			break;
		case ParseSection::Question:
			if      (key == "prompt")      cur_q.prompt      = parse_toml_string(val);
			else if (key == "default")     cur_q.default_raw = val;
			else if (key == "enabled_if")  cur_q.enabled_if  = parse_toml_string(val);
			else if (key == "type")        cur_q.type_hint   = parse_toml_string(val);
			else if (key == "choices")     cur_q.choices     = parse_toml_arr(val);
			else if (key == "examples")    cur_q.examples    = parse_toml_arr(val);
			break;
		case ParseSection::Body:
			result.body_lines.push_back(line);
			break;
		default:
			break;
		}
	}
	flush_q();
	return result;
}

// ============================================================================
// Emit flat .vsim
// ============================================================================

static std::string emit_flat(const PreFile& pre, const VarTable& vars)
{
	std::ostringstream out;
	out << "# Generated by vsim-precompile  (WO-75D)\n";
	out << "# Source variables substituted at compile time.\n\n";

	for (const auto& line : pre.body_lines) {
		out << substitute(line, vars) << "\n";
	}
	return out.str();
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char* argv[])
{
	std::string in_path;
	std::string out_path;
	bool non_interactive = false;
	bool dry_run         = false;

	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--non-interactive" || a == "--batch")
			non_interactive = true;
		else if (a == "--dry-run")
			dry_run = true;
		else if (a == "--help" || a == "-h") {
			std::cout <<
				"vsim-precompile  -  .vsim-pre -> .vsim transformation tool\n"
				"Usage: vsim-precompile [--non-interactive] [--dry-run]\n"
				"                       input.vsim-pre [output.vsim]\n";
			return 0;
		} else if (a.front() != '-') {
			if (in_path.empty())  in_path  = a;
			else                  out_path = a;
		}
	}

	if (in_path.empty()) {
		std::cerr << "[ERROR] No input file specified.\n";
		return 1;
	}

	// Default output path: replace .vsim-pre -> .vsim
	if (out_path.empty() && !dry_run) {
		out_path = in_path;
		size_t pos = out_path.rfind(".vsim-pre");
		if (pos != std::string::npos)
			out_path = out_path.substr(0, pos) + "_compiled.vsim";
		else
			out_path = out_path + ".vsim";
	}

	// Parse
	PreFile pre = parse_pre_file(in_path);

	// Asking loop
	bool interactive = pre.ask_enabled && pre.interactive && !non_interactive;
	if (pre.ask_enabled || !pre.questions.empty()) {
		if (!interactive)
			std::cout << "[vsim-precompile] non-interactive mode: using defaults.\n";
		else {
			std::cout << "\nVSIM Pre-run Configuration\n";
			std::cout << std::string(42, '-') << "\n";
		}
		run_asking_loop(pre.questions, pre.variables, interactive, pre.loop_confirmed);
	}

	// Filter body lines by enabled_if (sections without enabled_if pass through)
	// Variable substitution happens in emit_flat

	// Emit
	std::string flat = emit_flat(pre, pre.variables);

	if (dry_run) {
		std::cout << flat;
		return 0;
	}

	std::ofstream of(out_path);
	if (!of) {
		std::cerr << "[ERROR] Cannot write: " << out_path << "\n";
		return 3;
	}
	of << flat;
	std::cout << "[vsim-precompile] Compiled: " << out_path << "\n";
	return 0;
}
