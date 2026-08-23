# Improvement Acceptance Scripts

These scripts prove MOOSE-inspired VSIM improvements through production CLI
surfaces. MOOSE is a design reference only; no external MOOSE code or data is
used by the runtime.

## Builder Distribution Preview

`builder_distribution_preview.vsim` declares a material and one typed
distribution rule. It is intentionally preview-only: `vsepr expand` must report
what was written, what the internal registry inferred, what could be scheduled
later, and that no distribution execution occurred.

```powershell
build/vsepr.exe validate scripts/improvement_acceptance/builder_distribution_preview.vsim
build/vsepr.exe expand scripts/improvement_acceptance/builder_distribution_preview.vsim
```
