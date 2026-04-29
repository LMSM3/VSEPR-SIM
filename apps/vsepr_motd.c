/*
 * vsepr_motd.c  --  VSEPR-SIM  Precursor Entry Renderer
 * =======================================================
 * Fires before the interactive shell or any VSEPR-SIM session.
 * Renders:
 *   1. ASCII art VSEPR logo with colour gradient
 *   2. Atom orbital spin animation
 *   3. MOTD panel (version, build info, warnings, tip of the day)
 *   4. Workspace hygiene summary (generated via embedded probe)
 *
 * Build:  cc -O2 -std=c99 apps/vsepr_motd.c -o build/vsepr-entry
 * CMake:  add_executable(vsepr-entry apps/vsepr_motd.c)
 *
 * Exit codes:
 *   0   normal
 *   1   --check-only found warnings (caller may abort)
 *
 * Flags:
 *   --no-spin     skip animation (CI / redirect)
 *   --no-motd     skip MOTD panel
 *   --check-only  warnings only, exit 1 if any
 *   --banner      print just the ASCII art and exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#  define SLEEP_MS(ms)  Sleep(ms)
#  define strncasecmp   _strnicmp
#  define PATH_SEP      "\\"
#else
#  include <unistd.h>
#  define SLEEP_MS(ms)  usleep((ms) * 1000)
#  define PATH_SEP      "/"
#endif

/* ── ANSI palette ────────────────────────────────────────────────────────── */
#define RST   "\033[0m"
#define BOLD  "\033[1m"
#define DIM   "\033[2m"
#define ITAL  "\033[3m"
#define UND   "\033[4m"

#define K "\033[30m"  /* black  */
#define R "\033[31m"  /* red    */
#define G "\033[32m"  /* green  */
#define Y "\033[33m"  /* yellow */
#define B "\033[34m"  /* blue   */
#define M "\033[35m"  /* magenta*/
#define C "\033[36m"  /* cyan   */
#define W "\033[97m"  /* white  */

#define BG_K "\033[40m"
#define BG_R "\033[41m"
#define BG_G "\033[42m"
#define BG_Y "\033[43m"
#define BG_B "\033[44m"
#define BG_M "\033[45m"
#define BG_C "\033[46m"

/* ── Version constants ───────────────────────────────────────────────────── */
#define VSEPR_VERSION    "4.0.4"
#define VSEPR_CODENAME   "Chromatic-Pillar"
#define VSEPR_BUILD_DATE __DATE__
#define VSEPR_BUILD_TIME __TIME__

/* ── ASCII art: VSEPR logo ───────────────────────────────────────────────── */
static void print_logo(void) {
	/* Gradient: V=cyan  S=green  E=yellow  P=magenta  R=red  -SIM=white */
	printf("\n");
	printf(DIM  "     ══════════════════════════════════════════════════════\n" RST);
	printf("\n");
	printf(BOLD C  "      ██╗   ██╗" G  "███████╗" Y  "███████╗" M  "██████╗ " R  " ██████╗ \n" RST);
	printf(BOLD C  "      ██║   ██║" G  "██╔════╝" Y  "██╔════╝" M  "██╔══██╗" R  "██╔═══██╗\n" RST);
	printf(BOLD C  "      ██║   ██║" G  "███████╗" Y  "█████╗  " M  "██████╔╝" R  "██║   ██║\n" RST);
	printf(BOLD C  "      ╚██╗ ██╔╝" G  "╚════██║" Y  "██╔══╝  " M  "██╔═══╝ " R  "██║   ██║\n" RST);
	printf(BOLD C  "       ╚████╔╝ " G  "███████║" Y  "███████╗" M  "██║     " R  "╚██████╔╝\n" RST);
	printf(BOLD C  "        ╚═══╝  " G  "╚══════╝" Y  "╚══════╝" M  "╚═╝     " R  " ╚═════╝ \n" RST);
	printf("\n");
	printf(DIM W   "              Valence Shell Electron Pair Repulsion\n" RST);
	printf(DIM C   "                  Simulation Engine  " BOLD W "v" VSEPR_VERSION RST DIM C
				   "  \"" VSEPR_CODENAME "\"\n" RST);
	printf("\n");
	printf(DIM  "     ══════════════════════════════════════════════════════\n" RST);
	printf("\n");
}

/* ── Small logo (one-liner) for --banner-short ───────────────────────────── */
static void print_logo_short(void) {
	printf(BOLD C " V" G "S" Y "E" M "P" R "R" W "-SIM" RST DIM "  v" VSEPR_VERSION
		   "  \"" VSEPR_CODENAME "\"" RST "\n");
}

/* ── Spin animation ──────────────────────────────────────────────────────── */

