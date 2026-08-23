# Provider Contract

The expansion layer depends on `IPropertyProvider` rather than concrete chemistry classes.

A provider declares whether it supports a `PropertyRequest` and evaluates a supported request for an atomistic `State`. Provider results include their provider identity, provenance, confidence, and diagnostics.

The default provider set contains `VseprGeometryProvider`, `OrganicDescriptorProvider`, and `SafeDefaultProvider`. If multiple registered providers support a request, the expansion layer chooses by stable provider identity so registration order does not affect output.

Providers infer missing state only. They do not parse VSIM syntax, format CLI text, render geometry, or execute the simulation kernel.
