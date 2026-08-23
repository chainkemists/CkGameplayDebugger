# Gate 2 — Detection rules, overrides, and Texture Health

> **Status:** ⏳ Pending
> **Depends on:** Gate 1 ✅

## Goal

After this gate, every material slot can explain its selected resolution source, artists can share ordered rules and overrides, and the inspector distinguishes source-quality, cook, fallback, and streaming/residency causes of blur or missing textures.

## Work items

1. Implement active-variant material-input enumeration: active quality/platform parameter values resolve through the instance-parent chain; active-quality/platform `GetUsedTextures` contributes potential non-parameter rows; every row is labeled `Parameter`, `UsedTexture`, or `Unavailable`.
2. Add project-owned rule/override settings plus per-user filters.
3. Add a capability/provenance adapter for runtime health: Cooked from the runtime asset; Resident/Requested only when the concurrent streamer exists, streaming is enabled, the asset is streamable, and state is valid; Authored only under editor-only data. Distinguish ManagerUnavailable, StreamingDisabled, NotStreamable, ResourceNotCreated, and Available. Do not present Requested as an independently measured Wanted value.
4. Add editor-only source/built size, mip/import, normal/data sRGB, and asset navigation facts behind editor gates.
5. Build Health and Rules/Overrides pages with issue filters and focus routing.

## Expected observations

- An injected Available state with cooked 4K, requested 2K, resident 512 is diagnosed as a residency gap; live PIE/package checks assert only the values the active streamer actually reports.
- Cooked/resident 512 of 512 is diagnosed as low source/cooked resolution, not streaming failure.
- Missing/fallback and streaming-disabled states show explicit Unknown/Unavailable fields, never zero-as-fact.
- Reordering rules changes the winning source deterministically; explicit override wins and can return to Auto.
- A High-quality 4K branch and active Low-quality 512 branch select/report the active 512 parameter; inactive union references are never labeled rendered truth.

## Exit criteria

- [ ] Focused rule/health/config tests pass in Editor and Game compile surfaces.
- [ ] Injected tests cover every health availability state and material provenance kind.
- [ ] No editor-only dependency is present in the Game link graph.
- [ ] Gate docs and permanent docs updated.