/* Orbital path characters — electron sweeping around a nucleus */
static const char *ORB[][2] = {
	{C "   ·   " RST,  C " (  ·  ) " RST},
	{G "  ·    " RST,  G "(   ·   )" RST},
	{Y " ·     " RST,  Y "(·       " RST},
	{M "·      " RST,  M "·        " RST},
	{R " ·     " RST,  R "·        " RST},
	{B "  ·    " RST,  B "(·       " RST},
	{C "   ·   " RST,  C " (  ·  ) " RST},
	{W "    ·  " RST,  W ")   ·   (" RST},
	{G "     · " RST,  G ")     ·  " RST},
	{Y "      ·" RST,  Y ")       ·" RST},
	{M "     · " RST,  M ")       ·" RST},
	{R "    ·  " RST,  R ")   ·   (" RST},
};
#define N_ORB (int)(sizeof(ORB)/sizeof(ORB[0]))

static const char *NUCLEUS[] = {
	BOLD C " ⬡ " RST,
	BOLD G " ⬡ " RST,
	BOLD Y " ⬡ " RST,
	BOLD M " ⬡ " RST,
	BOLD R " ⬡ " RST,
	BOLD B " ⬡ " RST,
};
#define N_NUC (int)(sizeof(NUCLEUS)/sizeof(NUCLEUS[0]))

static const char *SPIN_CHARS = "|/-\\";

static void do_spin(int ticks, const char *label) {
	for (int i = 0; i < ticks; i++) {
		printf("\r  %s  %s  %s  " DIM "%c  %s" RST "   ",
			   ORB[i % N_ORB][0],
			   NUCLEUS[i % N_NUC],
			   ORB[(i + N_ORB/2) % N_ORB][1],
			   SPIN_CHARS[i % 4],
			   label);
		fflush(stdout);
		SLEEP_MS(55);
	}
	printf("\r\033[K");
	fflush(stdout);
}

/* ── MOTD tips ───────────────────────────────────────────────────────────── */
static const char *TIPS[] = {
	"Run  wsm status  inside doc_shell to audit workspace pollution.",
	"Use  make xyz  to browse and dispatch CMake targets interactively.",
	"make debug  builds with debug symbols and full sanitiser output.",
	"wsm log-molecules  snapshots all formula refs before a reset.",
	"pillar 3  inside doc_shell spawns three randomised colour-pillar plants.",
	"make mrproper  nukes build/ AND out/  (non-recoverable — log first!).",
	"The ten colour pillars: WOOD FIRE EARTH METAL WATER NUCLEAR NOBLE PLASTIC CERAMIC PLASMA.",
	"Five-prong atlas: 40 materials, deterministic PVT + saturation grids.",
	"UF6 is the only formula not in the five-prong atlas (intentional gap).",
	"vsepr-entry --no-spin  runs silently in CI environments.",
};
#define N_TIPS (int)(sizeof(TIPS)/sizeof(TIPS[0]))

/* ── Workspace probe ─────────────────────────────────────────────────────── */

typedef struct { long files; double mb; int warnings; } WsProbe;

static WsProbe probe_workspace(void) {
	WsProbe p = {0, 0.0, 0};

	/* Count files in out/ if it exists */
#ifdef _WIN32
	FILE *f = _popen("powershell -NoProfile -Command "
					 "\"(Get-ChildItem out -Recurse -File -ErrorAction SilentlyContinue"
					 " | Measure-Object).Count\" 2>nul", "r");
#else
	FILE *f = popen("find out -type f 2>/dev/null | wc -l", "r");
#endif
	if (f) {
		if (fscanf(f, "%ld", &p.files) != 1) p.files = -1;
#ifdef _WIN32
		_pclose(f);
#else
		pclose(f);
#endif
	}

	/* Disk usage */
#ifdef _WIN32
	FILE *g = _popen("powershell -NoProfile -Command "
					 "\"[math]::Round((Get-ChildItem out -Recurse -File -ErrorAction SilentlyContinue"
					 " | Measure-Object -Sum Length).Sum / 1MB, 1)\" 2>nul", "r");
#else
	FILE *g = popen("du -sm out 2>/dev/null | cut -f1", "r");
#endif
	if (g) {
		if (fscanf(g, "%lf", &p.mb) != 1) p.mb = -1.0;
#ifdef _WIN32
		_pclose(g);
#else
		pclose(g);
#endif
	}

	/* Warn if out/ is very large */
	if (p.mb > 500.0) p.warnings++;

	/* Warn if build/ missing (no compile yet) */
#ifdef _WIN32
	DWORD a = GetFileAttributesA("build");
	if (a == INVALID_FILE_ATTRIBUTES) p.warnings++;
#else
	if (access("build", F_OK) != 0) p.warnings++;
#endif

	return p;
}

