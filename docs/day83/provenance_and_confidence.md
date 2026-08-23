# Provenance and Confidence

Every inferred property records one provenance value:

- `deterministic_inference`
- `approximate_inference`
- `fallback_default`
- `unavailable`

The model also supports `declared_by_user` and `database_lookup` for future providers.

Confidence describes the quality of the available source for the current inference. It is not experimental certainty. Fallback defaults are deliberately low confidence and carry an explicit diagnostic when no provider data is available.
