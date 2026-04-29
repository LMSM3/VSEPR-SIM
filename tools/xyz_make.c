/*
 * xyz_make.c  --  VSEPR-SIM  XYZ Make Target Helper
 * ====================================================
 * Colourful interactive make-target browser for the VSEPR build system.
 * Renders a periodic-table-style target grid, atom spin animation, and
 * lets the user select a target to dispatch via CMake.
 *
 * Build:  cc -O2 -std=c99 tools/xyz_make.c -o build/xyz_make
 * CMake:  add_executable(xyz_make tools/xyz_make.c)
 *
 * Part of the VSEPR Make concept — Valence Shell Electron Pair Repulsion
 * build orchestration.  Each target is a "bonding site" on the molecule.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#  define SLEEP_MS(ms) Sleep(ms)
#else
#  include <unistd.h>
#  define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

/* ── ANSI ─────────────────────────────────────────────────────────────── */
#define RST     "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RED     "\033[31m"
#define GRN     "\033[32m"
#define YEL     "\033[33m"
#define BLU     "\033[34m"
#define MAG     "\033[35m"
#define CYN     "\033[36m"
#define WHT     "\033[97m"
#define BGRN    "\033[42m"
#define BRED    "\033[41m"
#define BBLU    "\033[44m"
#define BMAG    "\033[45m"
#define BCYN    "\033[46m"
#define BYEL    "\033[43m"

/* ── Target table ─────────────────────────────────────────────────────── */
typedef struct {
	const char *symbol;   /* 1-3 char XYZ symbol shown in grid cell */
	const char *name;     /* long name */
	const char *color;    /* ANSI fg for cell */
	const char *bg;       /* ANSI bg for cell */
	const char *cmake;    /* cmake --build target name (NULL = phony) */
	const char *desc;     /* one-line description */
} Target;

static const Target TARGETS[] = {
	/* Group 1 — Core libs */
	{"Vc",  "vsepr_core",     CYN,  BBLU, "vsepr_core",     "Header-only core interface"},
	{"Tr",  "vsepr_tracker",  CYN,  BBLU, "vsepr_tracker",  "Particle identity + random picker"},
	{"Sm",  "vsepr_sim",      CYN,  BBLU, "vsepr_sim",      "Main simulation library"},
	{"Pr",  "vsepr_periodic", GRN,  BBLU, "vsepr_periodic", "Periodic table Z=1-102"},
	/* Group 2 — Apps */
	{"At",  "atomistic",      YEL,  BYEL, "atomistic",      "Atomistic sim driver"},
	{"Th",  "thermal-loss",   YEL,  BYEL, "thermal_loss_explorer", "Thermal loss explorer"},
	{"Cl",  "vsepr-cli",      YEL,  BYEL, "vsepr-cli",      "Command-line interface"},
	{"Bio", "bio-report",     YEL,  BYEL, "bio_report_generator","Bio report generator"},
	/* Group 3 — Infra */
	{"In",  "vsepr_infra",    MAG,  BMAG, "vsepr_infra",    "Bootstrap probe, MOTD, NVIDIA TUI"},
	{"Io",  "vsepr_io",       MAG,  BMAG, "vsepr_io",       "I/O layer"},
	{"Ap",  "vsepr_api",      MAG,  BMAG, "vsepr_api",      "API gateway"},
	{"Gl",  "vsepr_glass",    MAG,  BMAG, "vsepr_glass",    "Molecule prerender pipeline"},
	/* Group 4 — Render / VIS */
	{"Vi",  "vsepr_vis",      RED,  BRED, "vsepr_vis",      "Visualisation library"},
	{"Rd",  "vsepr_render",   RED,  BRED, "vsepr_render",   "Render engine"},
	{"Gu",  "vsepr_gui",      RED,  BRED, "vsepr_gui",      "Qt6 GUI application"},
	{"Vw",  "vsepr-view",     RED,  BRED, "vsepr-view",     "OpenGL molecule viewer"},
	/* Group 5 — Tools */
	{"Xy",  "xyz_make",       WHT,  DIM,  "xyz_make",       "This helper (self-ref)"},
	{"Me",  "vsepr-entry",    WHT,  DIM,  "vsepr-entry",    "MOTD precursor renderer"},
	{"Ts",  "tests",          GRN,  BGRN, NULL,             "CTest suite (ctest -R .)"},
	{"Pk",  "package",        GRN,  BGRN, NULL,             "CPack installer package"},
};
#define N_TARGETS ((int)(sizeof(TARGETS)/sizeof(TARGETS[0])))

/* ── Atom spin frames ─────────────────────────────────────────────────── */
static const char *SPIN_FRAMES[] = {
	"   +H+   ",
	"  -H-    ",
	" *H*     ",
	"  -H-    ",
	"   +H+   ",
	"    =H=  ",
	"     *H* ",
	"    =H=  ",
};
#define N_SPIN (int)(sizeof(SPIN_FRAMES)/sizeof(SPIN_FRAMES[0]))

/* ── Orbital electron path ────────────────────────────────────────────── */
static const char *ORB_FRAMES[] = {
	CYN " ◌ " RST,
	GRN " ● " RST,
	YEL " ◉ " RST,
	MAG " ◎ " RST,
	RED " ● " RST,
	BLU " ◌ " RST,
	WHT " · " RST,
	DIM " ○ " RST,
};
#define N_ORB (int)(sizeof(ORB_FRAMES)/sizeof(ORB_FRAMES[0]))

