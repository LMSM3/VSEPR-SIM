# WO-66Q — Non-Molecular Objects + XBIT Documentation
<!-- v5.1.4 | branch: v5.0.0-main -->

## Summary

Defines the typed non-molecular constructor objects (geometry, surface, source,
sink, ambient) and introduces the XBIT (Extended Binary Identity Tag) — a
deterministic 256-bit identity tag that can be attached to any simulation entity.

## Non-Molecular Object Types

| Type | Constructor | Path example |
|---|---|---|
| GeometryObject | PipeGeometry, BoxGeometry, SphereGeometry, GenericGeometry | system.geometry.pipe |
| SurfaceObject | WallSurface | system.surface.wall |
| SourceObject | InletSource | system.source.inlet |
| SinkObject | OutletSink | system.sink.outlet |
| AmbientObject | AmbientEnv | environment.carrier |

These objects are consumed by DEMBridge (WO-67N) and FEABridge (WO-67O) via
ObjectPath references.  The SurfaceObject frozen field contract is the shared
bridge data interface.

## XBIT — Extended Binary Identity Tag

XBIT is a 32-byte deterministic identity tag encoding:

`
Bits 255-224  tier_tag     scale tier (atomistic / object / bridge / ...)
Bits 223-192  kind_tag     ConstructorObjectKind or particle Z
Bits 191-128  lineage_id   64-bit parent/formation lineage hash
Bits 127-64   instance_id  64-bit deterministic instance hash
Bits  63-32   batch_tag    batch index + batch base hash
Bits  31-0    checksum     CRC-32 of preceding 28 bytes
`

### In .vsim

`sim
system.surface.wall = WallSurface(xbit = auto)
`

xbit = auto computes the tag from the ObjectPath + constructor args.  
xbit = <hex64> allows a user-supplied 64-char hex override (testing/replay).

### Serialisation

- Binary: 32-byte little-endian blob
- Hex:    XBIT:<64-char lowercase hex>

## Implementation

| File | Change |
|---|---|
| include/vsim/objects/non_molecular_objects.hpp | GeometryObject, SurfaceObject, SourceObject, SinkObject, AmbientObject, NonMolecularObjectStore |
| include/vsim/objects/bridge_objects.hpp | DEMBridgeObject, FEABridgeObject, BridgeObjectStore |
| include/vsim/xbit/xbit.hpp | XbitTier, Xbit, layout constants, inline accessors |
| include/vsim/vsim_document.hpp | 
m_objects, ridge_objects |
| src/vsim/vsim_parser.cpp | pply_nm_*_key appliers |

## Acceptance criteria

- [x] All five non-molecular object types defined with frozen bridge-contract fields
- [x] NonMolecularObjectStore and BridgeObjectStore added to VsimDocument
- [x] Parser appliers route [objects.geometry], [objects.surface], etc.
- [x] Xbit struct layout matches 256-bit spec; inline accessors verified
- [x] Build clean, no errors

## Diagnostic codes

VSIM-E090 — geometry type unknown  
VSIM-E091 — surface references unresolved geometry  
VSIM-E092 — required bridge field missing on SurfaceObject  
VSIM-W090 — geometry declared but not referenced by any surface or bridge  
VSIM-W091 — XBIT checksum mismatch on load  