/* ── MOTD panel ──────────────────────────────────────────────────────────── */
static void print_motd(int warnings) {
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	char datebuf[64];
	strftime(datebuf, sizeof(datebuf), "%Y-%m-%d  %H:%M:%S", t);

	/* Deterministic tip of the day */
	int tip_idx = (t->tm_yday + t->tm_mday) % N_TIPS;

	printf(DIM "  ┌────────────────────────────────────────────────────────┐\n" RST);
	printf(DIM "  │" RST BOLD C "  VSEPR-SIM  Message of the Day" RST
			   DIM "                           │\n" RST);
	printf(DIM "  ├────────────────────────────────────────────────────────┤\n" RST);

	printf(DIM "  │" RST "  " DIM "Version  " RST BOLD W "v" VSEPR_VERSION RST
			   DIM "  [" VSEPR_CODENAME "]" RST
			   "                           " DIM "│\n" RST);

	printf(DIM "  │" RST "  " DIM "Built    " RST W VSEPR_BUILD_DATE " " VSEPR_BUILD_TIME RST
			   "                              " DIM "│\n" RST);

	printf(DIM "  │" RST "  " DIM "Session  " RST W "%s" RST
			   "                 " DIM "│\n" RST, datebuf);

	printf(DIM "  ├────────────────────────────────────────────────────────┤\n" RST);

	if (warnings > 0) {
		printf(DIM "  │" RST "  " R BOLD "⚠  %d workspace warning(s) detected" RST
				   "                     " DIM "│\n" RST, warnings);
		printf(DIM "  │" RST "  " Y "   Run: wsm status   (inside doc_shell)" RST
				   "              " DIM "│\n" RST);
	} else {
		printf(DIM "  │" RST "  " G "✓  Workspace looks clean" RST
				   "                              " DIM "│\n" RST);
	}

	printf(DIM "  ├────────────────────────────────────────────────────────┤\n" RST);
	printf(DIM "  │" RST "  " DIM "Tip  " RST);

	/* Word-wrap tip to ~50 chars */
	const char *tip = TIPS[tip_idx];
	int col = 0, len = (int)strlen(tip);
	for (int i = 0; i < len; i++) {
		if (col > 49 && tip[i] == ' ') {
			printf("\n" DIM "  │" RST "       ");
			col = 0;
		} else {
			putchar(tip[i]);
			col++;
		}
	}
	/* pad to box edge roughly */
	printf(RST "\n");

	printf(DIM "  └────────────────────────────────────────────────────────┘\n" RST);
	printf("\n");
}

/* ── Entry commands panel ────────────────────────────────────────────────── */
static void print_commands(void) {
	printf(BOLD "  Quick commands\n" RST);
	printf(DIM "  ─────────────────────────────────────────────────────────\n" RST);
	printf("  " BOLD G "make" RST DIM " / " RST BOLD G "make debug" RST
		   "         configure + build (Release / Debug)\n");
	printf("  " BOLD Y "make xyz" RST
		   "               interactive XYZ target browser\n");
	printf("  " BOLD C "python scripts/doc_shell.py" RST
		   "  interactive workspace shell\n");
	printf("  " BOLD M "wsm status" RST
		   "             workspace pollution report  (inside shell)\n");
	printf("  " BOLD R "wsm reset" RST
		   "              staged wipe of generated outputs\n");
	printf("  " BOLD W "make test" RST
		   "              run full CTest suite\n");
	printf("\n");
}

/* ── ANSI enable ─────────────────────────────────────────────────────────── */
static void enable_ansi(void) {
#ifdef _WIN32
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	if (GetConsoleMode(h, &mode))
		SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

/* ── Main ────────────────────────────────────────────────────────────────── */
int main(int argc, char **argv) {
	enable_ansi();

	int spin_enabled = 1;
	int do_motd   = 1;
	int check_only = 0;
	int banner_only = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--no-spin")    == 0) spin_enabled = 0;
		if (strcmp(argv[i], "--no-motd")    == 0) do_motd    = 0;
		if (strcmp(argv[i], "--check-only") == 0) check_only = 1;
		if (strcmp(argv[i], "--banner")     == 0) banner_only = 1;
		if (strcmp(argv[i], "--short")      == 0) {
			print_logo_short();
			return 0;
		}
	}

	if (banner_only) {
		print_logo();
		return 0;
	}

	if (!check_only) {
		print_logo();

		if (spin_enabled) {
			do_spin(28, "initialising \xe2\x80\xa6");
			do_spin(20, "probing workspace \xe2\x80\xa6");
		}
	}

	WsProbe ws = probe_workspace();

	if (check_only) {
		if (ws.warnings > 0) {
			printf(R BOLD "⚠  %d workspace warning(s)" RST "\n", ws.warnings);
			return 1;
		}
		printf(G "✓  workspace OK\n" RST);
		return 0;
	}

	if (do_motd) {
		if (ws.files > 0)
			printf(DIM "  out/  %ld files  (%.1f MB)\n\n" RST, ws.files, ws.mb);
		print_motd(ws.warnings);
	}

	print_commands();

	return 0;
}
