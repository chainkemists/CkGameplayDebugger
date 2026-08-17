# CLAUDE.md — CkGameplayDebugger (repo) / CkDebugger (plugin)

**Framework rules: see [CkFoundation/CLAUDE.md](../CkFoundation/CLAUDE.md)** — the doctrine of
record for style, macros, ECS patterns, and collaboration protocol; nothing from it is restated
here. Full extension runbooks live in the `ck-gameplaydebugger-extension` skill (this plugin's
`.claude/skills/`, authored in Phase 2 of the doc campaign).

## Identity (verified 2026-07-14)

- **Naming surprise, up front:** the repo/folder is `CkGameplayDebugger`, but the plugin it ships
  is **`CkDebugger.uplugin`** (FriendlyName "Ck Gameplay Debugger"). One *module* inside also
  carries the repo name — that module is the legacy generation, not the plugin.
- **26-module debugger suite** for the CkFoundation ECS: 4 Runtime (`CkGameplayDebugger`,
  `CkDebuggerCommon`, `CkEntityDebugOverlay`, `CkInputHudOverlay`) + 21 DeveloperTool (`CkEcsDebugger`, `CkSmDebugger`,
  `CkSchedulerDebugger`, `CkGoapDebugger`, `CkDialogDebugger`,
  `CkAggroDebugger`, `CkUIDebugger`, `CkAStarDebugger`, `CkCrowdDebugger`, `CkEqsDebugger`, `CkInputDebugger`,
  `CkObjectPoolingDebugger`, `CkJoltDebugger`, `CkMapDebugger`, `CkInsightsDebugger`,
  `CkAudioDebugger`, `CkStyleLabDebugger`, `CkSaveDebugger`, `CkOptimizationDebugger`, `CkIntentDebugger`, `CkDebuggerLauncher`) + 1 Editor companion
  (`CkSaveDebuggerEditor`). Plugin dependency: **CkFoundation only**. The Runtime type on overlay + common is load-bearing — they
  declare native gameplay tags (commit `a4de221`).
- **Packaged-module contract (2026-08-11):** all 21 DeveloperTool modules are included in
  Development/DebugGame and excluded from Test/Shipping. ECS, State Machine, Scheduler, and GOAP
  use the same runtime-Slate graph canvases in editor and packaged targets; native GraphEditor
  adapters remain editor-only and must never become a Game-target dependency.
- **No AngelScript surface anywhere in this plugin** — no `Script/` dir, zero `.as` files
  (verified via `rg --no-ignore`). **Blueprint surface exists only on the legacy module**
  (`Abstract, Blueprintable, EditInlineNew` filter/submenu/action classes, e.g.
  `Source/CkGameplayDebugger/Public/CkGameplayDebugger/Submenu/CkDebugger_Submenu.h:18`); the
  Slate and overlay modules are C++-only.

## Three generations — and which to extend

