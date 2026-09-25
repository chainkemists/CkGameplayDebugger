# CkProceduralAnimationDebugger

DeveloperTool UI for CkFoundation's procedural gait, surface motion and rigid rig. Open through the CK Debugger Suite (Systems), an ECS entity's Open In menu, or `ck.ProceduralAnimationDebugger 1`. This is read-only inspection; it does not retune settings, move entities, run contact queries or pause gameplay.

## Data and ownership

`FCkProceduralAnimationDebugger_DataCollector` is the sole runtime reader. Discovery and capture use the public `UCk_Utils_ProceduralAnimation_Debug_UE` API. Each roster row represents one gait entity; a segmented creature can have multiple gait entities. The shared closest-lineage route resolves a picked part to its owning gait feature without treating siblings as equivalent.

The live model owns handle-bearing roster rows. History and preview contain only copied snapshots, without gameplay handles, UObjects or world pointers. A destroyed selection loses its live handle but retains copied history, with an explicit gone status. Selecting another entity clears history. Session invalidation and selected-world teardown clear all rows, selection and history synchronously. `ReleaseSession` is terminal: it unsubscribes observers and makes held model/control callbacks inert. Module teardown releases the window on `OnEnginePreExit`, before registry destruction.

## Sampling and controls

The default ring holds 600 accepted samples (bounded constructor range 1–4096). Duplicate/backward sequences, backward time, mixed entity identities and unaccepted snapshots are rejected without changing the ring. Hold/Scrub pins a copy, so display does not move when recording evicts that sample. Live returns to the latest sample. Recording continues while inspection is held.

Capture follows the common per-window refresh policy. This is SAMPLE history, not a complete recording of every solver frame. Timeline diamonds are actual observations; colored spans join only adjacent observed simulation sequences. Gaps are not reconstructed. Times remain `FCk_Time` in the model and become seconds only at widget boundaries.

The left roster supports filtering and common viewport picking. Selecting a leg in the evidence list or preview focuses that leg's diagnostic selection. Timeline click/drag scrubs to the nearest retained sample. The common preview supplies camera navigation, frame-all/frame-selection and label visibility. Frame rig restores a useful overview without moving gameplay actors.

## Preview contract

The shared `SCkDebug_3dPreviewViewport` owns the preview world; `FCk_DebugScene_Target` owns retained geometry. Cached Common primitive meshes represent a diagnostic body/segments/feet, not replicas of authored gameplay meshes. Root-relative presentation avoids large world coordinates while preserving recorded orientations and relative poses. Actual rig segments are drawn only when the snapshot says rig and gait sequences agree and no transform application is pending; otherwise the preview shows the solver foot/hip line explicitly.

Goal cubes, the final actual probe ray, hit normals and foot-to-target lines are observations copied from runtime. The debugger never reruns traces. The runtime snapshot currently exposes the final probe attempt and its count, not every retry; UI must not imply otherwise.

## Shared surfaces and verification

Uses Common WindowChrome, WorldSelector, EntityHealthList, EvidenceList, EventTimeline, Sparkline, viewport picker, selection sync and preview shell. No local canvas, style registry, input processor or physics world exists. Native layout composes these shared widgets; the feature-specific preview adapter is required to translate procedural pose/contact data into retained geometry.

Pure history and mounted-window/PIE integration tests live in `CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation/Debugger`. Launcher lifecycle/census tests remain in CkDebuggerLauncher. Public model/history APIs support production controls and focused verification. A passing native test verifies wiring; visual layout and actual viewport gestures still require the real editor.
