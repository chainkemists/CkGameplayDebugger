#include "Misc/AutomationTest.h"

#include "CkInputHudOverlay/Model/CkInputHud_Model.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_spec
{
    constexpr auto TapHoldThresholdMs = 300.0f;
    constexpr auto FadeLifetime       = 10.0f;

    // The clock the model reads is wall time, which is never zero at runtime; the tests use a base offset so a
    // press stamped at "t = 0" cannot accidentally read as the never-pressed default.
    constexpr auto T0 = 100.0;

    auto
        Get_Label(
            const FCk_InputHud_Model& InModel,
            int32                     InIndex)
        -> FString
    {
        const auto& Events = InModel.Get_Events();

        return Events.IsValidIndex(InIndex) ? Events[InIndex].KeyLabel : FString{};
    }

    auto
        Get_Kind(
            const FCk_InputHud_Model& InModel,
            int32                     InIndex,
            double                    InNow)
        -> ECk_InputHud_EventKind
    {
        return FCk_InputHud_Model::Get_EventKind(InModel.Get_Events()[InIndex], InNow, TapHoldThresholdMs);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_EventLifecycle_Test,
    "Ck.InputHud.Model.EventLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_EventLifecycle_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    TestEqual(TEXT("empty model has no events"), Model.Get_Events().Num(), 0);
    TestEqual(TEXT("nothing is open"), Model.TryGet_OpenEvent(TEXT("E")), INDEX_NONE);

    const auto First = Model.Open_Event(TEXT("E"), FName(TEXT("E")), 10, T0, false);

    TestEqual(TEXT("the press is addressable"), Model.TryGet_OpenEvent(TEXT("E")), First);
    TestEqual(TEXT("an open press counts as held"), Model.Get_HeldNum(), 1);
    TestEqual(TEXT("and not as history"), Model.Get_ReleasedNum(), 0);

    Model.Set_EventResolution(First, TEXT("Dash"), true);

    TestEqual(TEXT("the intent lands"), Model.Get_Events()[First].IntentLabel, FString(TEXT("Dash")));
    TestTrue(TEXT("and marks the chip resolved"), Model.Get_Events()[First].Resolved);

    Model.Close_Event(First, 14, T0 + 0.1);

    TestEqual(TEXT("a released press stops being held"), Model.Get_HeldNum(), 0);
    TestEqual(TEXT("and becomes history"), Model.Get_ReleasedNum(), 1);
    TestEqual(TEXT("the up frame is recorded"), Model.Get_Events()[First].UpFrame, 14);
    TestEqual(TEXT("a closed press is no longer open"), Model.TryGet_OpenEvent(TEXT("E")), INDEX_NONE);

    // ---- Repeated presses of one key are INDEPENDENT chips ----
    const auto Second = Model.Open_Event(TEXT("E"), FName(TEXT("E")), 20, T0 + 1.0, false);

    TestEqual(TEXT("a second press appends rather than reopening"), Model.Get_Events().Num(), 2);
    TestEqual(TEXT("and it is the one a release would close"), Model.TryGet_OpenEvent(TEXT("E")), Second);

    // ---- A release resolves to the NEWEST open press of that key ----
    const auto Third = Model.Open_Event(TEXT("E"), FName(TEXT("E")), 25, T0 + 1.2, false);
    TestEqual(TEXT("the newest open press wins the address"), Model.TryGet_OpenEvent(TEXT("E")), Third);

    Model.Close_Event(Third, 26, T0 + 1.3);
    TestEqual(TEXT("closing it exposes the older one again"), Model.TryGet_OpenEvent(TEXT("E")), Second);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_EventKind_Test,
    "Ck.InputHud.Model.EventKind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_EventKind_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    const auto Live = Model.Open_Event(TEXT("W"), FName(TEXT("W")), 1, T0, false);

    TestEqual(TEXT("a fresh press is Press"),
        static_cast<int32>(Get_Kind(Model, Live, T0 + 0.05)),
        static_cast<int32>(ECk_InputHud_EventKind::Press));

    TestEqual(TEXT("held past the threshold it is Hold"),
        static_cast<int32>(Get_Kind(Model, Live, T0 + 0.5)),
        static_cast<int32>(ECk_InputHud_EventKind::Hold));

    TestEqual(TEXT("a live hold measures against NOW"),
        FCk_InputHud_Model::Get_DurationSeconds(Model.Get_Events()[Live], T0 + 0.5), 0.5);

    const auto Quick = Model.Open_Event(TEXT("Q"), FName(TEXT("Q")), 2, T0, false);
    Model.Close_Event(Quick, 3, T0 + 0.1);

    TestEqual(TEXT("released under the threshold is Tap"),
        static_cast<int32>(Get_Kind(Model, Quick, T0 + 5.0)),
        static_cast<int32>(ECk_InputHud_EventKind::Tap));

    const auto Long = Model.Open_Event(TEXT("R"), FName(TEXT("R")), 4, T0, false);
    Model.Close_Event(Long, 40, T0 + 2.6);

    TestEqual(TEXT("released past the threshold is HoldRelease"),
        static_cast<int32>(Get_Kind(Model, Long, T0 + 5.0)),
        static_cast<int32>(ECk_InputHud_EventKind::HoldRelease));

    TestEqual(TEXT("a closed hold freezes its duration"),
        FCk_InputHud_Model::Get_DurationSeconds(Model.Get_Events()[Long], T0 + 30.0), 2.6);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_HistoryCap_Test,
    "Ck.InputHud.Model.HistoryCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_HistoryCap_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    constexpr auto Cap = 3;

    auto Model = FCk_InputHud_Model{};

    // One pinned hold, then more releases than the cap allows.
    Model.Open_Event(TEXT("Shift"), FName(TEXT("Shift")), 0, T0, true);

    for (auto Index = 0; Index < 6; ++Index)
    {
        const auto Label  = FString::Printf(TEXT("K%d"), Index);
        const auto Opened = Model.Open_Event(Label, FName{*Label}, Index + 1, T0, false);
        Model.Close_Event(Opened, Index + 2, T0 + 0.05);
    }

    Model.Enforce_HistoryCap(Cap);

    TestEqual(TEXT("released entries are capped"), Model.Get_ReleasedNum(), Cap);
    TestEqual(TEXT("the held entry is never evicted"), Model.Get_HeldNum(), 1);
    TestEqual(TEXT("and it survives at the front"), Get_Label(Model, 0), FString(TEXT("Shift")));
    TestEqual(TEXT("the OLDEST releases went"), Get_Label(Model, 1), FString(TEXT("K3")));
    TestEqual(TEXT("newest release kept"), Get_Label(Model, 3), FString(TEXT("K5")));

    // A cap smaller than the number of holds still evicts nothing that is held.
    Model.Enforce_HistoryCap(0);
    TestEqual(TEXT("a zero cap clears history only"), Model.Get_ReleasedNum(), 0);
    TestEqual(TEXT("the hold is still pinned"), Model.Get_HeldNum(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_Fade_Test,
    "Ck.InputHud.Model.Fade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_Fade_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    const auto Held = Model.Open_Event(TEXT("Shift"), FName(TEXT("Shift")), 0, T0, true);

    const auto Released = Model.Open_Event(TEXT("E"), FName(TEXT("E")), 1, T0, false);
    Model.Close_Event(Released, 2, T0 + 0.05);

    const auto ReleaseTime = Model.Get_Events()[Released].UpTimeSeconds;

    const auto Fade = [&](double InAtSeconds) -> float
    {
        return FCk_InputHud_Model::Get_FadeOpacity(Model.Get_Events()[Released], InAtSeconds, FadeLifetime);
    };

    TestEqual(TEXT("full at the moment of release"), Fade(ReleaseTime), 1.0f);
    TestEqual(TEXT("full through the hold fraction"), Fade(ReleaseTime + 3.0), 1.0f);

    // Halfway along the 30%..100% ramp (t = 6.5s of a 10s lifetime with a 3s hold).
    TestEqual(TEXT("linear across the ramp"), Fade(ReleaseTime + 6.5), 0.5f, 0.001f);

    TestEqual(TEXT("gone at the end of the lifetime"), Fade(ReleaseTime + FadeLifetime), 0.0f);

    TestEqual(TEXT("a held entry never fades"),
        FCk_InputHud_Model::Get_FadeOpacity(Model.Get_Events()[Held], T0 + 1000.0, FadeLifetime), 1.0f);

    // ---- Pruning follows the same math ----
    Model.Prune_FadedEvents(ReleaseTime + 6.5, FadeLifetime);
    TestEqual(TEXT("a fading entry is kept"), Model.Get_Events().Num(), 2);

    Model.Prune_FadedEvents(ReleaseTime + FadeLifetime, FadeLifetime);
    TestEqual(TEXT("a spent entry is pruned"), Model.Get_ReleasedNum(), 0);
    TestEqual(TEXT("the hold outlives every lifetime"), Model.Get_HeldNum(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_LayerLine_Test,
    "Ck.InputHud.Model.LayerLineRebuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_LayerLine_Test::RunTest(const FString&)
{
    auto Model = FCk_InputHud_Model{};

    auto Layers = TArray<TPair<int32, FString>>{};
    Layers.Emplace(100, TEXT("Modal"));
    Layers.Emplace(10,  TEXT("Player"));

    TestTrue(TEXT("first set rebuilds"), Model.Set_Layers(Layers));
    TestEqual(TEXT("line joins top-down"), Model.Get_LayerLine(), FString(TEXT("Modal > Player")));

    TestFalse(TEXT("an unchanged list does not rebuild"), Model.Set_Layers(Layers));

    // Same names, different priority — the compare key carries BOTH, so this is a real change.
    Layers[1] = TPair<int32, FString>{20, TEXT("Player")};
    TestTrue(TEXT("a priority move rebuilds"), Model.Set_Layers(Layers));

    Layers.RemoveAt(0);
    TestTrue(TEXT("a popped layer rebuilds"), Model.Set_Layers(Layers));
    TestEqual(TEXT("line follows the pop"), Model.Get_LayerLine(), FString(TEXT("Player")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_Reset_Test,
    "Ck.InputHud.Model.Reset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_Reset_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    Model.Open_Event(TEXT("E"), FName(TEXT("E")), 0, T0, false);
    Model.Set_HasSource(true);
    Model.Set_LastSeenSamplerFrame(42);

    auto Layers = TArray<TPair<int32, FString>>{};
    Layers.Emplace(1, TEXT("Player"));
    Model.Set_Layers(Layers);

    // Losing the source must not leave a key pinned forever: the release will never arrive.
    Model.Reset_VolatileState(T0 + 1.0);

    TestEqual(TEXT("open entries are closed, not dropped"), Model.Get_HeldNum(), 0);
    TestEqual(TEXT("and become history"), Model.Get_ReleasedNum(), 1);
    TestEqual(TEXT("layer line cleared"), Model.Get_LayerLine(), FString{});
    TestEqual(TEXT("sampler cursor cleared"), Model.Get_LastSeenSamplerFrame(), INDEX_NONE);

    Model.Reset();

    TestEqual(TEXT("reset empties the stream"), Model.Get_Events().Num(), 0);
    TestFalse(TEXT("source cleared"), Model.Get_HasSource());

    // Reset must also clear the layer COMPARE key, or the first post-reset set would be seen as unchanged and the
    // line would stay empty forever.
    TestTrue(TEXT("the first set after reset rebuilds"), Model.Set_Layers(Layers));
    TestEqual(TEXT("line rebuilt"), Model.Get_LayerLine(), FString(TEXT("Player")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_LivenessSweep_Test,
    "Ck.InputHud.Model.LivenessSweep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_LivenessSweep_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    // A genuinely held key, and an ORPHAN whose routed release never arrived (focus-loss flush, or a rapid
    // click whose within-frame ordering paired as release-then-press).
    const auto StillHeld = Model.Open_Event(TEXT("W"), FName(TEXT("W")), 1, T0, false);
    const auto Orphan    = Model.Open_Event(TEXT("LMB"), FName(TEXT("LeftMouseButton")), 2, T0 + 0.1, false);

    auto PhysicallyHeld = TSet<FName>{};
    PhysicallyHeld.Add(FName(TEXT("W")));

    Model.Close_OpenEventsNotHeld(
        [&](const FName& InKeyName) { return PhysicallyHeld.Contains(InKeyName); },
        T0 + 1.0);

    TestEqual(TEXT("the physically held key stays pinned"), Model.TryGet_OpenEvent(TEXT("W")), StillHeld);
    TestEqual(TEXT("the orphan is closed"), Model.TryGet_OpenEvent(TEXT("LMB")), INDEX_NONE);
    TestFalse(TEXT("closed, not dropped — it lands in history"),
        FCk_InputHud_Model::Get_IsHeld(Model.Get_Events()[Orphan]));
    TestEqual(TEXT("with no up-frame to name"), Model.Get_Events()[Orphan].UpFrame, INDEX_NONE);
    TestEqual(TEXT("and only history counts changed"), Model.Get_HeldNum(), 1);

    // The sweep is idempotent: a second pass with the same truth closes nothing further.
    Model.Close_OpenEventsNotHeld(
        [&](const FName& InKeyName) { return PhysicallyHeld.Contains(InKeyName); },
        T0 + 2.0);
    TestEqual(TEXT("a second sweep is a no-op"), Model.Get_HeldNum(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
