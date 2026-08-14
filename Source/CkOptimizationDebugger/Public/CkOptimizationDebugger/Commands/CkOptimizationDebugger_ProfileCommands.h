#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/NameTypes.h"

// --------------------------------------------------------------------------------------------------------------------

/** The shelves the profiling page is divided into. Declaration order IS the order the sections render in — the page
 *  walks `Get_AllProfileGroups()` and asks for each group's entries, so there is no second ordering to keep in step. */
enum class ECkOptimizationDebugger_ProfileGroup : uint8
{
    Timing,
    Gpu,
    ViewModes,
    Nanite,
    Lumen,
    VirtualShadowMaps,
};

// --------------------------------------------------------------------------------------------------------------------

/** How an entry behaves once it is pressed, which is also what widget renders it.
 *
 *  `Toggle`   — flips something that stays on until it is flipped back (a stat overlay). Renders as an icon toggle.
 *  `OneShot`  — does its work and is over (a GPU capture). Renders as a command button.
 *  `ViewMode` — replaces whatever the viewport was showing. Renders as an icon toggle too, but the whole
 *               `ViewMode`-kinded set behaves as ONE exclusive choice: turning one on turns the others off, because
 *               a viewport has exactly one view mode. */
enum class ECkOptimizationDebugger_ProfileCommandKind : uint8
{
    Toggle,
    OneShot,
    ViewMode,
};

// --------------------------------------------------------------------------------------------------------------------

/** WHICH engine lever an entry pulls. This is not decoration: the four levers are genuinely different APIs, and
 *  picking the wrong one is how a profiling panel ships buttons that do nothing.
 *
 *  In particular the `viewmode <name>` console command is wired into `UGameViewportClient::Exec` ONLY
 *  (`GameViewportClient.cpp:3406`) — `UUnrealEdEngine::Exec` does not handle it. Typing it at an editor level
 *  viewport changes nothing, so every viewport-affecting entry here drives `FEditorViewportClient` directly, exactly
 *  as the engine's own viewport menu does (`FEditorViewportClient::ChangeNaniteVisualizationMode` et al,
 *  `EditorViewportClient.cpp:3122-3165`). */
enum class ECkOptimizationDebugger_ProfileLever : uint8
{
    /** `UEngine::ExecEngineStat(World, ViewportClient, Name)` — the same call `SEditorViewport::ToggleStatCommand`
     *  makes (`SEditorViewport.cpp:903-909`). It parks the viewport client in `GStatProcessingViewportClient` and
     *  then execs `STAT <name>`, which serves both engine stats (fps/unit/unitgraph) and plain stat GROUPS. */
    EngineStat,

    /** A plain console string, exec'd against the editor world. Reaches both the exec handlers and the console
     *  variable manager (`UnrealEngine.cpp:5415`). */
    ConsoleCommand,

    /** `FEditorViewportClient::SetViewMode(EViewModeIndex)`. */
    ViewMode,

    /** `FEditorViewportClient::ChangeNaniteVisualizationMode(FName)` — sets `VMI_VisualizeNanite` AND the mode. */
    NaniteVisualization,

    /** `FEditorViewportClient::ChangeLumenVisualizationMode(FName)` — sets `VMI_VisualizeLumen` AND the mode.
     *
     *  Deliberately not the `r.Lumen.Visualize.ViewMode` cvar: that string cvar is registered
     *  (`LumenVisualizationData.cpp:99-103`) but nothing in Runtime or Renderer ever reads it back, so setting it
     *  changes nothing anybody can see. */
    LumenVisualization,

    /** `FEditorViewportClient::ChangeVirtualShadowMapVisualizationMode(FName)`.
     *
     *  Deliberately not the `r.Shadow.Virtual.Visualize` cvar: the code that would force the show flag on from that
     *  cvar only runs once the show flag is ALREADY on (`VirtualShadowMapArray.cpp:937,2148-2154`), so the cvar
     *  alone is inert. */
    VirtualShadowMapVisualization,
};

// --------------------------------------------------------------------------------------------------------------------

