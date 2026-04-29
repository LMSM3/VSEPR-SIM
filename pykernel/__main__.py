"""
PyKernel — run as: python -m pykernel [command]

Commands:
    run       Continuous simulation runner (Phase A)
    improve   Improvement loop (Phase C)
    scan      Scan flagged files only
    info      Show bridge status
"""

import sys


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help", "help"):
        print(__doc__)
        print("Usage: python -m pykernel <command> [options]")
        print()
        print("Commands:")
        print("  run       Continuous runner (Phase A walk-away)")
        print("  improve   Improvement loop (Phase C closed loop)")
        print("  scan      Scan flagged files only")
        print("  info      Show bridge and GPU info")
        return

    command = sys.argv[1]
    sys.argv = [sys.argv[0]] + sys.argv[2:]  # Strip command from argv

    if command == "run":
        from pykernel.runner import main as runner_main
        runner_main()

    elif command == "improve":
        from pykernel.improvement_loop import main as improve_main
        improve_main()

    elif command == "scan":
        from pykernel.improvement_loop import ImprovementLoop
        loop = ImprovementLoop()
        flagged = loop.scan_flagged_files()
        cmake = loop.scan_cmake_disabled()
        all_f = flagged + cmake
        print(f"Found {len(all_f)} flagged files:\n")
        for f in all_f[:30]:
            print(f"  [{f.priority:3d}] {f.path}")
            print(f"        Flags: {', '.join(f.flags)} ({f.flag_count} instances)")
        if len(all_f) > 30:
            print(f"\n  ... and {len(all_f) - 30} more")
        print(f"\nTotal: {len(all_f)} files, "
              f"{sum(f.flag_count for f in all_f)} flag instances")

    elif command == "info":
        from pykernel.gpu_bridge import GPUBridge
        bridge = GPUBridge()
        info = bridge.info()
        print("PyKernel Bridge Status")
        print("=" * 40)
        for k, v in info.items():
            print(f"  {k}: {v}")

    else:
        print(f"Unknown command: {command}")
        print("Run 'python -m pykernel --help' for usage")
        sys.exit(1)


if __name__ == "__main__":
    main()
