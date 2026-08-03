# Runbooks A-D

Reference for `ck-gameplaydebugger-extension`: add an EntityDebugOverlay provider (A), an ECS-debugger component inspector (B), a whole Slate debugger module (C), a legacy IGameplayDebugger category (D). Pick from Orientation in SKILL.md first.

## RUNBOOK A — add an EntityDebugOverlay provider

Exemplar to copy: `Source/CkEntityDebugOverlay/Private/Providers/CkDebugOverlay_Provider_Aggro.h/.cpp`.
19 registered providers exist as of 2026-07-02 (re-verify command in Provenance).

1. **Create** `Source/CkEntityDebugOverlay/Private/Providers/CkDebugOverlay_Provider_<Feature>.h`
   and `.cpp`. Subclass `ICk_DebugOverlay_Provider`
   (`Source/CkEntityDebugOverlay/Public/CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h:30`).
   Virtuals and what each feeds:

   | Virtual (Provider.h line) | Feeds |
   |---|---|
   | `Get_ProviderTag()` :36 | Unique tag under `Ck.OnScreenDebugger.Provider.<Feature>` — keys the card section, layout membership, and the world-pill badge. |
   | `Get_FieldTags()` :39 | Every field the provider can emit, each with `DefaultEnabled` (`FCk_DebugOverlay_FieldDesc` :9-13). Layouts resolve these into `Cfg.EnabledFields`. |
   | `Get_SortPriority()` :42 | Lower = higher on the focus card (Aggro uses 22, Timer 50). |
   | `CanProvide(Entity)` :48 | The candidate gate — decides whether an entity gets a marker/pill/card at all. Keep it a cheap `UCk_Utils_<Feature>_UE::Has(Entity)`; it runs across all transform-bearing entities every overlay tick (pipeline: `Subsystem/CkDebugOverlay_Subsystem.h:37-45`). |
   | `Collect(Entity, Cfg, Out)` :54-57 | Fill `FCk_DebugOverlay_Section` (`Model/CkDebugOverlay_Model.h:19-25`): set `Out.ProviderTag` + `Out.SortPriority` **before** any early-out, then append `FCk_DebugOverlay_Row` (`:9-16` — `FieldTag`, `Value` (FText), `Severity`). Only called when `CanProvide` returned true. |
   | `Get_CompactToken(...)` :63-65 | Optional short string for compact views/world pills; empty (the default) = hidden in compact mode. |
   | `Get_SupportsEntryFilter()` :45 | Optional; return true only if your `Collect` honours `Cfg.EntryFilter`. |

2. **Native tags** — `UE_DEFINE_GAMEPLAY_TAG` at the top of your `.cpp` (Aggro `.cpp:18-22`): one
   provider tag, one tag per field. File-local helpers in an **anonymous namespace** are the local
   convention here (Aggro `.cpp:26-30`) and are safe **in this module only** because
   `CkEntityDebugOverlay.Build.cs:11` sets `bUseUnity = false` — the in-code rationale at
   `Build.cs:7-10` is that every provider defines identically-named `ProviderTag()`/`FieldTag_*()`
   helpers, which would collide in a unity TU. Everywhere else the root doctrine's named-namespace
   rule applies (style: `Plugins/CkFoundation/CLAUDE.md`).

3. **Register** — bottom of the `.cpp` (Aggro `.cpp:136`):
   `CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_<Feature>)`
   (macro: `Provider/CkDebugOverlay_Registry.h:66-69`). It plants a static registrar that only
   calls `Register()` — no `StaticStruct()`/`StaticClass()` at static-init (`:47`), so it is safe
   at DLL load.

4. **Build.cs** — add the feature's CkFoundation module to
   `Source/CkEntityDebugOverlay/CkEntityDebugOverlay.Build.cs` (pattern comments `:19-27`).
   Shipping awareness: `WITH_CK_DEBUG_OVERLAY` is 1 for non-Shipping, 0 for Shipping
   (`Build.cs:36-39`); the subsystem's implementation compiles out at 0
   (`Subsystem/CkDebugOverlay_Subsystem.h:34-35,60`) so the overlay never ticks. Provider files
   themselves are not `#if`-gated (confirmed on the Aggro file) — they cost only their static
   registrar in Shipping.

