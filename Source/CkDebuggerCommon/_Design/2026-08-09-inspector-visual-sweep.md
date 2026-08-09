# Inspector visual sweep (P3 of the Debugger UX campaign)

Design authored 2026-08-09 (Fable session). Brief: "Improve the inspector displays… Transform
can easily be RGB colors, aligned numbers… even better if rotation and scale are VISUALLY shown
with a cube. Go through all the inspectors." Raw material: the eleven richer primitives already
in `CkDebuggerCommon/Widgets/` that no inspector uses today (`SCkDebug_MeterBar`, `_Sparkline`,
`_StatPair`, `_ValuePill`, `_StatusPill`, `_CountBadge`, `_EventTimeline`, `_CategoryDot`,
`_GlowWrap`, `_Chip`, `_Card`).

## Wave 1 — vocabulary (two parallel units)

### U1 — builder extension + value filter (owns `CkInspectorWidgetBuilder.h/.cpp` + the filter)

Extend `FCkInspectorWidgetBuilder` with rows wrapping the primitives. U1 reads each primitive's
actual API first and shapes the row methods to fit the builder's existing idiom (AddRow /
AddConditionalRow / TAttribute values / no structural rebuild from Tick). Target vocabulary —
names indicative, U1 finalizes signatures EXCEPT the one frozen below:

- `AddMeterRow` (label + 0..1 fraction attr or value/max attrs + tone + optional value text) —
  bounded quantities: timers, attribute current/max, playback position.
- `AddSparklineRow` (label + samples provider) — velocities, rates. Note the builder's rows are
  built once and value-driven — the sample HISTORY has to live somewhere; U1 decides the
  ring-buffer home (row-owned state via a shared ptr captured in the attribute is acceptable)
  and documents it.
- `AddStatusPillRow` (label + text attr + tone attr) — NetRole, SM state, enable/disable.
- `AddChipsRow` (label + array-of-chips provider) — tag lists, feature lists (via
  `ck::debug_axes::Make_Chip`, ChipStyle axis).
- `AddTimelineRow` (label + events provider) — SM transition history via `SCkDebug_EventTimeline`.
- `AddCategoryDotRow` if `SCkDebug_CategoryDot` fits a real consumer; skip if speculative.
- **FROZEN (U2 codes against this today):**
  ```cpp
  // One aligned monospace numeric row. Components render right-aligned in equal fixed-width
  // columns (MonoFont); color by component index 0/1/2 = the X/Y/Z axis colors (R/G/B
  // families from CkStyle roles), further components neutral. Honors the ValueAlignment axis
  // (AlignedColumns = this row's reason to exist; Left/Right degrade gracefully).
  auto AddAlignedNumericRow(
      const FText& InLabel,
      const TArray<TAttribute<FText>>& InComponents) -> FCkInspectorWidgetBuilder&;
  ```
- Filter upgrade: `CkInspectorWidgetBuilder.cpp:190` matches labels only — extend to match
  VALUES too (the current value of TAttribute-driven rows at filter time; document staleness
  semantics). This is user ask #9's "inspector value search".

New axis colors: X/Y/Z axis roles do not exist in `CkStyleSettings` yet — U1 adds
`AxisX/AxisY/AxisZ` palette roles (CkFoundation `CkStyleSettings.h` + `CkStyle.h/.cpp`
accessors; defaults = UE gizmo-familiar red/green/blue tuned to the palette) — the ONE
CkFoundation touch in wave 1.

### U2 — Transform flagship (owns `Widgets/SCkDebug_OrientationCube.h/.cpp` [new, CkDebuggerCommon] + `CkInspector_Transform.cpp`)

- `SCkDebug_OrientationCube` — `SLeafWidget::OnPaint`: project a unit cube through a
  rotation attr, edges scaled per-axis by a Scale3D attr, back edges thinner/muted, front
  edges weighted, axis edges colored with the same X/Y/Z roles (until U1's roles land, code
  against `CkStyle::AxisX()` etc. as frozen API). Fixed-size leaf widget (~64px), live via
  attributes, no per-paint allocation (reuse layout-space arrays).
- Rewire `CkInspector_Transform.cpp` (:32-57 today: three concatenated strings): Location /
  Rotation / Scale become `AddAlignedNumericRow`s; the cube sits beside/below them
  (`AddWidgetRow`). Keep the PMG in-world gizmo triad behavior untouched.

## Wave 2 — the sweep (five parallel units after wave 1 gates; each owns ONLY its listed inspector .cpp files)

| Cluster | Inspectors | Primary upgrades |
|---|---|---|
| A — attributes/meters | ByteAttributes, FloatAttributes, IntegerAttributes, Timer, Objective, ObjectiveOwner, Resolver | bounded values → meters; counts → badges |
| B — state/identity | Network, StateMachine, EntityInfo, Variables, DynamicFragments, TagSet, EntityTag, EntityTagQuery | NetRole/SM state → status pills; SM history → timeline; tags → chips |
| C — spatial/physics | Physics, Jolt, OverlapBody, Shapes, SceneNode, Probes, ProbeTraces, AStar, PathNetwork, PathNetworkFollower | velocities → sparklines; vectors → aligned numeric rows |
| D — rendering/av | IsmProxy, IskmProxy, IskmRenderer, Vfx, Audio, Camera, FogOfWar, MontagePlayer, AnimPlans, Tween | playback → meters; states → pills; positions → aligned rows |
| E — gameplay/ui | Inventories, Compass, Minimap, Poi, InteractTarget, InteractionResolver, EntityCollections, Relationships, Aggro, UI, ActorRelay | Inventories' private 20-color palette → roles; aggro scores → meters; refs stay EntityRef pills |

Per-cluster standing orders:
- Use the wave-1 vocabulary; do NOT invent new builder methods (report gaps instead).
- Upgrade only where the richer widget genuinely communicates better — a plain AddRow that
  reads fine STAYS. State per inspector: upgraded / left / why.
- Respect: no structural rebuild from Tick; values via TAttribute; `RequestRebuild()` only on
  structural change; `OnDeactivated` releases per-entity state.
- Straggler migration (opportunistic, only in files you own): `Get_FeatureColor()` hex
  literals → nearest `CkStyle::` role or a documented derivation; ambiguous → leave + report.
- GOAP gateway inspector (registered from CkGoapDebugger) is OUT of scope.

## Gates

Wave 1: toolbox `--build --test` pattern `Debug` (+ new specs U1 writes for the aligned-row
metric math and value-filter matching — pure parts only). Wave 2: same gate; visual outcomes
are `[EDITOR-VERIFY]` per cluster (one line per upgraded inspector).