/* ── Helpers ──────────────────────────────────────────────────────────── */
static void enable_ansi(void) {
#ifdef _WIN32
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	GetConsoleMode(h, &mode);
	SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

static void clear_line(void) { printf("\r\033[K"); }

static void spin_atom(int frames, const char *label) {
	for (int i = 0; i < frames; i++) {
		clear_line();
		printf(CYN BOLD "  %s" RST "  %s",
			   SPIN_FRAMES[i % N_SPIN],
			   ORB_FRAMES[i % N_ORB]);
		printf("  " YEL "%s" RST, label);
		fflush(stdout);
		SLEEP_MS(80);
	}
	clear_line();
}

static void print_banner(void) {
	printf(CYN BOLD
		"\n"
		"  ╔═══════════════════════════════════════════════════════════════╗\n"
		"  ║  " WHT "XYZ Make" CYN " — VSEPR-SIM Build Target Browser           v4.0.4  ║\n"
		"  ║  " DIM "Valence Shell Electron Pair Repulsion  ·  CMake + Ninja    " CYN "  ║\n"
		"  ╚═══════════════════════════════════════════════════════════════╝" RST "\n\n"
	);
}

static void print_legend(void) {
	printf("  " BBLU CYN "  Core  " RST
		   "  " BYEL YEL "  Apps  " RST
		   "  " BMAG MAG "  Infra " RST
		   "  " BRED RED "  VIS   " RST
		   "  " DIM "  Tools " RST
		   "  " BGRN GRN "  Test  " RST
		   "\n\n");
}

static void print_grid(void) {
	/* 4 columns */
	int cols = 4;
	printf("  ");
	for (int i = 0; i < N_TARGETS; i++) {
		const Target *t = &TARGETS[i];
		/* cell: BG + bold symbol + number */
		printf("%s%s" BOLD " %-3s" RST "%s"
			   " %-16s" RST,
			   t->bg, t->color, t->symbol, RST,
			   t->name);
		if ((i + 1) % cols == 0)
			printf("\n  ");
		else
			printf("  ");
	}
	printf("\n");
}

static void print_targets(void) {
	printf(BOLD "  #   Symbol  Target                 Description\n" RST);
	printf(DIM  "  ──  ──────  ─────────────────────  ────────────────────────────────\n" RST);
	for (int i = 0; i < N_TARGETS; i++) {
		const Target *t = &TARGETS[i];
		printf("  %s%s%-2d" RST "  %s%-6s" RST "  %-22s %s%s\n",
			   BOLD, CYN, i + 1,
			   t->color, t->symbol,
			   t->name,
			   DIM, t->desc, RST);
	}
	printf("\n");
}

static void dispatch(const Target *t, const char *build_dir) {
	char cmd[512];
	if (t->cmake) {
		snprintf(cmd, sizeof(cmd),
				 "cmake --build %s --target %s --parallel",
				 build_dir, t->cmake);
	} else if (strcmp(t->symbol, "Ts") == 0) {
		snprintf(cmd, sizeof(cmd), "cd %s && ctest --output-on-failure", build_dir);
	} else if (strcmp(t->symbol, "Pk") == 0) {
		snprintf(cmd, sizeof(cmd), "cd %s && cpack", build_dir);
	} else {
		printf(YEL "  ⚠  No cmake target defined for %s\n" RST, t->name);
		return;
	}
	printf(CYN "\n  › %s\n" RST, cmd);
	spin_atom(12, t->name);
	int rc = system(cmd);
	if (rc == 0)
		printf(GRN BOLD "\n  ✓  %s  built OK\n" RST, t->name);
	else
		printf(RED BOLD "\n  ✗  %s  exit %d\n" RST, t->name, rc);
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(int argc, char **argv) {
	enable_ansi();

	const char *build_dir = "build";
	if (argc >= 2) build_dir = argv[1];

	print_banner();
	spin_atom(16, "loading target table …");
	print_legend();
	print_grid();
	printf("\n");
	print_targets();

	/* Interactive loop */
	while (1) {
		printf(GRN BOLD "  xyz> " RST);
		fflush(stdout);

		char line[64];
		if (!fgets(line, sizeof(line), stdin)) break;

		/* strip newline */
		line[strcspn(line, "\r\n")] = '\0';

		if (!*line || strcmp(line, "q") == 0 || strcmp(line, "quit") == 0) {
			printf(DIM "\n  Bye from XYZ Make.\n\n" RST);
			break;
		}
		if (strcmp(line, "?") == 0 || strcmp(line, "help") == 0) {
			print_targets();
			continue;
		}
		if (strcmp(line, "grid") == 0) {
			print_grid();
			continue;
		}

		/* numeric selection */
		char *end;
		long n = strtol(line, &end, 10);
		if (*end == '\0' && n >= 1 && n <= N_TARGETS) {
			dispatch(&TARGETS[n - 1], build_dir);
			continue;
		}

		/* symbol or name lookup (case-insensitive prefix) */
		int found = -1;
		for (int i = 0; i < N_TARGETS; i++) {
			if (strncasecmp(line, TARGETS[i].symbol, strlen(line)) == 0 ||
				strncasecmp(line, TARGETS[i].name,   strlen(line)) == 0 ||
				(TARGETS[i].cmake &&
				 strncasecmp(line, TARGETS[i].cmake, strlen(line)) == 0)) {
				found = i;
				break;
			}
		}
		if (found >= 0) {
			dispatch(&TARGETS[found], build_dir);
		} else {
			printf(RED "  Unknown target '%s'  (type ? for list, q to quit)\n" RST, line);
		}
	}
	return 0;
}
