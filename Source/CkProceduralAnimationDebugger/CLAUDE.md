# CkProceduralAnimationDebugger

DeveloperTool UI for CkFoundation's procedural gait, surface motion and rigid rig. Open through the CK Debugger Suite (Systems), an ECS entity's Open In menu, or `ck.ProceduralAnimationDebugger 1`. This is read-only inspection; it does not retune settings, move entities, run contact queries or pause gameplay.

## Data and ownership

`FCkProceduralAnimationDebugger_DataCollector` is the sole runtime reader. Discovery uses `UCk_Utils_ProceduralAnimation_Debug_UE::Get_Entities`; each roster row carries a summary (identity, gait ready/failed, rig ready/failed, leg, enabled and planted counts) read through the public gait, leg and rig utils. Only the selected entity is copied as a full snapshot. Each roster row represents one gait entity; a segmented creature can have multiple gait entities. The shared closest-lineage route resolves a picked leg or part to its owning gait body without treating siblings as equivalent. Picking a leg entity selects its body and focuses that leg; switching to another body clears the focused leg.

The live model owns handle-bearing roster rows. History and preview contain only copied snapshots, without gameplay handles, UObjects or world pointers. A destroyed selection loses its live handle but retains copied history, with an explicit gone status. Selecting another entity clears history. The model notifies the window only when the roster, the recorded history or the gone status changes. Session invalidation and selected-world teardown clear all rows, selection and history synchronously. `ReleaseSession` is terminal: it unsubscribes observers and makes held model/control callbacks inert. Module teardown releases the window on `OnEnginePreExit`, before registry destruction.

## Sampling and controls

The default ring holds 600 accepted samples (bounded constructor range 1–4096). Duplicate/backward sequences, backward time, mixed entity identities and unaccepted snapshots are rejected without changing the ring. Hold/Scrub pins a copy, so display does not move when recording evicts that sample. Live returns to the latest sample. Recording continues while inspection is held.

Capture follows the common per-window refresh policy. This is SAMPLE history, not a complete recording of every solver frame. Timeline diamonds are actual observations; colored spans join only adjacent observed simulation sequences. Gaps are not reconstructed. The window appends newly recorded samples to its timeline and sparklines and drops evicted ones, rather than rebuilding them every refresh; a marker selects its sample by solve sequence. Pan, zoom and follow-live survive a change of the lane set. Times remain `FCk_Time` in the model and become seconds only at widget boundaries.

The left roster supports filtering and common viewport picking. Selecting a leg in the evidence list or preview focuses that leg's diagnostic selection. The leg list shows each leg as Enabled or Disabled with its own rig status; a detached leg disappears from the list and preview, while the snapshot keeps its index-stable entry with an empty leg entity id so timeline lanes do not shift. Timeline click/drag scrubs to the nearest retained sample. The common preview supplies camera navigation, frame-all/frame-selection and label visibility. Frame rig restores a useful overview without moving gameplay actors.

## Preview contract

The shared `SCkDebug_3dPreviewViewport` owns the preview world; `FCk_DebugScene_Target` owns retained geometry. Cached Common primitive meshes represent a diagnostic body/segments/feet, not replicas of authored gameplay meshes. Root-relative presentation avoids large world coordinates while preserving recorded orientations and relative poses. Actual rig segments are drawn from each recorded segment transform, one box per segment plus the foot part, only when the snapshot says rig and gait sequences agree and no transform application is pending; otherwise the preview shows the solver foot/hip line explicitly. Disabled legs are drawn muted.

Goal cubes, the final actual probe ray, hit normals and foot-to-target lines are observations copied from runtime. The debugger never reruns traces. The runtime snapshot currently exposes the final probe attempt and its count, not every retry; UI must not imply otherwise.

## Shared surfaces and verification

Uses Common WindowChrome, WorldSelector, EntityHealthList, EvidenceList, EventTimeline, Sparkline, viewport picker, selection sync and preview shell. No local canvas, style registry, input processor or physics world exists. Native layout composes these shared widgets; the feature-specific preview adapter is required to translate procedural pose/contact data into retained geometry.

Pure history and mounted-window/PIE integration tests live in `CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation/Debugger`. Launcher lifecycle/census tests remain in CkDebuggerLauncher. Public model/history APIs support production controls and focused verification. A passing native test verifies wiring; visual layout and actual viewport gestures still require the real editor.