| Gen | Module(s) | Type | What it is | Status |
|---|---|---|---|---|
| 1 (2023) | `CkGameplayDebugger` | Runtime | Single UE-GameplayDebugger category: DebugProfile data assets, Blueprint Filters/Submenus/Actions, canvas draw. Compiles out of Shipping via self-defined `WITH_GAMEPLAY_DEBUGGER` (`CkGameplayDebugger.Build.cs:33-39`). | Frozen since early 2024 — **maintenance-only; do not add new features here** (deprecation not proclaimed; maintainer's call). |
| 2 (2025→) | 20 packaged feature debugger modules + `CkDebuggerLauncher` | 21 DeveloperTool | Slate debugger tabs on the shared `CkDebuggerCommon` widget base, plus the discovery/launch rail. Editor targets dock them under Tools > Debug; packaged Development/DebugGame targets open DeveloperTool windows as floating Slate windows. Test/Shipping exclude DeveloperTool modules. | **Extend when you need a standalone analysis tool. Preserve packaged support.** |
| 3 (2026, current flagship) | `CkEntityDebugOverlay` | Runtime | In-game on-screen overlay: `ULocalPlayerSubsystem` (`Subsystem/CkDebugOverlay_Subsystem.h:48`), self-registering providers, focus card + world pills + diamond markers. Compiled under `WITH_CK_DEBUG_OVERLAY`, non-Shipping only (`CkEntityDebugOverlay.Build.cs:36-39`); driven by `ck.DebugOverlay*` cvars/commands (`Subsystem/CkDebugOverlay_Subsystem.cpp:159-213`). | **Extend when you need in-game/on-screen debug info.** |

The 2024-2025 Cog-based EcsDebugger era is dead — removed from `Source/`; only stale traces
remain (see Open issues).

## Extension entry points

- **Overlay provider** (most common ask): implement `ICk_DebugOverlay_Provider`
  (`Source/CkEntityDebugOverlay/Public/CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h:30`)
  and put `CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_<Feature>)` at the bottom
  of the .cpp (macro: `Provider/CkDebugOverlay_Registry.h:66`; exemplar:
  `Private/Providers/CkDebugOverlay_Provider_Aggro.cpp:136`). 19 providers registered as of
  2026-07-02; 7 automation specs in `Private/Tests/*.spec.cpp`. Add the feature's CkFoundation
  module to `CkEntityDebugOverlay.Build.cs`.
- **ECS-debugger inspector**: implement `ICkDebuggerComponentInspector_Base`
  (`Source/CkEcsDebugger/Public/CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h:8`), register
  with `CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_<Feature>)`
  (macro: `Inspectors/CkDebuggerInspectorRegistry.h:58`; exemplar: `CkInspector_Timer.cpp:15`).
  40 registration call sites exist as exemplars.
- **New Slate debugger module**: follow the 7-step checklist in
  [Source/CkDebuggerCommon/CLAUDE.md](Source/CkDebuggerCommon/CLAUDE.md) ("Creating a new debugger
  module — checklist", lines 455-483). That file is the canonical authoring doc for the shared
  widgets, copy/selectable-text policy, list-row click-trap and pointer-identity contracts, search
  bars, PMG overlays, and safety rules — read it first; it is deliberately not duplicated here.
- **Debugger Launcher entry**: every standalone debugger tab registers an
  `FCkDebuggerToolDescriptor` after its tab spawner and unregisters it before the spawner. The exact
  census is enforced by `Source/CkDebuggerLauncher/Private/Tests/CkDebuggerLauncherCatalog.spec.cpp`;
  permanent authoring steps live in `Source/CkDebuggerLauncher/CLAUDE.md`.
- **Standalone window chrome**: every plugin-owned catalog window wraps its specialized root in
  `SCkDebug_WindowChrome`, which provides single-line command lanes, the trailing status/refresh
  cluster, and the Tools menu. The dock tab is the sole debugger identity surface; never repeat its
  title or icon inside the command bar. At constrained widths, command lanes scroll horizontally
  rather than wrapping controls. This includes `CkInsightsDebugger`; CkFoundation retains only its
  UI-free trace-analysis dependency.
- **Entity-aware debugger entry**: a debugger that can select an ECS entity registers an
  `FCkDebug_EntityTargetRoute` and resolves exact/ancestor/descendant handles through the common
  closest-lineage helper. This powers both ECS inspector `Open In` links and the common
  `Sync from ECS <id>` status action; do not register a route that only opens a tab.
- **Viewport picker**: click-to-select in the game viewport is a shared facility
  (`CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h` + `SCkDebug_ViewportPickerControls`), not an
  ECS-debugger exclusive. The ECS debugger runs it unfiltered; every other entity-capable debugger
  runs it with a `TargetFilter` so only its supported entities (plus their owner chain up to the
  top-most non-transient, non-ActorRelay ancestor) are previewed and pickable. Authoring rules:
  [Source/CkDebuggerCommon/CLAUDE.md](Source/CkDebuggerCommon/CLAUDE.md) §"Common window chrome
  and entity targeting".
- **Legacy submenu/filter/action**: don't — see the Gen-1 status above.

## Plugin-specific rules

- **`CkEntityDebugOverlay` sets `bUseUnity = false` deliberately** (`Build.cs:11`): each provider
  .cpp defines identically-named file-local tag helpers in anonymous namespaces, which collide
  when merged into a unity TU (in-code rationale at `Build.cs:7-10`). Consequence: that
  anonymous-namespace pattern is safe **in this module only** — everywhere else the root
  doctrine's named-namespace rule applies.
- **Lifecycle contracts for Slate debuggers** (crash-grade; both have bitten):
  - Clear every cached `FCk_Handle` on `FEditorDelegates::EndPIE` — handles hold the registry by
    value and crash on destruct once the PIE registry dies. Contract, audit list, and crash
    signature: [Source/CkSmDebugger/CLAUDE.md](Source/CkSmDebugger/CLAUDE.md).
  - Tear down handle-holding Slate on `FCoreDelegates::OnEnginePreExit`, NOT in `ShutdownModule`
    (too late — the registry's shared state is already freed). Canonical:
    `Source/CkEcsDebugger/CkEcsDebugger_Module.cpp:60-66,109-121`.
- **Settings split** (commit `d607751`, packaged persistence update 2026-08-11): per-user input
  keybinds live in `UCk_DebugOverlay_InputSettings` (`Config=GameUserSettings`, still presented
  under Editor Preferences → Ck through `GetContainerName()` —
  `Settings/CkDebugOverlay_Settings.h`); project visuals/layouts live in
  `UCk_DebugOverlay_Settings` (`Config=Game, DefaultConfig`, Project Settings → Ck — same file
  `:11-19`). The runtime store is per-user and available in packaged builds; editor startup imports
  the old `EditorPerProjectUserSettings` section once when no runtime section exists. New gesture
  keybinds go in the Input class, never the project class. The same storage/migration contract
  covers common debugger style/window preferences, ECS filters, Crowd viewport preferences, and
  EQS view toggles so Editor and packaged tools persist the same categories of user choice.
- **DeveloperTool debuggers are not runtime dependencies**: game modules and the three Runtime
  modules here must never depend on them. Editor-only workspace-menu and `UnrealEd` dependencies
  stay behind `Target.bBuildEditor` / `WITH_EDITOR`; packaged windows use runtime Slate's global
  tab manager.
- **Packaged QA tools use `DeveloperTool`, not `Runtime`**: this includes them when
  `bBuildDeveloperTools` is enabled (editor and packaged Development/DebugGame) while excluding
  Test/Shipping. Their editor-only workspace-menu and `UnrealEd` dependencies must remain behind
  `Target.bBuildEditor` / `WITH_EDITOR`; packaged windows use runtime Slate's global tab manager.
- **Overlay focus-card capacity**: row budgeting happens in the presentation model before Slate.
  Respect total/per-section limits, protected AI/navigation provider ordering, and explicit
  omission summaries; do not solve capacity by increasing the hard clip or hiding overflow.

## Known open issues (campaign record: `CkFoundation/.claude/reports/`)

- Repo-root `README.md:11` is stale — it claims a Cog plugin dependency; Cog is gone (zero real
  references left in `Source/`; `Config/DefaultCkDebugger.ini:4-7` still redirects dead Cog-era
  classes, and `Content/CkEcsDebugger_WindowManager.uasset` is orphaned).
- `[PACKAGED-VERIFY]` The overlay module and its per-user `GameUserSettings` gestures now compile
  into packaged non-Shipping builds, but the complete double-tap gesture and persistence workflow
  still requires live packaged acceptance.

## Provenance and maintenance

Facts above verified against code on **2026-07-14** (launcher branch based on `7fd41c8`). Re-verify with:

- Module count/types: `rg -c '"Name"' CkDebugger.uplugin` (expect **27** = 26 modules + the
  CkFoundation entry in the `Plugins` dependency array), or count modules only:
  `(Get-Content CkDebugger.uplugin -Raw | ConvertFrom-Json).Modules.Count` (expect 26); read the
  `"Type"` fields.
- Registration macros: `rg -n 'define CK_REGISTER_DEBUG_OVERLAY_PROVIDER' Source/CkEntityDebugOverlay` · `rg -n 'define CK_REGISTER_DEBUGGER_INSPECTOR' Source/CkEcsDebugger`.
- Settings Config attributes: `rg -n 'UCLASS\(Config' Source/CkEntityDebugOverlay/Public/CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h`.
- No-AS claim: `rg --no-ignore --files -g '*.as' .` (expect zero matches).
