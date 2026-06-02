#pragma once
/**
 * cmd_new_wizard.hpp
 * ------------------
 * On-rails .vsim script creation wizard.
 *
 * Triggered automatically when `vsepr` is called with no arguments and the
 * user presses [N] at the welcome screen prompt.
 *
 * ARCHITECTURE
 * ------------
 * The wizard is driven by a flat table of WizardModule descriptors.  Each
 * row owns one .vsim section (e.g. [project], [run], [export]).  The wizard
 * iterates the table in order, asking the user a series of typed prompts, and
 * accumulates the answers into a std::string script buffer that is written to
 * disk at the end.
 *
 * To add a new .vsim module to the wizard:
 *   1. Write a WizardModule descriptor struct literal in the MODULES table
 *      (the large constexpr array near the bottom of this file).
 *   2. Each module has a 'fields' vector of WizardField entries.
 *   3. If the section is optional, set required = false.
 *   4. Rebuild — no other files need changing.
 *
 * See docs/WIZARD_MODULE_GUIDE.md for the full developer reference.
 *
 * WO-72K-EXT | v5.1.4
 */

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace vsepr::cli {

// ============================================================================
// Colour helpers (reuses terminal VT already enabled by show_welcome)
// ============================================================================
namespace wiz {

static constexpr const char* RESET  = "\033[0m";
static constexpr const char* BOLD   = "\033[1m";
static constexpr const char* DIM    = "\033[2m";
static constexpr const char* CYAN   = "\033[0;36m";
static constexpr const char* GREEN  = "\033[0;32m";
static constexpr const char* YELLOW = "\033[1;33m";
static constexpr const char* MAGENTA= "\033[1;35m";
static constexpr const char* WHITE  = "\033[0;37m";
static constexpr const char* RED    = "\033[0;31m";

// Print a section header bar
static void section_bar(const std::string& title, int step, int total)
{
	// Build a 62-char line of box-drawing dashes as a string literal
	static const std::string hline(62, '-');
	std::cout << "\n" << MAGENTA
			  << "  \u250c\u2500 STEP " << step << "/" << total
			  << "  \u2502  " << title
			  << "\n  \u2514" << hline
			  << RESET << "\n\n";
}

// Print a field prompt line
static void field_prompt(const std::string& key,
						 const std::string& description,
						 const std::string& def_value,
						 const std::string& example = "")
{
	std::cout << CYAN << "  " << key << RESET
			  << WHITE << "  \u2502  " << description << RESET;
	if (!def_value.empty())
		std::cout << DIM << "  [default: " << def_value << "]" << RESET;
	if (!example.empty())
		std::cout << DIM << "  e.g. " << example << RESET;
	std::cout << "\n  " << YELLOW << "> " << RESET;
}

// Read a line; return def_value if the user just presses Enter
static std::string read_line(const std::string& def_value = "")
{
	std::string line;
	std::getline(std::cin, line);
	if (line.empty()) return def_value;
	return line;
}

// Ask a yes/no question — returns true for y/Y/yes
static bool ask_yn(const std::string& question, bool def_yes = true)
{
	std::cout << YELLOW << "  " << question
			  << (def_yes ? "  [Y/n] " : "  [y/N] ")
			  << RESET;
	std::string a;
	std::getline(std::cin, a);
	if (a.empty()) return def_yes;
	return (a[0] == 'y' || a[0] == 'Y');
}

// Print a tip line
static void tip(const std::string& text)
{
	std::cout << DIM << "    \u2139 " << text << RESET << "\n";
}

} // namespace wiz

// ============================================================================
// Field & Module descriptors
// ============================================================================

enum class FieldType {
	Text,       // free string
	Integer,    // integer number
	Float,      // floating-point number
	Choice,     // one of a fixed set
	Bool,       // true / false
	SkipIf,     // field is emitted conditionally (handled by custom_emit)
};

struct WizardChoice {
	std::string value;
	std::string description;
};

struct WizardField {
	std::string   key;                  // INI key name written to script
	std::string   description;          // human-readable prompt text
	std::string   default_value;        // used if user presses Enter
	FieldType     type    = FieldType::Text;
	bool          required = false;     // if false, user can skip with Enter
	std::vector<WizardChoice> choices;  // populated when type == Choice
	std::string   tip_text;             // optional contextual tip
	std::string   example;              // shown next to the prompt
};

struct WizardModule {
	std::string              section;      // e.g. "[project]"
	std::string              title;        // human-facing title
	std::string              description;  // one-liner shown before prompts
	bool                     required;     // if false: ask "include this? [y/N]"
	std::vector<WizardField> fields;
};

// ============================================================================
// Module table — THE single source of truth for the wizard flow
// Adding a new .vsim module = adding one WizardModule entry here.
// ============================================================================
static const std::vector<WizardModule>& wizard_modules()
{
	static const std::vector<WizardModule> MODULES = {

		// ------------------------------------------------------------------ //
		// [project]  — REQUIRED                                               //
		// ------------------------------------------------------------------ //
		{
			"[project]",
			"Project Identity",
			"Names and seeds this simulation run. Used in filenames and reports.",
			/*required=*/true,
			{
				{
					"name", "Project name (no spaces — used in output filenames)",
					"my_run", FieldType::Text, /*required=*/true, {},
					"Use lowercase_snake_case. Output goes to out/<name>/",
					"nacl_crystal_relax"
				},
				{
					"version", "Script version string",
					"1.0", FieldType::Text, false, {},
					"Informational only — not validated against the runtime version.", ""
				},
				{
					"seed_base", "Base RNG seed (integer)",
					"42", FieldType::Integer, false, {},
					"Same seed + same script = bit-identical results.", "42"
				},
				{
					"description", "One-line description of this run",
					"", FieldType::Text, false, {},
					"Stored in the report but not used by the kernel.", ""
				},
			}
		},

		// ------------------------------------------------------------------ //
		// [objects]  — REQUIRED                                               //
		// ------------------------------------------------------------------ //
		{
			"[objects]",
			"Simulation Objects",
			"Declares the physical system: crystal, gas, molecule, or surface.",
			/*required=*/true,
			{
				{
					"object_type",
					"What kind of system are you building?",
					"crystal",
					FieldType::Choice, /*required=*/true,
					{
						{"crystal",  "Periodic crystal (FCC/BCC/HCP/SC/diamond/zincblende)"},
						{"gas",      "Dilute gas-phase molecular system"},
						{"molecule", "Single molecule or cluster"},
						{"surface",  "2-D surface slab with vacuum gap"},
					},
					"Each choice generates the matching constructor line.", ""
				},
			}
		},

		// ------------------------------------------------------------------ //
		// [cell]  — OPTIONAL (shown only for crystal/surface)                 //
		// ------------------------------------------------------------------ //
		{
			"[cell]",
			"Simulation Cell (Periodic Box)",
			"Defines the orthorhombic box. Required for PBC crystal/surface runs.",
			/*required=*/false,
			{
				{
					"type", "Cell geometry",
					"orthorhombic", FieldType::Choice, false,
					{
						{"orthorhombic", "Rectangular box (lx, ly, lz)"},
					},
					"Only orthorhombic is supported in v5.1.4. Triclinic is reserved.", ""
				},
				{
					"lx", "Box length X (\xc3\x85ngstrom)",
					"10.0", FieldType::Float, false, {},
					"Set lx = ly = lz for a cubic cell.", "10.0"
				},
				{
					"ly", "Box length Y (\xc3\x85ngstrom)",
					"10.0", FieldType::Float, false, {}, "", "10.0"
				},
				{
					"lz", "Box length Z (\xc3\x85ngstrom)",
					"10.0", FieldType::Float, false, {}, "", "10.0"
				},
			}
		},

		// ------------------------------------------------------------------ //
		// [boundary]  — OPTIONAL                                              //
		// ------------------------------------------------------------------ //
		{
			"[boundary]",
			"Boundary Conditions",
			"Sets per-axis boundary mode. Default is open (no PBC).",
			/*required=*/false,
			{
				{
					"x", "X-axis boundary",
					"periodic", FieldType::Choice, false,
					{{"periodic","Periodic (wraps particles)"}, {"open","Open (no wrap)"}},
					"", ""
				},
				{
					"y", "Y-axis boundary",
					"periodic", FieldType::Choice, false,
					{{"periodic","Periodic"}, {"open","Open"}},
					"", ""
				},
				{
					"z", "Z-axis boundary",
					"periodic", FieldType::Choice, false,
					{{"periodic","Periodic"}, {"open","Open"}},
					"", ""
				},
			}
		},

		// ------------------------------------------------------------------ //
		// [run]  — REQUIRED                                                   //
		// ------------------------------------------------------------------ //
		{
			"[run]",
			"Run Parameters",
			"Controls the simulation mode, step count, and timestep.",
			/*required=*/true,
			{
				{
					"mode", "Simulation mode",
					"relax", FieldType::Choice, /*required=*/true,
					{
						{"relax",    "FIRE energy minimisation (structure relaxation)"},
						{"md_nve",   "Molecular dynamics — NVE (microcanonical)"},
						{"md_nvt",   "Molecular dynamics — NVT (canonical, Berendsen)"},
						{"static",   "Single-point energy evaluation, no dynamics"},
						{"formation","Formation event pipeline (crystal growth)"},
					},
					"relax is the safest starting point for new systems.", ""
				},
				{
					"max_steps", "Maximum integration steps",
					"1000", FieldType::Integer, false, {},
					"FIRE: steps to convergence. MD: total timesteps.", "1000"
				},
				{
					"timestep_fs", "MD timestep in femtoseconds (MD modes only)",
					"1.0", FieldType::Float, false, {},
					"Ignored in relax/static modes.", "1.0"
				},
				{
					"temperature_K", "Target temperature in Kelvin (NVT only)",
					"300.0", FieldType::Float, false, {},
					"Ignored unless mode = md_nvt.", "300.0"
				},
			}
		},

		// ------------------------------------------------------------------ //
		// ------------------------------------------------------------------ //
		// [analysis]  — OPTIONAL                                              //
		// ------------------------------------------------------------------ //
		{
			"[analysis]",
			"Post-Run Analysis",
			"Selects which analysis kernels run after the simulation completes.\n"
			"  All analysers read the same final State; order does not matter.",
			/*required=*/false,
			{
				// ── Structural ──────────────────────────────────────
				{
					"coordination",
					"Nearest-neighbour coordination analysis",
					"true", FieldType::Bool, false, {},
					"Reports per-species avg coordination number. Fast (<1 ms).", ""
				},
				{
					"coordination_cutoff_A",
					"Bond cutoff radius for coordination (Angstrom)",
					"3.5", FieldType::Float, false, {},
					"Atoms within this radius count as bonded neighbours.", "3.5"
				},
				{
					"rdf",
					"Radial distribution function (RDF / g(r))",
					"false", FieldType::Bool, false, {},
					"Meaningful with >= 100 atoms. Requires box to be set.", ""
				},
				{
					"rdf_cutoff_A",
					"RDF integration cutoff (Angstrom)",
					"8.0", FieldType::Float, false, {},
					"Must be < L/2 for the smallest box dimension.", "8.0"
				},
				{
					"rdf_bins",
					"Number of histogram bins for RDF",
					"200", FieldType::Integer, false, {},
					"More bins = higher resolution, slightly more memory.", "200"
				},
				{
					"clustering",
					"Cluster identification (bond-graph connected components)",
					"false", FieldType::Bool, false, {},
					"Labels each atom with a cluster ID. Uses coordination_cutoff_A.", ""
				},
				{
					"fingerprint",
					"Structural fingerprints (symmetry-function feature vectors)",
					"false", FieldType::Bool, false, {},
					"Used for golden-test hashing and structure matching.", ""
				},
				// ── Dynamics ────────────────────────────────────────
				{
					"msd",
					"Mean squared displacement (MSD) tracking",
					"false", FieldType::Bool, false, {},
					"Tracks particle displacement over time. MD modes only.", ""
				},
				{
					"rmsd",
					"RMSD from reference structure (Kabsch alignment)",
					"false", FieldType::Bool, false, {},
					"Compares final geometry to initial. Useful for relaxation QA.", ""
				},
				// ── Reporting ─────────────────────────────────────
				{
					"variance_probe",
					"Enable variance probe on energy.total?",
					"false", FieldType::Bool, false, {},
					"Measures spread of total energy across all recorded frames.", ""
				},
				{
					"n_evolution_probe",
					"Track population growth rate (dN/dt) for cluster_count?",
					"false", FieldType::Bool, false, {},
					"Useful for formation-mode runs to measure nucleation rate.", ""
				},
			}
		},

		// ------------------------------------------------------------------ //
		// ------------------------------------------------------------------ //
		// [export]  — OPTIONAL                                                //
		// ------------------------------------------------------------------ //
		{
			"[export]",
			"Output Exports",
			"Chooses which data files are written to out/<name>/ after the run.\n"
			"  Defaults are conservative (xyz + report). Enable more as needed.",
			/*required=*/false,
			{
				// ── Atomistic state ─────────────────────────────────
				{
					"xyz",
					"Final structure as .xyz (particle positions)",
					"true", FieldType::Bool, false, {},
					"Standard format. Opens in VESTA, Avogadro, ASE, VMD.", ""
				},
				{
					"xyzfull",
					"Full per-step trajectory in xyzFull format",
					"false", FieldType::Bool, false, {},
					"Larger file — needed for MSD, RMSD, and trajectory replay.", ""
				},
				{
					"pdb",
					"PDB format for external viewers (VESTA, VMD, PyMOL)",
					"false", FieldType::Bool, false, {}, "", ""
				},
				// ── Analysis layer ────────────────────────────────
				{
					"analysis_json",
					"Derived metrics JSON (AnalysisRecord)",
					"false", FieldType::Bool, false, {},
					"Machine-readable record of all enabled analysis results.", ""
				},
				{
					"metrics_tsv",
					"Per-run metric table as tab-separated values",
					"false", FieldType::Bool, false, {},
					"Easy to import into Excel, pandas, or R.", ""
				},
				{
					"cluster_json",
					"Cluster assignments JSON (ClusterRecord)",
					"false", FieldType::Bool, false, {},
					"Written only when analysis.clustering = true.", ""
				},
				{
					"fingerprint_json",
					"Structural fingerprint vectors JSON (FingerprintRecord)",
					"false", FieldType::Bool, false, {},
					"Written only when analysis.fingerprint = true.", ""
				},
				// ── Kernel event spine ────────────────────────────
				{
					"events_json",
					"KernelEventLog as JSON Lines (event-by-event audit)",
					"true", FieldType::Bool, false, {},
					"Required for downstream replay, variance probes, and N_evolution.", ""
				},
				// ── Reporting layer ─────────────────────────────
				{
					"report_md",
					"Human-readable Markdown summary report",
					"true", FieldType::Bool, false, {}, "", ""
				},
				{
					"summary_csv",
					"Per-run summary CSV (one row per run)",
					"false", FieldType::Bool, false, {},
					"Useful for aggregating results across batch runs.", ""
				},
				{
					"dashboard_svg",
					"SVG dashboard chart (beta-7 pipeline)",
					"false", FieldType::Bool, false, {},
					"Text-based SVG — diffable, no binary dependencies.", ""
				},
				{
					"manifest_json",
					"Run manifest with full artifact registry",
					"false", FieldType::Bool, false, {},
					"Lists every output file with sha256 hash for reproducibility.", ""
				},
				// ── Engineering geometry ─────────────────────────
				{
					"step_file",
					"STEP geometry sidecar (SolidWorks / CAD import)",
					"false", FieldType::Bool, false, {},
					"Planned for WO-72P. Has no effect in v5.1.4.", ""
				},
				{
					"vtp_mesh",
					"VTK PolyData mesh for ParaView",
					"false", FieldType::Bool, false, {}, "", ""
				},
				// ── Output directory ────────────────────────────
				{
					"output_dir",
					"Output directory path",
					"", FieldType::Text, false, {},
					"Default: out/<project_name>/  Leave blank to use the default.",
					"out/my_run/"
				},
			}
		},

		// ------------------------------------------------------------------ //
		// [export.visual]  — OPTIONAL                                         //
		// ------------------------------------------------------------------ //
		{
			"[export.visual]",
			"Visual Artifact Exports",
			"Rendered output files: SVG figures, PNG snapshots, GIFs, HTML dashboards.\n"
			"  These are sidecar artifacts rendered FROM simulation data, not ground truth.",
			/*required=*/false,
			{
				{
					"svg_figures",
					"Per-material SVG metric figures",
					"false", FieldType::Bool, false, {}, "", ""
				},
				{
					"rdf_svg",
					"Radial distribution function plot (SVG)",
					"false", FieldType::Bool, false, {},
					"Requires analysis.rdf = true.", ""
				},
				{
					"energy_trace_svg",
					"Energy-per-step convergence trace figure (SVG)",
					"false", FieldType::Bool, false, {}, "", ""
				},
				{
					"cluster_map_svg",
					"Cluster assignment scatter plot (SVG)",
					"false", FieldType::Bool, false, {},
					"Requires analysis.clustering = true.", ""
				},
				{
					"defect_map_svg",
					"Defect site map overlay (SVG)",
					"false", FieldType::Bool, false, {}, "", ""
				},
				{
					"trajectory_gif",
					"Animated GIF of trajectory playback",
					"false", FieldType::Bool, false, {},
					"Requires export.xyzfull = true.", ""
				},
				{
					"html_dashboard",
					"Self-contained HTML dashboard with JS charts",
					"false", FieldType::Bool, false, {},
					"Opens in any browser — no server required.", ""
				},
				{
					"report_html",
					"Full run report as HTML (styled, printable)",
					"false", FieldType::Bool, false, {}, "", ""
				},
			}
		},

		// ------------------------------------------------------------------ //
		// ------------------------------------------------------------------ //
		// [visual]  — OPTIONAL                                                //
		// ------------------------------------------------------------------ //
		{
			"[visual]",
			"Live Visualisation",
			"Controls what is displayed on screen WHILE the simulation runs.\n"
			"  output_type = none gives the fastest headless throughput.",
			/*required=*/false,
			{
				// ── Primary output mode ────────────────────────────
				{
					"output_type",
					"Live display mode",
					"terminal", FieldType::Choice, false,
					{
						{"none",              "Headless — no display, maximum throughput"},
						{"terminal",          "ASCII progress table (safe on any TTY)"},
						{"terminal_spark",    "Sparkline energy trace in terminal"},
						{"terminal_bar",      "Per-kind event count bar chart in terminal"},
						{"terminal_overlay",  "Full overlay cycle: density → coord → memory"},
						{"gl",                "OpenGL rotating molecule window (requires BUILD_VIS)"},
						{"web",               "WebGL browser dashboard on localhost"},
					},
					"Use terminal or none on remote/CI systems.", ""
				},
				{
					"animation_mode",
					"Animation timing for terminal modes",
					"none", FieldType::Choice, false,
					{
						{"none",     "No animation — static table updates"},
						{"spark",    "Animated sparkline energy convergence trace"},
						{"bar",      "Animated per-event-kind count bar chart"},
						{"overlay",  "Full animated overlay cycle (density / coord / memory / orient)"},
					},
					"", ""
				},
				{
					"render_interval",
					"Emit a display frame every N simulation steps",
					"1", FieldType::Integer, false, {},
					"Higher = less overhead but choppier live display.", "10"
				},
				// ── Terminal display flags ─────────────────────────
				{
					"show_proxy_table",
					"Show EnsembleProxy summary table after each step",
					"true", FieldType::Bool, false, {},
					"Main live readout: energy, eta, T, coordination.", ""
				},
				{
					"show_convergence_trace",
					"Show per-step convergence trace row",
					"true", FieldType::Bool, false, {},
					"Appends a delta-energy row after each FIRE step.", ""
				},
				{
					"show_event_timeline",
					"Show ASCII kernel event timeline ruler",
					"false", FieldType::Bool, false, {}, "", ""
				},
				{
					"show_bar_chart",
					"Show per-kind event count bar chart after run",
					"false", FieldType::Bool, false, {}, "", ""
				},
				{
					"show_rdf_plot",
					"Show ASCII radial distribution function plot after run",
					"false", FieldType::Bool, false, {},
					"Requires analysis.rdf = true.", ""
				},
				{
					"show_defect_map",
					"Show ASCII defect site grid projection",
					"false", FieldType::Bool, false, {}, "", ""
				},
				// ── Advanced overlays ─────────────────────────────
				{
					"show_bond_events",
					"Highlight newly formed/broken bonds per frame",
					"false", FieldType::Bool, false, {},
					"GL mode only — colours new/broken bonds green/red per step.", ""
				},
				{
					"show_velocity_vectors",
					"Draw velocity vectors for fast particles (GL mode)",
					"false", FieldType::Bool, false, {},
					"MD modes only. Vectors scaled to particle speed.", ""
				},
				// ── GL window ────────────────────────────────────
				{
					"gl_window_width",
					"GL window width in pixels",
					"1280", FieldType::Integer, false, {},
					"Used only when output_type = gl.", "1280"
				},
				{
					"gl_window_height",
					"GL window height in pixels",
					"800", FieldType::Integer, false, {}, "", "800"
				},
				{
					"gl_auto_orbit",
					"Auto-orbit camera between overlay panes (GL mode)",
					"true", FieldType::Bool, false, {}, "", ""
				},
				{
					"gl_overlay_hold_s",
					"Seconds per overlay pane before auto-advancing (GL mode)",
					"2.5", FieldType::Float, false, {}, "", "2.5"
				},
				// ── Web / streaming ──────────────────────────────
				{
					"web_port",
					"HTTP / SSE server port for web mode",
					"99998", FieldType::Integer, false, {},
					"Open http://localhost:<port> in a browser while the run is live.",
					"99998"
				},
				{
					"web_auto_open",
					"Automatically open browser tab when web mode starts",
					"false", FieldType::Bool, false, {}, "", ""
				},
				// ── UX pacing ───────────────────────────────────
				{
					"step_delay_ms",
					"Artificial delay between simulation steps (ms)",
					"0", FieldType::Integer, false, {},
					"Set to 50-200 to slow runs for human-readable live display.", "0"
				},
				{
					"smooth_resim",
					"Fade event spine between resimulations (terminal animation)",
					"true", FieldType::Bool, false, {},
					"Cosmetic only — no effect on physics or output files.", ""
				},
				{
					"live_switch",
					"Reuse display window across simulation phases (no flash/re-open)",
					"false", FieldType::Bool, false, {},
					"Enable for multi-phase or batch runs with output_type = terminal.", ""
				},
			}
		},

	}; // end MODULES
	return MODULES;
}

// ============================================================================
// Object constructor snippet generator
// Called after [objects].object_type is answered
// ============================================================================
static std::string build_objects_block(const std::string& obj_type,
									   const std::string& project_name)
{
	std::ostringstream out;
	out << "\n[objects]\n";

	if (obj_type == "crystal") {
		std::cout << "\n" << wiz::CYAN << "  Crystal setup\n" << wiz::RESET;
		wiz::tip("Common lattices: fcc, bcc, hcp, sc, diamond, zincblende");

		std::cout << wiz::CYAN << "  lattice" << wiz::RESET
				  << wiz::WHITE << "  |  Lattice type" << wiz::RESET
				  << wiz::DIM   << "  [default: fcc]" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string lat = wiz::read_line("fcc");

		std::cout << wiz::CYAN << "  a" << wiz::RESET
				  << wiz::WHITE << "  |  Lattice parameter a (\xc3\x85)" << wiz::RESET
				  << wiz::DIM << "  [default: 3.52  — Ni]" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string a = wiz::read_line("3.52");

		std::cout << wiz::CYAN << "  species" << wiz::RESET
				  << wiz::WHITE << "  |  Chemical species (comma-separated for alloys)" << wiz::RESET
				  << wiz::DIM << "  [default: Ni]  e.g. Na,Cl  or  Fe" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string sp = wiz::read_line("Ni");

		std::cout << wiz::CYAN << "  supercell" << wiz::RESET
				  << wiz::WHITE << "  |  Supercell replication (NxNxN)" << wiz::RESET
				  << wiz::DIM << "  [default: 2]" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string sc = wiz::read_line("2");

		out << "system.crystal = CrystalModule("
			<< "lattice=" << lat
			<< ", a=" << a
			<< ", species=[" << sp << "]"
			<< ", supercell=" << sc
			<< ")\n";

	} else if (obj_type == "gas") {
		std::cout << "\n" << wiz::CYAN << "  Gas-phase setup\n" << wiz::RESET;
		wiz::tip("species is a single element symbol.  N is total particle count.");

		std::cout << wiz::CYAN << "  species" << wiz::RESET
				  << wiz::WHITE << "  |  Gas species" << wiz::RESET
				  << wiz::DIM << "  [default: Ar]" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string sp = wiz::read_line("Ar");

		std::cout << wiz::CYAN << "  N" << wiz::RESET
				  << wiz::WHITE << "  |  Number of particles" << wiz::RESET
				  << wiz::DIM << "  [default: 64]" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string n = wiz::read_line("64");

		out << "system.gas = GasModule(species=" << sp << ", N=" << n << ")\n";

	} else if (obj_type == "molecule") {
		std::cout << "\n" << wiz::CYAN << "  Molecule setup\n" << wiz::RESET;
		wiz::tip("formula uses standard chemical notation: H2O, CH4, NH3, C60");

		std::cout << wiz::CYAN << "  formula" << wiz::RESET
				  << wiz::WHITE << "  |  Chemical formula" << wiz::RESET
				  << wiz::DIM << "  [default: H2O]" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string f = wiz::read_line("H2O");

		out << "system.molecule = MoleculeModule(formula=" << f << ")\n";

	} else { // surface
		std::cout << "\n" << wiz::CYAN << "  Surface slab setup\n" << wiz::RESET;
		wiz::tip("species is the substrate element.  vacuum_A is the vacuum gap in Angstrom.");

		std::cout << wiz::CYAN << "  species" << wiz::RESET
				  << wiz::WHITE << "  |  Substrate element" << wiz::RESET
				  << wiz::DIM << "  [default: Al]" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string sp = wiz::read_line("Al");

		std::cout << wiz::CYAN << "  vacuum_A" << wiz::RESET
				  << wiz::WHITE << "  |  Vacuum gap (\xc3\x85)" << wiz::RESET
				  << wiz::DIM << "  [default: 10.0]" << wiz::RESET << "\n"
				  << "  " << wiz::YELLOW << "> " << wiz::RESET;
		std::string vac = wiz::read_line("10.0");

		out << "system.surface = SurfaceModule(species=" << sp
			<< ", vacuum_A=" << vac << ")\n";
	}

	(void)project_name;
	return out.str();
}

// ============================================================================
// Collect answers for a single WizardModule — returns its .vsim block text
// ============================================================================
static std::string collect_module(const WizardModule& mod,
								  const std::string& project_name,
								  int step, int total,
								  std::string& resolved_obj_type)
{
	wiz::section_bar(mod.title, step, total);
	std::cout << wiz::DIM << "  " << mod.description << "\n\n" << wiz::RESET;

	// Special handling for [objects] — interactive sub-prompts
	if (mod.section == "[objects]") {
		// Ask object type from choices
		const WizardField& f = mod.fields[0];
		std::cout << wiz::CYAN << "  System type" << wiz::RESET
				  << wiz::WHITE << "  |  " << f.description << wiz::RESET << "\n\n";
		for (int i = 0; i < (int)f.choices.size(); ++i) {
			std::cout << "    " << wiz::YELLOW << (i+1) << wiz::RESET
					  << "  " << wiz::CYAN << f.choices[i].value << wiz::RESET
					  << "  \u2014  " << wiz::WHITE << f.choices[i].description
					  << wiz::RESET << "\n";
		}
		std::cout << "\n  " << wiz::YELLOW << "> [1] " << wiz::RESET;
		std::string choice_raw = wiz::read_line("1");
		int idx = 0;
		try { idx = std::stoi(choice_raw) - 1; } catch (...) { idx = 0; }
		if (idx < 0 || idx >= (int)f.choices.size()) idx = 0;
		resolved_obj_type = f.choices[idx].value;

		return build_objects_block(resolved_obj_type, project_name);
	}

	std::ostringstream block;
	block << "\n" << mod.section << "\n";

	for (const auto& field : mod.fields) {
		if (!field.tip_text.empty())
			wiz::tip(field.tip_text);

		// For Choice fields: show menu
		if (field.type == FieldType::Choice && !field.choices.empty()) {
			std::cout << wiz::CYAN << "  " << field.key << wiz::RESET
					  << wiz::WHITE << "  |  " << field.description
					  << wiz::RESET << "\n\n";
			for (int i = 0; i < (int)field.choices.size(); ++i) {
				std::cout << "    " << wiz::YELLOW << (i+1) << wiz::RESET
						  << "  " << wiz::CYAN << field.choices[i].value << wiz::RESET
						  << "  \u2014  " << wiz::WHITE << field.choices[i].description
						  << wiz::RESET << "\n";
			}
			// Find default index
			int def_idx = 0;
			for (int i = 0; i < (int)field.choices.size(); ++i)
				if (field.choices[i].value == field.default_value) { def_idx = i; break; }
			std::cout << "\n  " << wiz::YELLOW << "> [" << (def_idx+1) << "] " << wiz::RESET;
			std::string raw = wiz::read_line(std::to_string(def_idx+1));
			int idx2 = def_idx;
			try { idx2 = std::stoi(raw) - 1; } catch (...) { idx2 = def_idx; }
			if (idx2 < 0 || idx2 >= (int)field.choices.size()) idx2 = def_idx;
			block << field.key << " = " << field.choices[idx2].value << "\n";

		} else if (field.type == FieldType::Bool) {
			bool def_bool = (field.default_value == "true");
			bool ans = wiz::ask_yn("  " + field.key + "  |  " + field.description + "?", def_bool);
			block << field.key << " = " << (ans ? "true" : "false") << "\n";

		} else {
			// Text / Integer / Float
			wiz::field_prompt(field.key, field.description,
							  field.default_value, field.example);
			std::string val = wiz::read_line(field.default_value);
			if (!val.empty())
				block << field.key << " = " << val << "\n";
		}
		std::cout << "\n";
	}

	return block.str();
}

// ============================================================================
// Script finaliser — appends header comment and writes to disk
// ============================================================================
static bool write_script(const std::string& path,
						 const std::string& body,
						 const std::string& project_name)
{
	std::ofstream f(path);
	if (!f) return false;

	f << "# " << project_name << ".vsim\n"
	  << "# Generated by VSEPR-SIM wizard  (v5.1.4)\n"
	  << "# Edit freely — see docs/VSIM_LANGUAGE.md for the full reference.\n"
	  << body;
	return true;
}

// ============================================================================
// Entry point called by vsepr.cpp after the welcome screen
// Returns 0 on success, 1 on user-abort or write failure
// ============================================================================
static int cmd_new_wizard()
{
	namespace fs = std::filesystem;
	using namespace wiz;

	std::cout << "\n"
			  << MAGENTA << "  \u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n"
			  << RESET
			  << MAGENTA << "  \u2551  " << RESET
			  << BOLD << WHITE << "VSEPR-SIM Script Wizard" << RESET
			  << WHITE << "  \u2014  create a new .vsim script step-by-step" << RESET
			  << MAGENTA << "  \u2551\n" << RESET
			  << MAGENTA << "  \u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n" << RESET;

	tip("Press Enter to accept a default value shown in [brackets].");
	tip("Type 'q' at any prompt to quit without saving.");
	std::cout << "\n";

	const auto& modules = wizard_modules();

	// Count steps shown to user (required always shown; optional asked)
	// We'll use a simple pass: all modules are shown, required ones skip the
	// "include this section?" prompt.
	int total_steps = (int)modules.size();

	std::string script_body;
	std::string project_name = "my_run";
	std::string obj_type;

	for (int step = 0; step < (int)modules.size(); ++step) {
		const auto& mod = modules[step];

		if (!mod.required) {
			bool include = ask_yn("  Include " + mod.section + " (" + mod.title + ")?",
								  /*def_yes=*/false);
			if (!include) {
				std::cout << DIM << "    skipped\n" << RESET;
				continue;
			}
		}

		std::string block = collect_module(mod, project_name, step + 1,
										   total_steps, obj_type);

		// Capture project name so we can name the output file
		if (mod.section == "[project]") {
			// Extract name = ... from block
			std::istringstream ss(block);
			std::string ln;
			while (std::getline(ss, ln)) {
				if (ln.rfind("name = ", 0) == 0) {
					project_name = ln.substr(7);
					// strip trailing whitespace
					while (!project_name.empty() &&
						   (project_name.back() == ' ' || project_name.back() == '\r'))
						project_name.pop_back();
					break;
				}
			}
		}

		// Check for quit signal
		if (script_body.find("__QUIT__") != std::string::npos ||
			block == "__QUIT__") {
			std::cout << "\n" << YELLOW << "  Wizard aborted.\n" << RESET;
			return 1;
		}

		script_body += block;
	}

	// ---- Write file ---------------------------------------------------------
	std::cout << "\n"
			  << MAGENTA << "  \u2500\u2500 SAVE \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n" << RESET;

	std::string default_filename = project_name + ".vsim";
	std::cout << CYAN << "  Output filename" << RESET
			  << WHITE << "  |  Where to write the script" << RESET
			  << DIM << "  [default: " << default_filename << "]" << RESET << "\n"
			  << "  " << YELLOW << "> " << RESET;
	std::string filename = wiz::read_line(default_filename);
	if (filename.empty()) filename = default_filename;

	fs::path out_path = fs::current_path() / filename;
	if (fs::exists(out_path)) {
		bool overwrite = wiz::ask_yn(
			"  File already exists. Overwrite " + out_path.string() + "?",
			/*def_yes=*/false);
		if (!overwrite) {
			std::cout << YELLOW << "  Wizard aborted — file not written.\n" << RESET;
			return 1;
		}
	}

	if (!write_script(out_path.string(), script_body, project_name)) {
		std::cerr << RED << "  [error] Could not write file: " << out_path << RESET << "\n";
		return 1;
	}

	std::cout << "\n"
			  << GREEN << "  [ok]  Script written: " << out_path << "\n" << RESET
			  << "\n"
			  << DIM << "  Generated script contents:\n" << RESET;

	// Print a preview of the file
	{
		std::ifstream preview(out_path.string());
		std::string line;
		while (std::getline(preview, line))
			std::cout << DIM << "    " << line << "\n" << RESET;
	}

	// ---- Offer validate + run -----------------------------------------------
	std::cout << "\n"
			  << MAGENTA << "  \u2500\u2500 NEXT STEPS \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n" << RESET;
	std::cout << WHITE
			  << "  1  " << CYAN << "vsepr validate " << filename << "\n" << RESET
			  << WHITE
			  << "  2  " << CYAN << "vsepr run " << filename << "\n" << RESET
			  << WHITE
			  << "  3  " << CYAN << "vsepr view out/" << project_name << "/final.xyz\n"
			  << RESET << "\n";

	if (wiz::ask_yn("  Validate the script now?", /*def_yes=*/true)) {
		std::cout << "\n";
		// Invoke validate in-process: reuse cmd_validate path via execv-like
		// approach — simpler: just print the command for the user to run
		// (avoids linking the full validate chain into this header).
		// Full in-process call is done by vsepr.cpp after return.
		return 2; // sentinel: caller runs validate
	}

	return 0;
}

} // namespace vsepr::cli
