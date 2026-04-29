#!/usr/bin/env python3
"""
run_sims.py - Continuous randomly initialised simulation launcher.
Spawns `vsepr viz` processes with randomised species, temperatures,
and atom counts. Runs via WSL (Linux binary, no Device Guard restriction).
Scales to available CPU cores. Press Ctrl+C to stop.

Usage:
    python scripts/run_sims.py
    python scripts/run_sims.py --workers 4
    python scripts/run_sims.py --distro AlmaLinux-10 --workers 2
"""
import subprocess, random, time, argparse, os, sys

SPECIES  = ["Ar", "N2", "CO2", "H2", "O2", "Ne", "Xe", "Kr", "CH4", "H2O"]
T_RANGES = [(77, 200), (200, 400), (300, 600), (400, 1000), (500, 1500)]
N_ATOMS  = [32, 64, 125]

# Linux binary path inside WSL (built by deploy/build_wsl.sh)
WSL_BINARY = "/root/vsper-sim/build-linux/vsepr"

def random_sim_args():
    sp    = random.choice(SPECIES)
    t0,t1 = random.choice(T_RANGES)
    T_s   = round(random.uniform(t0, t1), 1)
    T_e   = round(T_s + random.uniform(50, 300), 1)
    N     = random.choice(N_ATOMS)
    return sp, ["viz", sp,
                "--T-start", str(T_s), "--T-end", str(T_e),
                "-N", str(N), "--frames", "0",
                "--atomic-fps", "10", "--analysis-fps", "1"]

def main():
    ap = argparse.ArgumentParser(description="Continuous random sim launcher (WSL)")
    ap.add_argument("--workers",       type=int,   default=2)
    ap.add_argument("--distro",        type=str,   default="AlmaLinux-10")
    ap.add_argument("--restart-delay", type=float, default=3.0)
    args = ap.parse_args()

    print(f"\033[1;36m  VSEPR-SIM continuous sim launcher (WSL: {args.distro})\033[0m")
    print(f"  Workers: {args.workers}  |  Ctrl+C to stop\n")

    procs = {}

    def launch(slot):
        sp, viz_args = random_sim_args()
        cmd = ["wsl", "-d", args.distro, "--", WSL_BINARY] + viz_args
        T_s = viz_args[viz_args.index("--T-start") + 1]
        print(f"  [slot {slot}] {sp}  T-start={T_s} K")
        return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    for i in range(args.workers):
        procs[i] = launch(i)
        time.sleep(0.4)

    try:
        while True:
            for i in range(args.workers):
                if procs[i].poll() is not None:
                    print(f"  [slot {i}] finished — restarting in {args.restart_delay}s...")
                    time.sleep(args.restart_delay)
                    procs[i] = launch(i)
            time.sleep(1.0)
    except KeyboardInterrupt:
        print("\n  Stopping all workers...")
        for p in procs.values():
            p.terminate()
        print("  Done.")

if __name__ == "__main__":
    main()
    main()