/** One thing the profiling page can do. Plain data: no widget, no engine handle, nothing that can die underneath it.
 *
 *  Exactly one lever field is meaningful per entry, and `Ck.OptimizationDebugger.Profiling.CatalogIntegrity` pins
 *  that — an entry with both a console command and a view-mode index would be an entry whose behaviour depends on
 *  which branch of the executor happened to be written first. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ProfileCommand
{
    /** Stable identity, `<Group>.<Thing>`. What the page keys widgets by and what a spec names an entry with. */
    FName Id;

    FString DisplayName;

    /** What the reader gets and roughly what it costs. Becomes the button's hover text, so it says the cost — a
     *  launcher that hides "this stalls the frame" behind a friendly label is a launcher people stop trusting. */
    FString Description;

    ECkOptimizationDebugger_ProfileCommandKind Kind = ECkOptimizationDebugger_ProfileCommandKind::OneShot;

    ECkOptimizationDebugger_ProfileGroup Group = ECkOptimizationDebugger_ProfileGroup::Timing;

    ECkOptimizationDebugger_ProfileLever Lever = ECkOptimizationDebugger_ProfileLever::ConsoleCommand;

    /** `EngineStat` and `ConsoleCommand` levers only — the string a reader would type to get the same result, which
     *  is what the tooltip shows and the status strip echoes. Empty on every viewport lever, because there is no
     *  console string that does what those do in an editor viewport. */
    FString ConsoleCommand;

    /** `EngineStat` lever only: the bare name `ExecEngineStat` takes and `IsStatEnabled` answers about, i.e. `fps`
     *  for `stat fps`. Kept beside the full command rather than parsed out of it — the spec asserts the two agree. */
    FString StatName;

    /** Every viewport lever: the view mode the entry leaves the viewport in. For the three visualization levers this
     *  is the mode `Change*VisualizationMode` itself selects, and it is what makes "is this entry the one showing"
     *  answerable without asking the engine twice. */
    EViewModeIndex ViewModeIndex = VMI_Unknown;

    /** The three visualization levers only: the mode name registered with the engine's visualization data. */
    FName VisualizationMode;
};

// --------------------------------------------------------------------------------------------------------------------

