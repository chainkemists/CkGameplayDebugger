# Gate 3 — UV density, material inputs, and surface context

> **Status:** ⏳ Pending
> **Depends on:** Gate 2 ✅

## Goal

After this gate, supported meshes report authoritative UV texels-per-world-unit, the inspector shows resolved material inputs as rendered, and artists can separate texture problems from normal/opacity/shadow/lightmap context without false claims on unsupported data.

## Work items

1. Add capability-gated UV density measurement. A numeric result requires a selected section/slot, authoritative triangle world area, authoritative UV channel area, a provably selected texture dimension, and a supported direct texture-coordinate transform. Formula and units are documented in code/tests; every missing prerequisite names an Unavailable reason.
2. Add Material Inputs page with instance-chain parameter rows, active-variant used-texture rows, explicit provenance, dimensions, sampler/color-space/compression facts, and isolation views where supported. Never claim sampler/slot mapping for a non-parameter union reference.
3. Add Surface & Lighting page with blend/shading/two-sided/normal/opacity/cast-shadow/lightmap facts.
4. Add unsupported/stripped-data states for skeletal, dynamic, or packaged meshes lacking authoritative UV CPU data.

## Expected observations

- Direct UV0 1 m² / 512-pixel fixture reports approximately 512 px/m with tolerance stated by the test; a supported known tiling fixture reports the corresponding scaled value.
- Missing UV channel, stripped CPU data, arbitrary CustomizedUV/material graph, or unprovable texture transform reports Unavailable with the missing prerequisite and no numeric density.
- Material fixtures cover inherited override, local override, hard-coded sample, inactive quality branch, and unsupported representation; only provable current values are labeled resolved.
- Surface page distinguishes normal/shadow/lightmap context but does not claim Lumen/VSM root-cause analysis.

## Exit criteria

- [ ] Focused density/input/surface tests pass.
- [ ] `[EDITOR-VERIFY]` page observations recorded.
- [ ] Gate docs and permanent docs updated.
