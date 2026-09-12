# Runtime Selection

Press `,` in a focused game viewport to toggle the overlay (`ck.DebugOverlay 1` remains the console equivalent). The existing focus/pinned cards remain; the selection HUD adds numbered vector diamonds and a projected dotted aim cone. These visuals use runtime Slate, not marker textures or debug-draw show flags.

| Default | Action |
|---|---|
| `,` | Toggle the overlay |
| `[` / `]` | Nearest previous / next entity in screen-space order |
| Hold `\` | Reveal the selected meaningful root's family |
| `Shift+,` | Open the shared runtime Overlay settings |
| `Shift+P` | Open the same Overlay settings (alternate chord) |

The same selection commands are available as `ck.DebugOverlay.Select`, `.Lock` (toggle), `.Prev`, `.Next`, `.Family` (toggle), and `.Settings`. Selection keys are rebindable while settings' Shift modifier remains fixed; the overlay toggle takes priority if a selection binding uses the same key. No semicolon, legacy double-V, or mouse-wheel selection. Selection shortcuts are owned only while a viewport has keyboard focus; text inputs, console and settings do not select entities. Losing focus cancels unfinished holds.

## Selection policy

Meaningful roots skip Transient and ActorRelay infrastructure. A self-owned context root or the explicit gameplay label `Ck.Debug.Selection.Root` defines an independent family even when lifetime-parented beneath another entity. Otherwise the highest ordinary ancestor represents the family. A visible member can promote an offscreen or transformless parent. Literal roots and all-entity discovery are alternative policies.

Discovery runs continuously inside the distance tuner (default 10,000 Unreal units). Late-spawned entities and entities entering range join automatically; entities leaving range or destroyed are removed. Top-most grouping and occlusion still apply. Cone and view scope do not hide other in-range candidates from discovery or the drawer; only on-screen markers participate in numbered navigation.

Aim selection follows the current camera automatically, combining view alignment and distance. Scope can prefer the view and fall back nearby, require the view, or allow nearby entities. Cone mode limits automatic aim targets to the projected cone, while other discovered candidates remain available for cycling. In weighted mode the cone is a tuning reference. Occlusion uses the Visibility collision channel, not renderer-perfect visibility for non-colliding geometry.

Stable order preserves surviving order and appends newly discovered candidates; this never freezes membership. Navigation is rebuilt around the current selection from projected positions: the nearest screen-left entity is `-1`, the nearest screen-right entity is `+1`, and magnitudes increase by screen-space distance on each side. `[` / `]` use that same order and stop at its edges, avoiding a wrap to a distant entity. There are no permanent ordinals or duplicate names beside diamonds. Score/screen order and stable/live reranking still control discovery presentation, not the best aim target.

Next/Previous temporarily override the current aim winner so overlapping entities remain inspectable; aiming at a different winner resumes automatic following. Hold Select to lock the focused entity across camera movement and range exit; hold again to unlock, or tap Select to resume best aim. Explicit Next/Previous moves the lock. Destroyed handles and world teardown clear it. Double-Shift independently pins a data card. Selected diamonds have a focus halo; selection lock uses a warm accent.

Family reveal ignores world radius/cone/occlusion filters. It includes transformless descendants, anchored at their nearest spatial ancestor when one exists; otherwise they remain available through the drawer shortlist without a world-origin diamond or numbered shortcut. Independent nested roots retain their separate families in Meaningful Roots and All Entities modes; Literal Roots includes those nested boundaries. Reveal is scoped to the root selected when it opens; releasing it hides children and resumes world aim selection unless selection is locked. A locked child's collapsed family uses its root as the world marker and relative-number anchor.

## Developer controls

The shared runtime/ECS Debugger drawer exposes both the overlay's card/world-tag/layout presentation settings and selection hierarchy, member/root anchoring, aim scope, targeting, view bias, radius, cone angle, score/screen order, stable order/live rerank, hold/toggle reveal, drawer shortlist, cone visibility, occlusion, diamond scale, and selection-lock hold duration. World tags settle fully opaque while in range. Crossing the maximum-distance boundary starts a short fixed fade-out; returning within range reverses smoothly from the current opacity and fades back in. There are no configurable fade or opacity bands. Focus Family, Spatial Sweep, and Aim Cone presets are starting points; mix and match remains available. Named presets and JSON clipboard import/export share the same validated runtime policy store; bindings and hold duration persist separately in input settings. Candidate count, range, lock state, and next/previous offsets appear compactly inside the existing focus card. There is no bottom status panel. Compact world cards use feature legends, not field values such as `Lbl:Root`; names remain confined to detailed cards.

Preferences persist in the writable per-user `GameUserSettings` config hierarchy, including packaged non-Shipping builds. Editor settings are an additional entry point, not the runtime dependency. Invalid saved policy is rejected as a whole and uses known defaults with a visible diagnostic. Shipping excludes overlay execution.

## Acceptance

Use the CkTests **Selection** gym and its scenario panel. Test possessed and ejected PIE, then repeat in a build-machine-produced Development or DebugGame package. No local cooking is needed or permitted for this task.

1. Topology: direct Transient children and independent nested roots appear separately; infrastructure does not.
2. Distance/overlap: center/near weighting changes Select predictably; the exact co-located pair has distinct numbers and correct next/previous previews.
3. Mixed hierarchy: hold reveal shows only the selected family, including SceneNode, state-machine, timer, attributes, ordinary transform and transformless children. Release hides children and resumes world aim selection; pinned data remains.
4. Visibility: member mode promotes the offscreen parent; root mode does not. Occlusion toggle changes the wall-hidden candidate. Cone outline and automatic aim-selection boundary agree while changing camera FOV and viewport size; other in-range roots still have visible numbered diamonds outside the cone.
5. Lifecycle: enable the overlay before fixtures spawn, rebuild a scenario, and walk in/out of range without pressing comma. Candidates must update automatically. Moving targets update their spatial offsets while discovery membership remains continuous; destroyed handles cannot be silently adopted as reused slots. A replacement may be selected as a new live aim target. Travel clears stale family/pin/selection state.
6. Input: console/chat/text never trigger selection; key repeat does not cycle; releasing reveal after focus loss does not stick. Wheel remains the game's input. Close the drawer and verify cursor/look/movement state recovers.
7. Persistence: adjust every tuner, save a named preset, export/import it, restart the package and confirm settings/bindings. Test a malformed JSON import; it must report rejection and preserve the previous complete configuration.
8. Package rendering: confirm vector diamonds, selected ring, numbering, cone and existing cards are visible without editor modules or marker textures. Record target/config, build revision, resolution/DPI and a screenshot/video.
9. Selection lock: bind Select to a key other than the overlay toggle. A short press selects on release; holding emits one lock toggle, never a trailing select. Move the camera and leave range; the locked data card stays. Hold again to unlock. Repeat with a revealed child, collapse the family, then destroy the locked entity. Double-Shift card pins remain independent.
10. Presentation: hover/navigate among five candidates and verify screen-left labels are negative, screen-right labels are positive, and magnitudes match actual bracket presses. At each edge, the outward bracket must not wrap to a distant entity. Check co-located badges at small/large diamond scales. Distant cards show feature legends without `Lbl:` fields; no name is repeated beside a diamond.

Automation and build results are tracked in the task's progress log. A green focused test is not a claim of human usability, packaged parity, or repository-wide regression freedom.
