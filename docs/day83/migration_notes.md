# Migration Notes

The former direct CLI classifier orchestration has moved to `atomistic/classify/script_enrichment.*`. The CLI now routes commands and renders the shared result.

Existing VSEPR, organic-candidate, callback-provider, generic script-preview, and batch-expansion APIs remain available. Batch factorial expansion is separate from scientific property expansion and was not changed.

No new VSIM language keywords were added. Full aromaticity, ring perception, lipid refinement, reaction kinetics, and viewer behavior remain deferred to their owning subsystems.
