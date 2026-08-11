---
name: ck-gameplaydebugger-extension
description: "Use when adding Ck debugger UI: overlays, ECS inspectors, Slate tabs, legacy UE GameplayDebugger maintenance, or PIE teardown crashes. Not for authoring debug data."
---

# Extending the Ck Debugger Suite

All paths below are relative to `Plugins/CkGameplayDebugger/` unless prefixed otherwise.
Identity, module table, and plugin-local rules: **[`CLAUDE.md`](../../CLAUDE.md) at this plugin's
root** — read it first; this skill does not restate it. Quick recap of the one naming trap: the
repo folder is `CkGameplayDebugger`, the plugin it ships is `CkDebugger.uplugin`, and the module
named `CkGameplayDebugger` is the *legacy* generation, not the plugin.

**The boundary (never cross it):** debug **data** lives in CkFoundation — fragments, `UCk_Utils_*`
accessors, records. Debug **UI** lives here — overlay providers, Slate tabs, inspectors. If the
value you want to show isn't reachable through a CkFoundation Utils call, first expose it there
(load `ckecs-architecture-contract`), then come back and render it here. Never park fragments or
gameplay state in this plugin, and never make a CkFoundation module depend on a debugger module.

All extension surfaces in Runbooks A-C are **C++-only** — this plugin has no AngelScript surface
at all, and Blueprint exists only on the legacy Gen-1 path (Runbook D). Source: plugin root
`CLAUDE.md` §identity.

## When NOT to use this skill

| You actually want to… | Load instead |
|---|---|
| Add/change the fragment, signal, or Utils the debugger would display | `ckecs-architecture-contract` (+ `ckecs-domain-reference` to find the feature) |
| Slate pitfalls/contracts while WRITING widget code (styles, rows, lifetime, flicker, viewport interaction) | `ck-slate-tools` (this plugin) |
| Diagnose a framework bug (the debugger is just how you noticed it) | `ck-debugging-playbook` |
| Fix build/environment failures while compiling this plugin | `ck-build-and-env` |
| Author automation tests for a feature | `ck-tests-authoring-and-running` |

## Orientation — three generations, which to extend

Full table with citations: plugin root `CLAUDE.md` §"Three generations". Decision rule:

| Your task | Extend | Runbook |
|---|---|---|
| Show live info **on screen, in-game/PIE** for entities (focus card, world pills) | Gen 3 — `CkEntityDebugOverlay` provider | A |
| Show a fragment family in the **CK ECS Debugger** editor tab's inspector panel | Gen 2 — `CkEcsDebugger` inspector | B |
| A whole **new editor analysis tool** (own tab, own view-model, graphs/history) | Gen 2 — new `Ck<Feature>Debugger` module | C |
| Anything touching the UE-GameplayDebugger category / BP DebugProfiles | Gen 1 — **maintenance only, no new features** | D |

Start with A or B; C only when a feature needs more surface than an inspector section (its own
timeline, graph, multi-panel layout — compare `CkGoapDebugger`). D is frozen.

---


## Reference files — load only what the task needs

Section numbers cited elsewhere in this skill point into these files.

| Topic | Read |
|---|---|
| Runbooks A-D | `references/runbooks.md` |

## Settings — who edits what, where

| Class | Config | Edited in | Holds |
|---|---|---|---|
| `UCk_DebugOverlay_Settings` (`Settings/CkDebugOverlay_Settings.h:11-19`) | `Config=Game, DefaultConfig` | **Project Settings → Ck → Ck On-Screen Debugger** (committed, shared) | Layouts/layout assets/StartingLayout, plate anchor/width/font, diamond scale, world-tag distances, fan-out |
| `UCk_DebugOverlay_InputSettings` (same file) | `Config=GameUserSettings` | **Editor Preferences → Ck → Ck On-Screen Debugger (Input)** in editor; the same per-user store is available to packaged tools | Double-tap gesture keys (LockKey, EcsDebuggerFocusKey, CycleCoLocatedKey, UnpinAllKey, HelpKey), co-located radii, double-tap window, remembered layout |
| `UCk_GameplayDebugger_ProjectSettings_UE` / `_UserSettings_UE` (`Source/CkGameplayDebugger/.../Settings/CkDebugger_Settings.h:33/:15`) | defaultconfig / user | Project Settings ↔ Editor Preferences ("Gameplay Debugger") | Legacy default profile / per-user profile override |

Rule (plugin root `CLAUDE.md` §rules): **new overlay gesture keybinds go in the Input class,
never the project class** — per-user rebinding must not dirty shared config. Slate-debugger
refresh-rate and node-theme settings are a fourth class (`UCkDebuggerSettings`,
`Source/CkDebuggerCommon/Settings/CkDebuggerSettings.h` — also per-user).