5. **Layout membership — a registered provider does NOT render until a layout enables it.**
   Resolution has three paths (`Layout/CkDebugOverlay_Resolve.h:13-15`): provider tag in
   `Layout.EnabledProviders` → its `DefaultEnabled` fields turn on (or *all* fields when the
   layout sets `bEnableAllFields`, `Layout/CkDebugOverlay_Layout.h:51-55`); a
   `FCk_DebugOverlay_ProviderEntry` with no explicit fields → `DefaultEnabled` fields; an Entry
   with explicit `EnabledFields` → exactly those. The default layouts are **hard-coded strings**
   in the `UCk_DebugOverlay_Settings` constructor
   (`Source/CkEntityDebugOverlay/Private/Settings/CkDebugOverlay_Settings.cpp`, "All" layout with
   the in-code NOTE "as more inspectors are ported, add their provider tags here" at `:59`) — add
   your provider tag string there (at minimum to "All"; themed layouts as appropriate). Caveat:
   these are `Config` properties, so a consuming project whose `DefaultGame.ini` already
   serializes `Layouts` overrides the constructor defaults — then add it in that project's
   Project Settings instead.

6. **Optional badge** — add a mapping in `ck_debugoverlay::Get_ProviderAbbrev`
   (`Private/Tags/CkDebugOverlay_Tags.cpp:31+`); unknown leaves fall back to the first 4
   characters uppercased (`Tags/CkDebugOverlay_Tags.h:26-29`), so this is polish, not required.

