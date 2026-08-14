#include "CkOptimizationDebugger/Commands/CkOptimizationDebugger_ProfileCommands.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_profiling_spec
{
    auto
        Join_Ids(
            const TArray<FCkOptimizationDebugger_ProfileCommand>& InCommands)
        -> FString
    {
        auto Ids = TArray<FString>{};

        for (const auto& Command : InCommands)
        { Ids.Add(Command.Id.ToString()); }

        return FString::Join(Ids, TEXT(","));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether a lever drives the viewport's view mode rather than the console. The three visualization levers and
     *  the plain view-mode lever share every structural rule, so the integrity spec asks this once. */
    auto
        Is_ViewportLever(
            ECkOptimizationDebugger_ProfileLever InLever)
        -> bool
    {
        return InLever == ECkOptimizationDebugger_ProfileLever::ViewMode ||
               InLever == ECkOptimizationDebugger_ProfileLever::NaniteVisualization ||
               InLever == ECkOptimizationDebugger_ProfileLever::LumenVisualization ||
               InLever == ECkOptimizationDebugger_ProfileLever::VirtualShadowMapVisualization;
    }

    // ----------------------------------------------------------------------------------------------------------------

    const auto k_LitId = FName{TEXT("ViewMode.Lit")};
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Profiling_CatalogIntegrity,
    "Ck.OptimizationDebugger.Profiling.CatalogIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Profiling_CatalogIntegrity::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_profiling_spec;
    using namespace ck_optimization_debugger_profile_commands;

    const auto& Commands = Get_AllCommands();

    TestTrue(TEXT("The catalog is not empty"), Commands.Num() > 0);

    auto SeenIds = TSet<FName>{};

    for (const auto& Command : Commands)
    {
        const auto Where = Command.Id.ToString();

        // ---- Identity ----
        TestFalse(*ck::Format_UE(TEXT("{}: has an id"), Where), Command.Id.IsNone());
        TestFalse(*ck::Format_UE(TEXT("{}: has a display name"), Where), Command.DisplayName.IsEmpty());
        TestFalse(*ck::Format_UE(TEXT("{}: has a description"), Where), Command.Description.IsEmpty());

        // Two entries sharing an id would be two buttons the window cannot tell apart — and `TryGet_Command` would
        // hand both of them the first one's lever.
        TestFalse(*ck::Format_UE(TEXT("{}: the id is unique"), Where), SeenIds.Contains(Command.Id));
        SeenIds.Add(Command.Id);

        const auto RoundTrip = TryGet_Command(Command.Id);

        TestNotNull(*ck::Format_UE(TEXT("{}: resolves back by id"), Where), RoundTrip);
        TestTrue(*ck::Format_UE(TEXT("{}: ...to the SAME entry"), Where),
            RoundTrip != nullptr && RoundTrip->Id == Command.Id);

        TestFalse(*ck::Format_UE(TEXT("{}: summarises itself"), Where), Get_CommandSummary(Command).IsEmpty());

        // ---- Exactly one lever is armed ----
        //
        // This is the rule the whole executor sits on. An entry carrying both a console string and a view-mode
        // index would behave according to whichever branch of the switch was written first, which is not a
        // behaviour anybody chose.
        switch (Command.Lever)
        {
            case ECkOptimizationDebugger_ProfileLever::EngineStat:
            {
                TestEqual(*ck::Format_UE(TEXT("{}: a stat is a toggle"), Where),
                    static_cast<int32>(Command.Kind),
                    static_cast<int32>(ECkOptimizationDebugger_ProfileCommandKind::Toggle));

                TestFalse(*ck::Format_UE(TEXT("{}: names a stat"), Where), Command.StatName.IsEmpty());

                // The two strings must agree, because the tooltip prints one and `ExecEngineStat` is handed the
                // other. A `stat rhi` button that toggled `initviews` would be indistinguishable from a wrong
                // threshold until somebody read the overlay carefully.
                TestEqual(*ck::Format_UE(TEXT("{}: the printed command IS the stat it runs"), Where),
                    Command.ConsoleCommand, ck::Format_UE(TEXT("stat {}"), Command.StatName));

                TestEqual(*ck::Format_UE(TEXT("{}: arms no view mode"), Where),
                    static_cast<int32>(Command.ViewModeIndex), static_cast<int32>(VMI_Unknown));

                TestTrue(*ck::Format_UE(TEXT("{}: arms no visualization mode"), Where),
                    Command.VisualizationMode.IsNone());

                break;
            }
            case ECkOptimizationDebugger_ProfileLever::ConsoleCommand:
            {
                TestEqual(*ck::Format_UE(TEXT("{}: a bare console command is one-shot"), Where),
                    static_cast<int32>(Command.Kind),
                    static_cast<int32>(ECkOptimizationDebugger_ProfileCommandKind::OneShot));

                TestFalse(*ck::Format_UE(TEXT("{}: carries a command"), Where), Command.ConsoleCommand.IsEmpty());
                TestTrue(*ck::Format_UE(TEXT("{}: names no stat"), Where), Command.StatName.IsEmpty());

                TestEqual(*ck::Format_UE(TEXT("{}: arms no view mode"), Where),
                    static_cast<int32>(Command.ViewModeIndex), static_cast<int32>(VMI_Unknown));

                break;
            }
            default:
            {
                TestTrue(*ck::Format_UE(TEXT("{}: the remaining levers all drive the viewport"), Where),
                    Is_ViewportLever(Command.Lever));

                TestEqual(*ck::Format_UE(TEXT("{}: a viewport lever is a view-mode entry"), Where),
                    static_cast<int32>(Command.Kind),
                    static_cast<int32>(ECkOptimizationDebugger_ProfileCommandKind::ViewMode));

                // Empty on purpose. `viewmode <name>` is handled by `UGameViewportClient::Exec` only, so a console
                // string here would be a string that does nothing in the editor viewport this page drives — and a
                // tooltip printing it would be a promise the button cannot keep.
                TestTrue(*ck::Format_UE(TEXT("{}: carries no console string"), Where),
                    Command.ConsoleCommand.IsEmpty());
                TestTrue(*ck::Format_UE(TEXT("{}: names no stat"), Where), Command.StatName.IsEmpty());

                TestTrue(*ck::Format_UE(TEXT("{}: arms a real view mode"), Where),
                    Command.ViewModeIndex != VMI_Unknown);

                const auto NeedsVisualizationMode =
                    Command.Lever != ECkOptimizationDebugger_ProfileLever::ViewMode;

                if (NeedsVisualizationMode)
                {
                    TestFalse(*ck::Format_UE(TEXT("{}: a visualizer names its mode"), Where),
                        Command.VisualizationMode.IsNone());
                }
                else
                {
                    TestTrue(*ck::Format_UE(TEXT("{}: a plain view mode names no visualization mode"), Where),
                        Command.VisualizationMode.IsNone());
                }

                break;
            }
        }
    }

    // ---- The one entry every visualizer shelf falls back to ----
    const auto* Lit = TryGet_Command(k_LitId);

    TestNotNull(TEXT("There is a Lit entry"), Lit);
    TestEqual(TEXT("...and it is the Lit view mode"),
        Lit != nullptr ? static_cast<int32>(Lit->ViewModeIndex) : -1, static_cast<int32>(VMI_Lit));

    TestNull(TEXT("An id nothing declares resolves to nothing"), TryGet_Command(FName{TEXT("NotACommand")}));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Profiling_GroupProjection,
    "Ck.OptimizationDebugger.Profiling.GroupProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Profiling_GroupProjection::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_profiling_spec;
    using namespace ck_optimization_debugger_profile_commands;

    const auto Groups = Get_AllGroups();

    // Section order on the page IS this list, so it has to be the enum's own order — a shelf that moved between two
    // sessions is a shelf the reader stops navigating by position.
    TestEqual(TEXT("Every group is enumerated"), Groups.Num(), 6);
    TestEqual(TEXT("...in declaration order"),
        static_cast<int32>(Groups[0]), static_cast<int32>(ECkOptimizationDebugger_ProfileGroup::Timing));
    TestEqual(TEXT("...through to the last one"),
        static_cast<int32>(Groups.Last()),
        static_cast<int32>(ECkOptimizationDebugger_ProfileGroup::VirtualShadowMaps));

    auto TotalInGroups = 0;
    auto SeenLabels = TSet<FString>{};

    for (const auto Group : Groups)
    {
        const auto InGroup = Get_CommandsInGroup(Group);
        const auto Label = Get_GroupLabel(Group);

        TestTrue(*ck::Format_UE(TEXT("{}: has entries"), Label), InGroup.Num() > 0);
        TestFalse(*ck::Format_UE(TEXT("{}: has a label"), Label), Label.IsEmpty());
        TestFalse(*ck::Format_UE(TEXT("{}: has a clarifier"), Label), Get_GroupSubText(Group).IsEmpty());

        // Two shelves with the same title would be two shelves the reader cannot refer to.
        TestFalse(*ck::Format_UE(TEXT("{}: the label is unique"), Label), SeenLabels.Contains(Label));
        SeenLabels.Add(Label);

        for (const auto& Command : InGroup)
        {
            TestEqual(*ck::Format_UE(TEXT("{}: only its own entries"), Label),
                static_cast<int32>(Command.Group), static_cast<int32>(Group));
        }

        // Determinism: it is a filter over one ordered table, so two calls cannot disagree and no tie-break is
        // involved. The assertion is cheap and it is what stops a future sort being slipped in here.
        TestEqual(*ck::Format_UE(TEXT("{}: the projection is deterministic"), Label),
            Join_Ids(InGroup), Join_Ids(Get_CommandsInGroup(Group)));

        TotalInGroups += InGroup.Num();
    }

    // A partition, not a cover: every entry appears in exactly one shelf, so nothing on the page is invisible and
    // nothing is drawn twice.
    TestEqual(TEXT("The shelves partition the catalog"), TotalInGroups, Get_AllCommands().Num());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Profiling_ViewportStateTransitions,
    "Ck.OptimizationDebugger.Profiling.ViewportStateTransitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Profiling_ViewportStateTransitions::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_profiling_spec;
    using namespace ck_optimization_debugger_profile_commands;

    const auto* Fps = TryGet_Command(FName{TEXT("Timing.Fps")});
    const auto* Rhi = TryGet_Command(FName{TEXT("Timing.Rhi")});
    const auto* Lit = TryGet_Command(k_LitId);
    const auto* QuadOverdraw = TryGet_Command(FName{TEXT("ViewMode.QuadOverdraw")});
    const auto* NaniteOverdraw = TryGet_Command(FName{TEXT("Nanite.Overdraw")});
    const auto* NaniteClusters = TryGet_Command(FName{TEXT("Nanite.Clusters")});
    const auto* ProfileGpu = TryGet_Command(FName{TEXT("Gpu.ProfileGpu")});

    if (Fps == nullptr || Rhi == nullptr || Lit == nullptr || QuadOverdraw == nullptr ||
        NaniteOverdraw == nullptr || NaniteClusters == nullptr || ProfileGpu == nullptr)
    {
        AddError(TEXT("The catalog no longer declares the entries this spec drives"));
        return false;
    }

    // ---- Nothing running ----
    auto State = FCkOptimizationDebugger_ProfileViewportState{};

    TestFalse(TEXT("A blank viewport runs no stat"), Is_CommandActive(*Fps, State));
    TestFalse(TEXT("...and shows no listed view mode"), Is_CommandActive(*Lit, State));
    TestTrue(TEXT("...so nothing is named as active"), TryGet_ActiveViewportCommandId(State).IsNone());
    TestEqual(TEXT("...and the tab shows no count"), Get_ActiveCommandCount(State), 0);

    // ---- A stat comes up ----
    //
    // Cased differently from the catalog on purpose: the engine's own stat menu enables "FPS" while this catalog
    // says "fps", and the two surfaces must agree about one overlay being on.
    State.EnabledStats.Add(FString{TEXT("FPS")});

    TestTrue(TEXT("An enabled stat reads as on, whatever its casing"), Is_CommandActive(*Fps, State));
    TestFalse(TEXT("...and only that one"), Is_CommandActive(*Rhi, State));
    TestEqual(TEXT("...and it counts"), Get_ActiveCommandCount(State), 1);

    // ---- Lit ----
    State.ViewModeIndex = VMI_Lit;

    TestTrue(TEXT("Lit reads as the active view mode"), Is_CommandActive(*Lit, State));
    TestEqual(TEXT("...and is what the header names"), TryGet_ActiveViewportCommandId(State), k_LitId);

    // Lit is the resting state, so it never inflates the tab's badge — otherwise a freshly opened editor would show
    // a count nobody could clear.
    TestEqual(TEXT("...but Lit does not count as something being changed"), Get_ActiveCommandCount(State), 1);

    // ---- A view mode replaces it ----
    State.ViewModeIndex = QuadOverdraw->ViewModeIndex;

    TestTrue(TEXT("The new view mode is on"), Is_CommandActive(*QuadOverdraw, State));
    TestFalse(TEXT("...and Lit is off, without anything having switched it off"), Is_CommandActive(*Lit, State));
    TestEqual(TEXT("...one active view mode, still one stat"), Get_ActiveCommandCount(State), 2);

    // ---- A visualizer needs BOTH halves ----
    //
    // The viewport client keeps the mode NAME after the view mode moves on, so a rule that read only the name would
    // report a visualizer nobody is looking at as still up.
    State.ViewModeIndex = VMI_Lit;
    State.NaniteVisualizationMode = NaniteOverdraw->VisualizationMode;

    TestFalse(TEXT("A stale visualization name alone is not an active visualizer"),
        Is_CommandActive(*NaniteOverdraw, State));

    State.ViewModeIndex = NaniteOverdraw->ViewModeIndex;

    TestTrue(TEXT("Name plus view mode IS one"), Is_CommandActive(*NaniteOverdraw, State));
    TestFalse(TEXT("...and its sibling mode is not"), Is_CommandActive(*NaniteClusters, State));
    TestEqual(TEXT("...and it is what the header names"), TryGet_ActiveViewportCommandId(State),
        NaniteOverdraw->Id);

    // Exclusivity, stated as the property the page depends on: at most one viewport entry can be active at once,
    // because a viewport has exactly one view mode.
    auto ActiveViewportEntries = 0;

    for (const auto& Command : Get_AllCommands())
    {
        if (Command.Kind != ECkOptimizationDebugger_ProfileCommandKind::ViewMode)
        { continue; }

        if (Is_CommandActive(Command, State))
        { ++ActiveViewportEntries; }
    }

    TestEqual(TEXT("Exactly one viewport entry is ever active"), ActiveViewportEntries, 1);

    // ---- A one-shot is never "on" ----
    TestFalse(TEXT("A GPU capture is an event, not a state"), Is_CommandActive(*ProfileGpu, State));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Profiling_RecentCommands,
    "Ck.OptimizationDebugger.Profiling.RecentCommands",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Profiling_RecentCommands::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_profile_commands;

    auto Recent = TArray<FString>{};

    Push_RecentCommand(Recent, FString{TEXT("  ")}, k_MaxRecentCommands);
    TestEqual(TEXT("Whitespace is not a command"), Recent.Num(), 0);

    Push_RecentCommand(Recent, FString{TEXT("  r.ScreenPercentage 50  ")}, k_MaxRecentCommands);
    TestEqual(TEXT("A command is stored trimmed"), Recent.Num(), 1);
    TestEqual(TEXT("...with no surrounding space"), Recent[0], FString{TEXT("r.ScreenPercentage 50")});

    Push_RecentCommand(Recent, FString{TEXT("stat unit")}, k_MaxRecentCommands);
    TestEqual(TEXT("The newest command is first"), Recent[0], FString{TEXT("stat unit")});

    // Re-running MOVES rather than duplicating: five slots holding the same command five times is a history that
    // has forgotten the other four.
    Push_RecentCommand(Recent, FString{TEXT("R.SCREENPERCENTAGE 50")}, k_MaxRecentCommands);

    TestEqual(TEXT("Re-running does not grow the list"), Recent.Num(), 2);
    TestEqual(TEXT("...it moves the command to the front"), Recent[0], FString{TEXT("R.SCREENPERCENTAGE 50")});
    TestEqual(TEXT("...and the other one drops back"), Recent[1], FString{TEXT("stat unit")});

    for (auto Index = 0; Index < k_MaxRecentCommands + 3; ++Index)
    { Push_RecentCommand(Recent, ck::Format_UE(TEXT("cmd{}"), Index), k_MaxRecentCommands); }

    TestEqual(TEXT("The list is capped"), Recent.Num(), k_MaxRecentCommands);
    TestEqual(TEXT("...keeping the newest"), Recent[0],
        ck::Format_UE(TEXT("cmd{}"), k_MaxRecentCommands + 2));

    const auto Before = Recent;
    Push_RecentCommand(Recent, FString{TEXT("ignored")}, 0);

    TestEqual(TEXT("A zero cap stores nothing"), Recent.Num(), Before.Num());
    TestEqual(TEXT("...and disturbs nothing"), Recent[0], Before[0]);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
