# CkJoltBakeInspector

Editor-only launcher tool for read-only inspection of every mesh under Jolt's configured baked roots.

- `Analyze All` is the only full-inventory load path. It processes one row per Slate tick after explicit user input. The queue holds one `FCk_Jolt_ScopedGlobalInit` lease only while active; cancel, refresh, window destruction, and completion release it.
- Inventory collection is AssetRegistry-only. Row audits call `ck::jolt::cook::Analyze_MeshShape`; do not duplicate cooker freshness, winding, or failure logic in this module.
- The preview renders audit value triangles through `FCk_DebugScene_Target`: source is red on the left, normalized candidate cyan in the center, and available current cooked shape green on the right. It labels capped or unavailable shared previews. Tear it down before its preview world; the module's `OnEnginePreExit` reset is required.
- Baking is explicit and routes exclusively through `UCk_JoltCook_EditorSubsystem_UE`. Bulk baking admits only audited, non-failing `CookMissing`, `RebuildStale`, `RebuildCorrupt`, and `RebuildInsideOut` rows. Never submit `FixSource`, `DeleteOrphan`, or current rows.
- Never rebuild the list from Tick. All-mode rows use attributes; filtered modes refresh only when a classification changes membership.
