# Script Expansion

Script expansion converts declared or available molecular state into a traceable, execution-ready request. It requests local geometry, VSEPR labeling, hybridization coverage, organic-family metadata, formal charge, and bond-order information.

The resulting status is one of:

- `runnable`: every current request was satisfied.
- `partially runnable`: one or more requests remain unresolved.
- `diagnostic-only`: no valid atomistic state was supplied.

The expansion preview reports inferred values, fallbacks, unresolved requests, and provider identity. It does not run the simulation kernel.