## Common mistakes

- **Provider registers but never renders** → its tag isn't in any layout's
  `EnabledProviders`/`Entries` (Runbook A step 5). Registration alone does nothing.
- **Anonymous-namespace file-local helpers copied outside `CkEntityDebugOverlay`** → unity-build
  symbol collisions. Only safe there because of `bUseUnity=false` (`Build.cs:7-11`); elsewhere use
  the root doctrine's filename-derived named namespace.
- **Cached `FCk_Handle` survives PIE stop** → AV on next PIE start
  (`~FCk_Handle → ReleaseSharedReference`). Clear on EndPIE (`Source/CkSmDebugger/CLAUDE.md`);
  module-held Slate goes down on `OnEnginePreExit`, not `ShutdownModule`
  (`CkEcsDebugger_Module.cpp:60-66`).
- **Debug data placed in this plugin / debugger UI placed in CkFoundation** → boundary violation
  (Overview). Expose data via the feature's Utils first.
- **Game/runtime code depending on an UncookedOnly debugger module** → they are editor-only in
  practice (ungated `WorkspaceMenuStructure`/`EditorStyle`/`ToolMenus`,
  `CkEcsDebugger.Build.cs:20-24`; `UnrealEd` behind `bBuildEditor` `:66-72`). Only the three
  Runtime modules may be referenced outside the editor, and game modules shouldn't need even those.
- **Expecting `Build_Inspector` to re-run per frame** → it runs on build/rebuild; rows update via
  their value lambdas. Structural change ⇒ `RequestRebuild()` from `Tick`
  (`CkDebuggerInspector_Base.h:57-65`).
- **New features on the legacy module** → Runbook D banner. Also skip its stale `README.md` (still
  claims a Cog dependency; Cog is gone — plugin root `CLAUDE.md` §open issues).
- **`SCkDebug_HistoryRow`/`SelectableLabel` inside `SListView` rows** → click-trap, rows become
  unselectable (`Source/CkDebuggerCommon/CLAUDE.md` §"List / tree rows").

## Open questions (do not silently resolve)

- `[PACKAGED-VERIFY]` **Packaged non-Shipping overlay**: `CkEntityDebugOverlay` is a Runtime module
  and `UCk_DebugOverlay_InputSettings` now uses the packaged-capable per-user `GameUserSettings`
  store while retaining its Editor Preferences placement. Live acceptance is still required:
  package a Development client, run `ck.DebugOverlay 1`, observe card/pills, exercise the LShift×2
  pin gesture, change a binding/layout, restart, and confirm the choice persists. Track the result
  in plugin root `CLAUDE.md` §open issues.

## Provenance and maintenance

Authored 2026-07-02 against plugin HEAD `d607751` (clean tree) and engine UnrealEngine-Angelscript
5.7.4 source. Every file:line above was read first-hand on that date. Re-verify volatile facts:

```bash
# cwd: Plugins/CkGameplayDebugger  (Git Bash; use rg --no-ignore — repo-root .ignore excludes
# Content/Intermediate here, and the Glob/Grep tools have produced false-empties under Plugins/)
rg --no-ignore -n 'CK_REGISTER_DEBUG_OVERLAY_PROVIDER\(' Source/CkEntityDebugOverlay --glob '*.cpp' | grep -vc define   # 19 providers
rg --no-ignore -n 'CK_REGISTER_DEBUGGER_INSPECTOR\('     Source --glob '*.cpp' | grep -vc define                        # 40 inspectors
rg --no-ignore -n 'TEXT\("ck\.DebugOverlay' Source/CkEntityDebugOverlay/Public/CkEntityDebugOverlay/Subsystem/CkDebugOverlay_Subsystem.cpp  # command list
rg --no-ignore -n 'bUseUnity|WITH_CK_DEBUG_OVERLAY' Source/CkEntityDebugOverlay/CkEntityDebugOverlay.Build.cs
rg --no-ignore -n 'UCLASS\(Config' Source/CkEntityDebugOverlay/Public/CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h
rg -n '"Type"' CkDebugger.uplugin        # 3 Runtime + 9 UncookedOnly
rg --no-ignore -n 'StartingLayout =' Source/CkEntityDebugOverlay/Private/Settings/CkDebugOverlay_Settings.cpp   # All
```

If a count or line number drifts, trust the code and update this skill; the interface shapes
(`ICk_DebugOverlay_Provider`, `ICkDebuggerComponentInspector_Base`) and the two registration
macros are the stable spine.