Skeleton (mirrors the Aggro shape — copy it, don't freestyle):

```cpp
// CkDebugOverlay_Provider_Foo.cpp
#include "CkDebugOverlay_Provider_Foo.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkFoo/CkFoo_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Foo,       "Ck.OnScreenDebugger.Provider.Foo")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Foo_Value, "Ck.OnScreenDebugger.Provider.Foo.Value")

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_Foo; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_Foo_Value; }
}

auto FCk_DebugOverlay_Provider_Foo::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Foo::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return { { FieldTag_Value(), true } };
}

auto FCk_DebugOverlay_Provider_Foo::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_Foo_UE::Has(Entity);
}

auto FCk_DebugOverlay_Provider_Foo::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    FCk_DebugOverlay_Row Row;
    Row.FieldTag = FieldTag_Value();
    Row.Value    = FText::FromString(/* read via UCk_Utils_Foo_UE, guard ck::IsValid */);
    Row.Severity = ECk_DebugOverlay_Severity::Normal;
    Out.Rows.Add(MoveTemp(Row));
}

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Foo)
```

(Note: the interface's parameter names are `Entity`/`Cfg`/`Out`, not the doctrine's `In*` — you
are overriding an existing interface, so match it, as every shipped provider does. The same
module-local-style rule covers the skeleton's compact one-line definitions —
`auto FCk_...::Get_ProviderTag() const -> FGameplayTag` mirrors the shipped Aggro provider
(`CkDebugOverlay_Provider_Aggro.cpp:34,51`), off root's split-definition shape — and the
`FCk_DebugOverlay_Row Row;` declare-then-fill construction: corpus-faithful here, not a template
for CkFoundation code.)

### Verify — overlay console surface (all names read from `Subsystem/CkDebugOverlay_Subsystem.cpp:158-216`)

| Console object | Effect |
|---|---|
| `ck.DebugOverlay` (cvar, `ECVF_Cheat`) | Master enable: 1 on, 0 off (`:158-162`) |
| `ck.DebugOverlay.Next` / `.Prev` | Cycle focus candidate (enables lock) (`:177-185`) |
| `ck.DebugOverlay.Lock` | Toggle focus lock on current entity (`:187-190`) |
| `ck.DebugOverlay.Layout.Next` / `.Layout.Prev` | Cycle layouts (`:192-200`) |
| `ck.DebugOverlay.UnpinAll` | Release all pinned cards (`:202-205`) |
| `ck.DebugOverlay.Help` | Toggle the full key-hints legend (`:207-210`) |
| `ck.DebugOverlay.World next|<idx>` | Override target PIE world (`:212-216`) |
| `ck.DebugOverlay.NearPlates` (cvar) | Lives in `CkDebugOverlay_Present.cpp`, not the subsystem (`:56-57` note) |

`[EDITOR-VERIFY]` — agents cannot PIE; a human must run this:
1. Build the editor (in BusterBlock: via the Unreal Toolbox per project rules), open a level whose
   entities carry your feature (a gym level is ideal), PIE.
2. Console: `ck.DebugOverlay 1`. Expect: focus card anchored top-right (default `PlateAnchor`,
   `Settings/CkDebugOverlay_Settings.h:39`), distance-scaled world pills, diamond markers.
3. Aim near an entity where your `CanProvide` is true. Expect your section on its card, ordered by
   `Get_SortPriority`. `ck.DebugOverlay.Next` cycles candidates. The starting layout is "All"
   (`Private/Settings/CkDebugOverlay_Settings.cpp:151`) — if you didn't add your tag to it,
   `ck.DebugOverlay.Layout.Next` until a layout that includes you, or expect nothing (step 5).
4. Automation: Session Frontend → Automation → run the `Ck.DebugOverlay.*` group (7 specs in
   `Source/CkEntityDebugOverlay/Private/Tests/`, e.g. `Ck.DebugOverlay.Registry.RegisterAndCreate`,
   `Ck.DebugOverlay.Resolve.ValidateLayout`).
5. Before landing: this is a class-2 (additive API) change — finish via CkFoundation's
   `ck-change-control` done-checklist (tests covering the new provider included).

---

## RUNBOOK B — add an ECS-debugger component inspector

Exemplar to copy: `Source/CkEcsDebugger/Public/CkEcsDebugger/Inspectors/CkInspector_Timer.h/.cpp`.
40 registered inspectors exist as of 2026-07-02. Module-local conventions and the panel
architecture: `Source/CkEcsDebugger/CLAUDE.md` (§"Adding a New Inspector", §"Inspector System").

1. **Create** `Source/CkEcsDebugger/Public/CkEcsDebugger/Inspectors/CkInspector_<Feature>.h` and
   `.cpp` — yes, the `.cpp` files of this module live under `Public/` too; match that.
2. **Subclass** `ICkDebuggerComponentInspector_Base`
   (`Source/CkEcsDebugger/Public/CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h:8`). Required
   (`:13-17`): `Get_ComponentName` (section title), `CanInspect` (cheap `Has()` gate),
   `Build_Inspector` (returns the section widget), `Get_SortPriority`, `Tick`. Optional:
   `IsFilterable` + the `(Entity, Filter)` overload (`:19-20`) for a per-inspector search box;
   `OnDeactivated` (`:31`) — **the** cleanup hook for per-entity state (debug draw, markers);
   entity may already be invalid when it fires; `Get_InspectorSections`/`IsMultiSection`
   (`:44-50`) to emit several collapsible sections; call `RequestRebuild()` from `Tick` (`:65`)
   when your data changed *structurally* — rows themselves are live lambdas and don't need it.
3. **Register** — `CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_<Feature>)`
   (macro: `Inspectors/CkDebuggerInspectorRegistry.h:58-61`). Timer places it right after the
   includes (`CkInspector_Timer.cpp:15`); providers place theirs at file bottom — position is
   irrelevant (static registrar), match the module you're in.
4. **Build rows** with `FCkInspectorWidgetBuilder` (`Inspectors/CkInspectorWidgetBuilder.h`):
   `AddRow(Label, ValueLambda, Color)` / `AddConditionalRow(Label, ValueLambda, ColorLambda)`.
   Capture the feature handle **by value** and validity-check on every read — the widget outlives
   the entity (Timer `.cpp:43-51`):

   ```cpp
   const auto CapturedTimer = TimerHandle;
   Builder.AddRow(
       FText::FromString(TEXT("Name:")),
       [CapturedTimer](const FCk_Handle&)
       {
           if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
           return FText::FromString(UCk_Utils_Timer_UE::Get_Name(CapturedTimer).ToString());
       },
       CkDebugStyle::Value_Tag());
   ```

   Render any `FCk_Handle` with `SCkDebug_EntityRef`, never a text block — the `ShowName`
   true/false matrix and the copy/selectable-text policy live in
   `Source/CkDebuggerCommon/CLAUDE.md` (§"Entity references").
5. **Priority** — lower renders higher in the panel. Existing ladder (from
   `Source/CkEcsDebugger/CLAUDE.md`): EntityInfo=10, Transform=20, TagSet=25, Network=30,
   Relationships=40; Timer=50. Slot yours accordingly.
6. **Build.cs** — add the feature module to `Source/CkEcsDebugger/CkEcsDebugger.Build.cs`
   (dep list `:8-64`).

### Verify

`[EDITOR-VERIFY]`:
1. Build, open the editor. Open the tab: console `ck.EcsDebugger` (toggle; `1`/`0` to force —
   `Source/CkEcsDebugger/CkEcsDebugger_Module.cpp:25-44`), or from the main menu. The spawner is
   registered in the Developer-Tools **Debug** workspace group (`Module.cpp:53-58`); in this
   engine (UnrealEngine-Angelscript 5.7.4) that group renders under the **Tools** main menu →
   Debug section (traced: engine `WorkspaceMenuStructureModule.cpp` anchors
   `DeveloperToolsCategory` in `ToolsMenuRoot`, which populates `MainFrame.MainMenu.Tools`) — NOT
   under Window. Confirm the exact rendered spot on first use.
2. PIE. In the tab: pick the PIE world (world selector buttons, top of the left panel), select an
   entity that satisfies your `CanInspect` — via the entity tree, or the viewport picker.
3. Expect your section in the **right-hand Inspector panel**, at its priority slot, values
   updating live. Change the entity's state in-game and watch the row lambdas track it.
4. Before landing: this is a class-2 (additive API) change — finish via CkFoundation's
   `ck-change-control` done-checklist.

---

## RUNBOOK C — add a whole Slate debugger module

**The canonical checklist is `Source/CkDebuggerCommon/CLAUDE.md` §"Creating a new debugger module
— checklist" (lines 449-477, 7 steps). Follow it — this runbook does not restate it.** That file
also owns the shared-widget catalog, copy/selectable-text policy, the `SListView` click-trap and
pointer-identity contracts, search bars, PMG overlays, and the safety rules — read it in full
before writing any Slate.

Contracts the checklist assumes, with their ground-truth citations:

| Contract | Citation |
|---|---|
| Module `"Type": "UncookedOnly"` in `CkDebugger.uplugin` (all 9 feature debuggers are) | `CkDebugger.uplugin` module entries; e.g. `:40-41` |
| Nomad tab spawner in Developer-Tools Debug group + optional console toggle | `Source/CkEcsDebugger/CkEcsDebugger_Module.cpp:53-58` (spawner), `:25-44` (`FAutoConsoleCommand`) |
| Honour the per-window refresh gate in your Tick | `FCkDebuggerRefreshGate::Should_RefreshNow(WindowId)` — `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Window/CkDebuggerRefreshGate.h:29-33` |
| **Clear every cached `FCk_Handle` on `FEditorDelegates::EndPIE`** — handles hold the registry by value; a handle outliving its PIE registry crashes on destruct (`~FCk_Handle → ReleaseSharedReference` AV on the *next* PIE start) | Contract, audit list, and crash signature: `Source/CkSmDebugger/CLAUDE.md` (whole file, 50 lines) |
| **Tear down handle-holding Slate on `FCoreDelegates::OnEnginePreExit`, NOT `ShutdownModule`** — by ShutdownModule the registry's shared state is freed | `Source/CkEcsDebugger/CkEcsDebugger_Module.cpp:60-66` (registration + rationale), `:109-121` (handler: drop refs, deliberately no `RequestCloseTab`) |
| `ck::DebugNav` — register nothing; you get click-to-open-in-ECS-Debugger for free on every `SCkDebug_EntityRef` you place (no-op if `CkEcsDebugger` isn't loaded) | `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Navigation/CkDebug_Navigator.h`; register/clear pattern `CkEcsDebugger_Module.cpp:71-88,127` |
| `ck::DebugSelectionSync` — subscribe to mirror selection made in *other* debuggers into yours (lineage-matched, `FApplyGuard` prevents echo loops); broadcast when *your* selection changes | `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h`; receive-side exemplar `CkEcsDebugger_Module.cpp:94-106` |

No `IGameplayDebugger` registration anywhere — the Slate suite is fully separate from UE's
GameplayDebugger. Data collection is a direct EnTT view walk on the world's transient entity
(e.g. `CkSmDebugger_DataCollector.cpp:63`), read-only.

Before landing: a new module is a class-2 (additive) change — finish via CkFoundation's
`ck-change-control` done-checklist (uplugin entry, module `Claude.md`, and tests included).

---

## RUNBOOK D — legacy IGameplayDebugger category

> **MAINTENANCE ONLY.** Frozen since early 2024 (plugin root `CLAUDE.md` §generations). Do not add
> new features here — new on-screen info goes to Runbook A. Touch this module only to fix a bug in
> the existing profile/filter/submenu pipeline.

Registration chain, end to end:
1. `FCkGameplayDebuggerModule::StartupModule` registers the **single** engine category
   `"CkGameplayDebugger"` (`Source/CkGameplayDebugger/CkGameplayDebugger_Module.cpp:15-27`),
   guarded by the self-defined `WITH_GAMEPLAY_DEBUGGER` (`CkGameplayDebugger.Build.cs:31-40`,
   0 in Shipping).
2. `FCk_GameplayDebugger_Category` (singleton, `Category/CkDebugger_Category.h:17`) is a thin shim
   forwarding the engine's four callbacks into dynamic delegates (`:52-55`).
3. `UCk_GameplayDebugger_Subsystem_UE::OnWorldBeginPlay` spawns the bridge actor — clients and
   standalone only, dedicated/listen servers skipped
   (`Subsystem/CkDebugger_Subsystem.cpp:27-49`).
4. `ACk_GameplayDebugger_DebugBridge_UE` binds the delegates and loads the profile: per-user
   override (Editor Preferences) if set, else project default (Project Settings), error-notify if
   neither (`Subsystem.cpp` profile block; settings classes `Settings/CkDebugger_Settings.h:15,33`).
5. `OnGameplayDebugger_CollectData` is **deliberately empty** ("Empty on purpose",
   `Bridge/CkDebugger_Bridge.cpp:164`) — everything runs in DrawData because CollectData does not
   run while the game is paused (in-code NOTE, `Bridge.cpp:215`). Consequence: nothing Ck-specific
   replicates through the engine's category replicator; all filtering/drawing is local.

Maintenance-mode extension (BP, no code): subclass `UCk_GameplayDebugger_DebugSubmenu_UE` /
`_DebugAction_UE` / `_DebugFilter_UE` (`Abstract, Blueprintable, EditInlineNew` —
`Submenu/CkDebugger_Submenu.h:18`; override the `BlueprintNativeEvent` hooks `DoActivateSubmenu` /
`DoDrawData` `:35-51`; set `_MenuName` `:58` and `_KeyToShowMenu` `:63`), add the instance to a
`UCk_GameplayDebugger_DebugProfile_PDA`, point Project Settings → Ck → Gameplay Debugger (or the
Editor Preferences per-user override) at that profile, PIE, activate the engine gameplay debugger
(apostrophe key by engine default — engine `GameplayDebuggerConfig`; the plugin ships an
apostrophe hint texture).

When this layer is still the right answer: you need integration with **UE's own in-game gameplay
debugger** on a running client — the apostrophe HUD alongside engine categories, with
BP-authorable content. That is the only thing Gens 2/3 don't do.

---

