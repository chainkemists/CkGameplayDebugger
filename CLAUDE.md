# CLAUDE.md — CkGameplayDebugger (repo) / CkDebugger (plugin)

**Framework rules: see [CkFoundation/CLAUDE.md](../CkFoundation/CLAUDE.md)** — the doctrine of
record for style, macros, ECS patterns, and collaboration protocol; nothing from it is restated
here. Full extension runbooks live in the `ck-gameplaydebugger-extension` skill (this plugin's
`.claude/skills/`, authored in Phase 2 of the doc campaign).

## Identity (verified 2026-07-14)

- **Naming surprise, up front:** the repo/folder is `CkGameplayDebugger`, but the plugin it ships
  is **`CkDebugger.uplugin`** (FriendlyName "Ck Gameplay Debugger"). One *module* inside also
  carries the repo name — that module is the legacy generation, not the plugin.
- **14-module debugger suite** for the CkFoundation ECS: 3 Runtime (`CkGameplayDebugger`,
  `CkDebuggerCommon`, `CkEntityDebugOverlay`) + 11 UncookedOnly (`CkEcsDebugger`, `CkSmDebugger`,
  `CkUIDebugger`, `CkSchedulerDebugger`, `CkAStarDebugger`, `CkGoapDebugger`, `CkCrowdDebugger`,
  `CkEqsDebugger`, `CkInputDebugger`, `CkObjectPoolingDebugger`, `CkDebuggerLauncher`). Plugin dependency: **CkFoundation only**
  (`CkDebugger.uplugin:160-164`). The Runtime type on overlay + common is load-bearing — they
  declare native gameplay tags (commit `a4de221`).
- **No AngelScript surface anywhere in this plugin** — no `Script/` dir, zero `.as` files
  (verified via `rg --no-ignore`). **Blueprint surface exists only on the legacy module**
  (`Abstract, Blueprintable, EditInlineNew` filter/submenu/action classes, e.g.
  `Source/CkGameplayDebugger/Public/CkGameplayDebugger/Submenu/CkDebugger_Submenu.h:18`); the
  Slate and overlay modules are C++-only.

## Three generations — and which to extend

| Gen | Module(s) | Type | What it is | Status |
|---|---|---|---|---|
| 1 (2023) | `CkGameplayDebugger` | Runtime | Single UE-GameplayDebugger category: DebugProfile data assets, Blueprint Filters/Submenus/Actions, canvas draw. Compiles out of Shipping via self-defined `WITH_GAMEPLAY_DEBUGGER` (`CkGameplayDebugger.Build.cs:33-39`). | Frozen since early 2024 — **maintenance-only; do not add new features here** (deprecation not proclaimed; maintainer's call). |
| 2 (2025→) | 10 feature debugger modules + `CkDebuggerLauncher` | UncookedOnly | Slate editor-tab debuggers on the shared `CkDebuggerCommon` widget base, plus the dockable discovery/launch rail (tabs under the Tools main menu → Developer Tools category; engine 5.7.4 `WorkspaceMenuStructureModule.cpp:184-185`). Flagship: `CkEcsDebugger` — entity tree, auto-registered inspectors, viewport picker; console toggle `ck.EcsDebugger` (`Source/CkEcsDebugger/CkEcsDebugger_Module.cpp:25-27`). | **Extend when you need an editor tool.** |
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
- **Settings split** (commit `d607751`): per-user input keybinds live in
  `UCk_DebugOverlay_InputSettings` (`Config=EditorPerProjectUserSettings`, Editor Preferences →
  Ck — `Settings/CkDebugOverlay_Settings.h:136-143`); project visuals/layouts live in
  `UCk_DebugOverlay_Settings` (`Config=Game, DefaultConfig`, Project Settings → Ck — same file
  `:11-19`). New gesture keybinds go in the Input class, never the project class.
- **UncookedOnly modules are editor-only in practice**: they publicly depend on
  `WorkspaceMenuStructure`/`EditorStyle`/`ToolMenus` ungated (`CkEcsDebugger.Build.cs:20-24`) and
  on `UnrealEd` behind `Target.bBuildEditor` (`:66-72`). Runtime code — game modules, or the three
  Runtime modules here — must never depend on them.

## Known open issues (campaign record: `CkFoundation/.claude/reports/`)

- Repo-root `README.md:11` is stale — it claims a Cog plugin dependency; Cog is gone (zero real
  references left in `Source/`; `Config/DefaultCkDebugger.ini:4-7` still redirects dead Cog-era
  classes, and `Content/CkEcsDebugger_WindowManager.uasset` is orphaned).
- `[EDITOR-VERIFY]` Whether the overlay works in **packaged non-Shipping builds** is UNVERIFIED:
  the module is Runtime and compiles in, but its keybinds are EditorPerProjectUserSettings, which
  don't ship — open question for the maintainer.

## Provenance and maintenance

Facts above verified against code on **2026-07-14** (launcher branch based on `7fd41c8`). Re-verify with:

- Module count/types: `rg -c '"Name"' CkDebugger.uplugin` (expect **15** = 14 modules + the
  CkFoundation entry in the `Plugins` dependency array), or count modules only:
  `(Get-Content CkDebugger.uplugin -Raw | ConvertFrom-Json).Modules.Count` (expect 14); read the
  `"Type"` fields.
- Registration macros: `rg -n 'define CK_REGISTER_DEBUG_OVERLAY_PROVIDER' Source/CkEntityDebugOverlay` · `rg -n 'define CK_REGISTER_DEBUGGER_INSPECTOR' Source/CkEcsDebugger`.
- Settings Config attributes: `rg -n 'UCLASS\(Config' Source/CkEntityDebugOverlay/Public/CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h`.
- No-AS claim: `rg --no-ignore --files -g '*.as' .` (expect zero matches).
