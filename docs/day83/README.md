# Day 83 Script Expansion and Enrichment

Day 83 treats molecular classification as enrichment metadata used to prepare an execution request. It is not an execution endpoint.

The current path is:

1. Build or receive an atomistic state.
2. Request derived properties through the common provider protocol.
3. Record each value with provider, provenance, confidence, and diagnostics.
4. Report unresolved requirements and the runnable state.
5. Keep simulation execution separate from preview and enrichment.

`vsepr classify` and `vsepr expand` use the same enrichment result. The preview does not execute a simulation.

See `script_expansion.md`, `derived_properties.md`, `provider_contract.md`, `provenance_and_confidence.md`, and `migration_notes.md` for the operational contract.