/** What a press did. A failure is ordinary — outside an editor session there is no viewport to drive — and saying so
 *  beats a button that appears to work. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ProfileCommandResult
{
    bool DidRun = false;

    FString Message;
};

// --------------------------------------------------------------------------------------------------------------------

/** The viewport's answer to "what are you showing right now", reduced to the four facts an entry's on/off depends
 *  on. A plain struct so the rule that reads it is pure and a spec can drive it without an editor.
 *
 *  This is a READ of live engine state, not a record this tool keeps. That distinction is the whole point: a toggle
 *  the reader flipped from the console, or from the viewport's own Show menu, moves these buttons too. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ProfileViewportState
{
    EViewModeIndex ViewModeIndex = VMI_Unknown;

    FName NaniteVisualizationMode;
    FName LumenVisualizationMode;
    FName VirtualShadowMapVisualizationMode;

    /** The names the viewport client reports as enabled. Compared case-insensitively, as `FString` always is. */
    TArray<FString> EnabledStats;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_profile_commands
{
    /** How many custom console commands the page remembers. Small on purpose: the box is for the command that is not
     *  on the shelf, and a long history of one-offs is a list nobody reads. */
    constexpr auto k_MaxRecentCommands = 5;

    // ---- Catalog ----

    /** Every entry, in the order the page renders them within their groups. One immutable table. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllCommands() -> const TArray<FCkOptimizationDebugger_ProfileCommand>&;

    /** Every group, in declaration order — which is section order on the page. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllGroups() -> TArray<ECkOptimizationDebugger_ProfileGroup>;

    /** The entries of one group, in catalog order. Deterministic by construction: it is a filter over one table, not
     *  a sort, so two calls cannot disagree and no tie-break is needed. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CommandsInGroup(
        ECkOptimizationDebugger_ProfileGroup InGroup) -> TArray<FCkOptimizationDebugger_ProfileCommand>;

    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_Command(
        FName InId) -> const FCkOptimizationDebugger_ProfileCommand*;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_GroupLabel(
        ECkOptimizationDebugger_ProfileGroup InGroup) -> FString;

    /** The muted clarifier under a section title. Says what the shelf is FOR, not what each button does. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_GroupSubText(
        ECkOptimizationDebugger_ProfileGroup InGroup) -> FString;

    /** The short form of what an entry pulls, for the tooltip's last line and the status strip's echo. Pure, so the
     *  sentence the button promises and the sentence the status prints cannot drift. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CommandSummary(
        const FCkOptimizationDebugger_ProfileCommand& InCommand) -> FString;

    // ---- Pure state rules ----

    /** Whether this entry is the thing the viewport is currently showing.
     *
     *  For the viewport levers this is inherently exclusive — a viewport has one view mode — which is why the whole
     *  `ViewMode`-kinded set behaves as one radio group without anything here enforcing it. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Is_CommandActive(
        const FCkOptimizationDebugger_ProfileCommand& InCommand,
        const FCkOptimizationDebugger_ProfileViewportState& InState) -> bool;

    /** How many entries are currently CHANGING what the reader sees — every enabled stat overlay plus the active
     *  view mode, unless that view mode is Lit.
     *
     *  Lit is excluded deliberately: it is the resting state, and a page tab that read `1` on a freshly opened
     *  editor would be a badge nobody could ever clear. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ActiveCommandCount(
        const FCkOptimizationDebugger_ProfileViewportState& InState) -> int32;

    /** The id of the one viewport-lever entry this state is showing, or `NAME_None` when the viewport is on a mode
     *  no entry names. The page prints it above the sections so the reader can see what they are looking at without
     *  scrolling the shelf they set it from. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_ActiveViewportCommandId(
        const FCkOptimizationDebugger_ProfileViewportState& InState) -> FName;

    /** Records a custom command as the most recent one: de-duplicated case-insensitively (re-running a command moves
     *  it to the front rather than growing the list), trimmed, and capped. Empty input changes nothing. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Push_RecentCommand(
        TArray<FString>& InOutRecent,
        const FString& InCommand,
        int32 InMaxEntries) -> void;

    // ---- Execution ----

    /** Whether there is anything to drive: an editor session with a level viewport. False in a packaged
     *  Development build, where this module still ships but has no editor viewport to point at. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CanExecute() -> bool;

    /** What the viewport is showing, read live. Returns a default-constructed state when there is no viewport, which
     *  reports every entry as off — the honest answer, and the same one the disabled buttons give. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ViewportState() -> FCkOptimizationDebugger_ProfileViewportState;

    /** `Is_CommandActive` over the live viewport, with the stat case answered by the viewport client's own
     *  `IsStatEnabled` so a per-paint attribute does not copy the enabled-stat list once per button. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_IsCommandActive(
        const FCkOptimizationDebugger_ProfileCommand& InCommand) -> bool;

    /** `Get_ActiveCommandCount` over the live viewport, WITHOUT building a state struct.
     *
     *  This is what the Profiling page tab's count binds to, and that tab bar sits outside the page switcher — so it
     *  is painted on every page, every frame. Going through `Get_ViewportState()` there copied the enabled-stat
     *  `TArray<FString>` twice per paint (the count attribute and the visibility attribute that reads it) on a
     *  window whose whole refresh doctrine is that nothing polls. Same rule, same answer, no allocation. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ActiveCommandCountLive() -> int32;

    /** `TryGet_ActiveViewportCommandId` over the live viewport, without building a state struct. Same reason. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_ActiveViewportCommandIdLive() -> FName;

    /** Pulls the entry's lever against the active editor level viewport. Outside the editor it runs nothing and says
     *  so. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Execute_Command(
        const FCkOptimizationDebugger_ProfileCommand& InCommand) -> FCkOptimizationDebugger_ProfileCommandResult;

    /** Execs a free-text command against the editor world — the escape hatch for everything not on the shelf. It is
     *  NOT routed through the viewport client, because an arbitrary string is not a stat and not a view mode. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Execute_CustomCommand(
        const FString& InCommand) -> FCkOptimizationDebugger_ProfileCommandResult;
}

// --------------------------------------------------------------------------------------------------------------------
