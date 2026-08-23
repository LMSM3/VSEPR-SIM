# WO-66N — Constructor Objects + Batching + Upper-Block References
<!-- v5.1.4 | branch: v5.0.0-main -->

## Summary

Adds a functional constructor expression syntax to [objects] sections,
enabling declarative typed object instances with structured arguments,
cross-object references (upper-block), and batch declarations.

## Motivation

Bridge WOs 67N and 67O require named, typed, path-addressable objects as
their inputs.  Without a first-class object model, bridge configurations
must use ad hoc string keys, losing type safety and referential integrity.

## Syntax

`sim
[objects]
system.geometry.pipe   = PipeGeometry(radius = 0.05, length = 2.0, material = steel)
system.surface.wall    = WallSurface(geometry = system.geometry.pipe, pressure_ref_Pa = 101325)
system.source.inlet    = InletSource(geometry = system.geometry.pipe, flux_rate = 1000)
system.sink.outlet     = OutletSink(geometry = system.geometry.pipe)
environment.carrier    = AmbientEnv(fluid_species = water, temperature_K = 293.15)
dem.pipe_packing       = DEMBridge(from = system.surface.wall, geometry = system.geometry.pipe)
fea.pipe_wall          = FEABridge(from = system.surface.wall, target = system.geometry.pipe)
system.crystal         = CrystalModule(lattice = fcc, a = 3.52, species = [Ni], supercell = [4,4,4])
`

### Batch form

`sim
[objects.batch]
base        = system.surface.wall
count       = 3
constructor = WallSurface
pressure    = [101325, 105000, 98000]
`

Expands to system.surface.wall[0], system.surface.wall[1], system.surface.wall[2].

### Upper-block references

Any constructor argument whose value matches an existing object path
(dotted, no quotes) is treated as an ObjectPathRef and validated against
the registry.  Circular references are rejected at validate().

## Implementation

| File | Change |
|---|---|
| include/vsim/objects/object_path.hpp | ObjectPath, ObjectPathRef |
| include/vsim/objects/constructor_object.hpp | ConstructorObject, ConstructorObjectRegistry, BatchGroup |
| include/vsim/vsim_document.hpp | VsimDocument::objects |
| src/vsim/vsim_parser.cpp | pply_objects_constructor_line, pply_objects_batch_key |

## Acceptance criteria

- [x] ConstructorObjectRegistry stores all [objects] declarations keyed by path
- [x] BatchGroup::expand() synthesises indexed instances
- [x] Cross-object references stored as ObjectPathRef in ConstructorObject::refs
- [x] Parser dispatches [objects] and [objects.batch]
- [x] Build clean

## Diagnostic codes

VSIM-E060 — unknown constructor kind  
VSIM-E061 — unresolved object reference  
VSIM-E062 — circular reference detected  
VSIM-W060 — constructor object declared but never consumed  