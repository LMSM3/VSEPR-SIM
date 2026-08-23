# V5 Known Failures

## Build
- `cmake --preset release` fails during GCC C++ module dependency scanning
- Existing build\vsepr.exe still reports v5.15.0

## Runtime (expected progress signals, not blockers)
- Synthetic path reports many validity warnings and partial convergence
- Standalone `vsepr-view.exe` absent; visual fallback uses Chrome/PDF

## Retired / removed
- Standalone Qt/ImGui visualizer as execution owner
- Legacy shell/PowerShell test orchestration and reporting scripts
