# Derived Properties

A `DerivedProperty` contains a property key, string value, confidence, provenance, provider identity, and diagnostics. Current keys are local geometry, VSEPR label, hybridization coverage, organic family, formal charge, and bond order.

VSEPR and organic-family results are enrichment metadata. They support model selection and operator inspection but do not replace atomistic state or execute a simulation.

Fallback values remain explicit. The default formal-charge value is `assumed neutral`; the default bond-order value is `partial inference`. Both have lower confidence than provider-backed or deterministic geometry results.
