#!/usr/bin/env bash
# =============================================================================
# examples/rotation/run_rotation.sh
#
# Launches the vsepr GL simulation window AND the Python live viewer window
# at the same time.  Ctrl+C shuts both down cleanly.
#
# Usage:
#   bash examples/rotation/run_rotation.sh                  -- sphere_shaded
#   bash examples/rotation/run_rotation.sh !new             -- wipe outputs first
#   bash examples/rotation/run_rotation.sh cube             -- cube BCC scene
#   bash examples/rotation/run_rotation.sh sphere           -- sphere shell scene
#   bash examples/rotation/run_rotation.sh sphere_shaded    -- full pipeline
#   bash examples/rotation/run_rotation.sh cube !new
#
# !new  :  delete previous out/ artifacts before launching (fresh run)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${ROOT_DIR}"

VSEPR="${ROOT_DIR}/build/vsepr.exe"
PYTHON="${PYTHON:-python3}"

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

banner() {
	echo -e "${BOLD}${CYAN}"
	echo "  ╔══════════════════════════════════╗"
	echo "  ║   XSIM  rotation  launcher       ║"
	echo "  ║   GL viewer  +  Python live win  ║"
	echo "  ╚══════════════════════════════════╝"
	echo -e "${NC}"
}

# ---- defaults ----------------------------------------------------------------
SCENE="sphere_shaded"
DO_NEW=false

for arg in "$@"; do
	case "${arg}" in
		"!new")          DO_NEW=true ;;
		"cube")          SCENE="cube" ;;
		"sphere")        SCENE="sphere" ;;
		"sphere_shaded") SCENE="sphere_shaded" ;;
		*)
			echo -e "${RED}[run_rotation]${NC} unknown arg: ${arg}"
			echo "  valid: cube | sphere | sphere_shaded | !new"
			exit 1 ;;
	esac
done

banner

# ---- !new: wipe outputs ------------------------------------------------------
if [ "${DO_NEW}" = true ]; then
	echo -e "${YELLOW}[!new]${NC} wiping previous outputs..."
	rm -rf \
		"${ROOT_DIR}/out/cube_rotation" \
		"${ROOT_DIR}/out/sphere_rotation" \
		"${ROOT_DIR}/out/sphere_shaded_sim" \
		"${ROOT_DIR}/out/sphere_shaded" \
		"${ROOT_DIR}/runs/sphere_shaded"
	echo -e "${GREEN}[!new]${NC} outputs cleared."
fi

# ---- scene selection ---------------------------------------------------------
case "${SCENE}" in
	"cube")
		echo -e "${CYAN}[scene]${NC} cube  (BCC Fe, Y-spin 40 deg/s)"
		WATCH_DIR="${ROOT_DIR}/out/cube_rotation"
		VSIM_CMD=("${VSEPR}" scene "${SCRIPT_DIR}/cube.x")
		;;
	"sphere")
		echo -e "${CYAN}[scene]${NC} sphere  (Ar shell, X-roll 25 deg/s)"
		WATCH_DIR="${ROOT_DIR}/out/sphere_rotation"
		VSIM_CMD=("${VSEPR}" scene "${SCRIPT_DIR}/sphere.x")
		;;
	"sphere_shaded"|*)
		echo -e "${CYAN}[scene]${NC} sphere_shaded  (NVT + RDF + overlay-cycle, Y-spin 35 deg/s)"
		WATCH_DIR="${ROOT_DIR}/out/sphere_shaded_sim"
		VSIM_CMD=("${VSEPR}" run "${SCRIPT_DIR}/sphere_shaded.vsim")
		;;
esac

mkdir -p "${WATCH_DIR}"

# ---- cleanup trap ------------------------------------------------------------
PIDS=()
cleanup() {
	echo -e "\n${YELLOW}[run_rotation]${NC} shutting down..."
	for pid in "${PIDS[@]}"; do
		kill "${pid}" 2>/dev/null || true
	done
	wait 2>/dev/null || true
	echo -e "${GREEN}[run_rotation]${NC} all processes stopped."
}
trap cleanup EXIT INT TERM

# ---- launch simulation (GL window) ------------------------------------------
if [ ! -x "${VSEPR}" ]; then
	echo -e "${RED}[run_rotation]${NC} vsepr not found at: ${VSEPR}"
	echo "  Build first:  cmake --preset release && ninja -C build"
	echo -e "  ${YELLOW}Continuing with Python viewer only (watch dir: ${WATCH_DIR})${NC}"
else
	echo -e "${CYAN}[sim]${NC}  ${VSIM_CMD[*]}"
	"${VSIM_CMD[@]}" &
	PIDS+=($!)
	echo -e "${GREEN}[sim]${NC}  PID ${PIDS[-1]}  GL window launched"
fi

# ---- brief pause so output dir starts forming --------------------------------
sleep 1

# ---- launch Python live viewer (800x800) ------------------------------------
echo -e "${CYAN}[live]${NC} launching Python viewer  (800x800, !live update mode)"
"${PYTHON}" "${SCRIPT_DIR}/live_viewer.py" \
	--watch "${WATCH_DIR}" \
	--title "${SCENE}" \
	--live &
PIDS+=($!)
echo -e "${GREEN}[live]${NC} PID ${PIDS[-1]}  Python window launched"

echo ""
echo -e "  ${BOLD}Both windows running.${NC}  Ctrl+C to stop."
echo -e "  Watch dir : ${WATCH_DIR}"
echo -e "  Commands  : type  ${CYAN}!new${NC}  or  ${CYAN}!live${NC}  in the Python window cmd bar"
echo ""

wait
