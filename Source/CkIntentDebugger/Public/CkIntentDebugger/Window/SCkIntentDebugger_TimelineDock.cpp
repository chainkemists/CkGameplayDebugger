#include "CkIntentDebugger/Window/SCkIntentDebugger_TimelineDock.h"

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"
#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_timeline
{
    constexpr auto PadS = 4.0f;
    constexpr auto PadM = 8.0f;
    // Enough vertical step that a micro-font label never overlaps its neighbour — 16 was not, and ten lanes of
    // intent names rendered as one scrunched column.
    constexpr auto LaneHeight = 22.0f;

    // A terminal phase is a FACT that persists (the matcher keeps one phase per intent), but drawing it to the
    // live edge renders a stale fact as a perpetual alarm bar. Terminal spans clamp to this width — a PULSE at
    // the frame it happened — and the marker + tooltip carry the full truth; only in-flight phases draw open.
    constexpr auto TerminalPhasePulseFrames = 20;
    constexpr auto MinTimelineHeight = 140.0f;

    // The default live-following window (~10s at a 60 Hz record — the SM scrub track's own default feel).
    // A full-range default makes right-drag pan a no-op, which reads as "pan is broken".
    constexpr auto InitialViewFrames = 600.0;

    // One lane per button the record witnessed inside the ring — the home of grammar-free presses (block Q),
    // which never enter the matcher and therefore have no intent lane to show up on.
    auto
        Get_ButtonLaneLabels(
            const FCkIntentDebugger_SourceSnapshot& InSource)
        -> TArray<FString>
    {
        auto Labels = TArray<FString>{};

        for (const auto& Frame : InSource.Frames)
        {
            for (const auto& Button : Frame.Held)
            { Labels.AddUnique(Button.Label); }
        }

        Labels.Sort();
        return Labels;
    }

    auto
        Get_LaneLabels(
            const FCkIntentDebugger_SourceSnapshot& InSource,
            const FCkIntentDebugger_LayerRow* InSelectedLayer)
        -> TArray<FString>
    {
        auto Labels = TArray<FString>{};

        for (const auto& Layer : InSource.Layers)
        { Labels.Add(ck::Format_UE(TEXT("L{}"), Layer.Priority)); }

        Labels.Append(Get_ButtonLaneLabels(InSource));

        if (InSelectedLayer != nullptr)
        {
            for (const auto& Intent : InSelectedLayer->Intents)
            { Labels.Add(Intent.Name.ToString()); }
        }

        Labels.Add(TEXT("BLOCKED"));

        return Labels;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_TimelineDock::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    const auto WeakPanel = TWeakPtr<SCkIntentDebugger_TimelineDock>(SharedThis(this));

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(TEXT("Timeline")))
                // The trailing rN is a BUILD STAMP — bump it on every behavior change to this dock or the
                // shared timeline widget. Several feedback rounds were burned on PIE sessions that predated
                // the build under discussion; the stamp makes "which build am I looking at" a read, not a guess.
                .SubText(FText::FromString(TEXT("frame axis · drag to scrub · wheel zooms · right-drag pans · F = follow live · r13")))
                .RightContent()
                [
                    SNew(SHorizontalBox)

                    // The IntentDebugHistory recording's retention ([P11-D15]) — the number typed IS the history
                    // depth, applied immediately through the ViewModel's one sanctioned mutation.
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(ck_intent_debugger_timeline::PadS, 0.0f)
                    [
                        SNew(STextBlock)
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(CkStyle::TextMute())
                            .Text(FText::FromString(TEXT("history")))
                            .ToolTipText(FText::FromString(TEXT(
                                "Frames the source's IntentDebugHistory recording retains (~60 per second) — "
                                "the debug fragment the timeline reads; applies immediately. A source composed "
                                "WITHOUT the feature falls back to the sampler's own short ring and this field "
                                "does nothing for it.")))
                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, ck_intent_debugger_timeline::PadM, 0.0f)
                    [
                        SNew(SCkDebug_NumericEditor)
                            .Kind(ECkDebug_NumericKind::Integer)
                            .MinValue(120.0)
                            .Value_Lambda([WeakPanel]() -> double
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                                { return 0.0; }

                                const auto* Source = Panel->_ViewModel->TryGet_SelectedSource();
                                if (Source == nullptr)
                                { return 0.0; }

                                return static_cast<double>(
                                    Source->HasHistory ? Source->HistoryCapacity : Source->RingCapacity);
                            })
                            .OnValueCommitted_Lambda([WeakPanel](double InValue)
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (Panel.IsValid() && Panel->_ViewModel.IsValid())
                                { Panel->_ViewModel->Request_SetHistoryCapacity(static_cast<int32>(InValue)); }
                            })
                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(ck_intent_debugger_timeline::PadS, 0.0f)
                    [
                        SNew(STextBlock)
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                            .Text_Lambda([WeakPanel]()
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                                { return FText::GetEmpty(); }

                                return Panel->_ViewModel->Get_IsLive()
                                    ? FText::FromString(TEXT("LIVE"))
                                    : FText::FromString(ck::Format_UE(TEXT("SCRUB @ {}"),
                                        Panel->_ViewModel->Get_ScrubFrame()));
                            })
                            .ColorAndOpacity_Lambda([WeakPanel]()
                            {
                                const auto Panel = WeakPanel.Pin();
                                const auto IsLive = NOT Panel.IsValid()
                                    || NOT Panel->_ViewModel.IsValid()
                                    || Panel->_ViewModel->Get_IsLive();

                                return FSlateColor{IsLive ? CkStyle::Ok() : CkStyle::Warn()};
                            })
                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Go live")))
                            .IsEnabled_Lambda([WeakPanel]()
                            {
                                const auto Panel = WeakPanel.Pin();
                                return Panel.IsValid()
                                    && Panel->_ViewModel.IsValid()
                                    && NOT Panel->_ViewModel->Get_IsLive();
                            })
                            .OnClicked_Lambda([WeakPanel]()
                            {
                                const auto Panel = WeakPanel.Pin();
                                if (Panel.IsValid() && Panel->_ViewModel.IsValid())
                                {
                                    Panel->_ViewModel->Set_ScrubFrame(INDEX_NONE);

                                    // Scrubbing parked the window; going live re-glues it to the newest frame.
                                    if (Panel->_Timeline.IsValid())
                                    { Panel->_Timeline->Refollow_Live(); }
                                }

                                return FReply::Handled();
                            })
                    ]
                ]
        ]

        + SVerticalBox::Slot().FillHeight(1.0f).Padding(ck_intent_debugger_timeline::PadM, ck_intent_debugger_timeline::PadS)
        [
            SAssignNew(_TimelineHost, SBox)
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_TimelineDock::
    Reset_ForWorldChange()
    -> void
{
    _LaneLabels.Reset();
    _StructureHash = 0;
    _Timeline.Reset();

    if (_TimelineHost.IsValid())
    { _TimelineHost->SetContent(SNullWidget::NullWidget); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_TimelineDock::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _TimelineHost.IsValid())
    { return; }

    const auto* Source = _ViewModel->TryGet_SelectedSource();
    if (Source == nullptr)
    {
        Reset_ForWorldChange();
        return;
    }

    const auto* SelectedLayer = _ViewModel->TryGet_SelectedLayer();
    const auto Labels = ck_intent_debugger_timeline::Get_LaneLabels(*Source, SelectedLayer);

    auto Hash = GetTypeHash(Labels.Num());
    for (const auto& Label : Labels)
    { Hash = HashCombine(Hash, GetTypeHash(Label)); }

    // Lane labels are a construction argument, so a changed lane SET is a context change — the one path on which
    // rebuilding a widget tree is allowed. Everything else flows through Set_Content. NEVER mid-drag: the drag
    // holds mouse capture on the widget a rebuild would destroy, killing the scrub under the user's finger —
    // the hash stays stale, so the rebuild simply lands on the next refresh after the drag ends.
    const auto IsInteracting = _Timeline.IsValid() && _Timeline->Get_IsInteracting();
    const auto NeedsRebuild = (Hash != _StructureHash || NOT _Timeline.IsValid()) && NOT IsInteracting;

    // Button lanes come and go with the ring, so a rebuild is routine — the user's pan/zoom must survive it.
    // Applied AFTER Set_Content below: a fresh widget's data range is 0..1 until then, and Set_View clamps.
    // Follow is carried as STATE, not re-derived from geometry: a parked window that still touches the live
    // edge (the common case right after a scrub) must not silently resume sliding.
    const auto CarriedViewStart = _Timeline.IsValid() ? _Timeline->Get_ViewStart() : 0.0;
    const auto CarriedViewDuration = _Timeline.IsValid() ? _Timeline->Get_ViewDuration() : 0.0;
    const auto CarriedFollow = NOT _Timeline.IsValid() || _Timeline->Get_IsFollowingLive();

    if (NeedsRebuild)
    {
        _StructureHash = Hash;
        _LaneLabels = Labels;

        const auto WeakPanel = TWeakPtr<SCkIntentDebugger_TimelineDock>(SharedThis(this));

        _TimelineHost->SetContent(
            SAssignNew(_Timeline, SCkDebug_EventTimeline)
                .LaneLabels(_LaneLabels)
                .DesiredHeight(FMath::Max(
                    ck_intent_debugger_timeline::MinTimelineHeight,
                    static_cast<float>(_LaneLabels.Num()) * ck_intent_debugger_timeline::LaneHeight))
                .AllowPanZoom(true)
                .InitialViewDuration(ck_intent_debugger_timeline::InitialViewFrames)
                .SelectedId_Lambda([WeakPanel]() -> int32
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                    { return INDEX_NONE; }

                    return Panel->_ViewModel->Get_ScrubFrame();
                })
                .OnEventSelected_Lambda([WeakPanel](int32 InSelectionId)
                {
                    const auto Panel = WeakPanel.Pin();
                    if (Panel.IsValid() && Panel->_ViewModel.IsValid())
                    { Panel->_ViewModel->Set_ScrubFrame(InSelectionId); }
                })
                .OnScrubbed_Lambda([WeakPanel](double InTime)
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid())
                    { return; }

                    const auto* ScrubSource = Panel->_ViewModel->TryGet_SelectedSource();
                    if (ScrubSource == nullptr || ScrubSource->Frames.IsEmpty())
                    { return; }

                    Panel->_ViewModel->Set_ScrubFrame(FMath::Clamp(
                        FMath::FloorToInt32(InTime),
                        ScrubSource->Frames[0].FrameIndex,
                        ScrubSource->Frames.Last().FrameIndex));
                })
                .OnFormatTick_Lambda([](double InTime)
                {
                    return ck::Format_UE(TEXT("f{}"), FMath::RoundToInt32(InTime));
                })
                .CursorTime_Lambda([WeakPanel]() -> TOptional<double>
                {
                    const auto Panel = WeakPanel.Pin();
                    if (NOT Panel.IsValid() || NOT Panel->_ViewModel.IsValid() || Panel->_ViewModel->Get_IsLive())
                    { return {}; }

                    // Mid-frame so the cursor sits inside the scrubbed frame's cell, not on its boundary.
                    return static_cast<double>(Panel->_ViewModel->Get_ScrubFrame()) + 0.5;
                }));
    }

    DoRebuild_Timeline();

    if (NeedsRebuild && CarriedViewDuration > 0.0)
    {
        _Timeline->Set_View(CarriedViewStart, CarriedViewDuration);
        _Timeline->Set_FollowLive(CarriedFollow);
    }

    // Scrub mode IS parked mode — enforced every refresh so no rebuild, zoom or clamp can quietly re-attach
    // the window while the user is reading a past frame.
    if (_ViewModel->Get_IsLive() == false && _Timeline.IsValid())
    { _Timeline->Set_FollowLive(false); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_TimelineDock::
    DoRebuild_Timeline()
    -> void
{
    if (NOT _Timeline.IsValid() || NOT _ViewModel.IsValid())
    { return; }

    const auto* Source = _ViewModel->TryGet_SelectedSource();
    if (Source == nullptr || Source->Frames.IsEmpty())
    {
        _Timeline->Set_Content(0.0, 1.0, {}, {});
        return;
    }

    const auto FirstFrame = static_cast<double>(Source->Frames[0].FrameIndex);
    const auto LastFrame = static_cast<double>(Source->Frames.Last().FrameIndex);
    const auto LatestFrame = Source->Frames.Last().FrameIndex;

    auto Events = TArray<FCkDebug_TimelineEvent>{};
    auto Spans = TArray<FCkDebug_TimelineSpan>{};

    // ---- Layer lanes: the frames each layer ended a routing walk on ----

    for (auto LaneIndex = 0; LaneIndex < Source->Layers.Num(); ++LaneIndex)
    {
        const auto& Layer = Source->Layers[LaneIndex];

        auto RunStart = static_cast<int32>(INDEX_NONE);
        auto RunEnd = static_cast<int32>(INDEX_NONE);

        const auto FlushRun = [&]()
        {
            if (RunStart == INDEX_NONE)
            { return; }

            auto Span = FCkDebug_TimelineSpan{};
            Span.LaneIndex = LaneIndex;
            Span.StartSeconds = static_cast<double>(RunStart);
            Span.EndSeconds = static_cast<double>(RunEnd) + 1.0;
            Span.Color = CkStyle::Accent();
            Span.Tooltip = ck::Format_UE(TEXT("{} consumed frames {}..{}"),
                Layer.Get_StackLabel(), RunStart, RunEnd);

            Spans.Add(MoveTemp(Span));

            RunStart = INDEX_NONE;
            RunEnd = INDEX_NONE;
        };

        for (const auto& Frame : Source->Frames)
        {
            if (NOT Frame.ConsumingLayerPriorities.Contains(Layer.Priority))
            {
                FlushRun();
                continue;
            }

            if (RunStart == INDEX_NONE)
            { RunStart = Frame.FrameIndex; }

            RunEnd = Frame.FrameIndex;
        }

        FlushRun();
    }

    // ---- Button lanes: held runs + press markers, straight off the record ring ----
    // This is where a grammar-free press (block) is visible: it has no intent lane, but its button lane
    // carries every press and hold the record saw.

    const auto ButtonLabels = ck_intent_debugger_timeline::Get_ButtonLaneLabels(*Source);
    const auto ButtonLaneBase = Source->Layers.Num();

    for (auto ButtonIndex = 0; ButtonIndex < ButtonLabels.Num(); ++ButtonIndex)
    {
        const auto& Label = ButtonLabels[ButtonIndex];
        const auto LaneIndex = ButtonLaneBase + ButtonIndex;

        auto RunStart = int32{INDEX_NONE};
        auto RunEnd = int32{INDEX_NONE};

        const auto FlushRun = [&]()
        {
            if (RunStart == INDEX_NONE)
            { return; }

            auto Span = FCkDebug_TimelineSpan{};
            Span.LaneIndex = LaneIndex;
            Span.StartSeconds = static_cast<double>(RunStart);
            Span.EndSeconds = static_cast<double>(RunEnd) + 1.0;
            Span.Color = CkStyle::AccentDim();
            Span.Tooltip = ck::Format_UE(TEXT("{} held frames {}..{}"), Label, RunStart, RunEnd);

            Spans.Add(MoveTemp(Span));

            RunStart = INDEX_NONE;
            RunEnd = INDEX_NONE;
        };

        for (const auto& Frame : Source->Frames)
        {
            const auto* Button = Frame.Held.FindByPredicate(
                [&Label](const FCkIntentDebugger_ButtonState& InButton) { return InButton.Label == Label; });

            if (Button == nullptr)
            {
                FlushRun();
                continue;
            }

            if (RunStart == INDEX_NONE)
            { RunStart = Frame.FrameIndex; }

            RunEnd = Frame.FrameIndex;

            if (Button->WentDown)
            {
                auto Marker = FCkDebug_TimelineEvent{};
                Marker.LaneIndex = LaneIndex;
                Marker.TimeSeconds = static_cast<double>(Frame.FrameIndex);
                Marker.Shape = ECkDebug_TimelineMarker::Square;
                Marker.Color = CkStyle::Ok();
                Marker.SelectionId = Frame.FrameIndex;
                Marker.Tooltip = ck::Format_UE(TEXT("{} pressed on frame {}"), Label, Frame.FrameIndex);

                Events.Add(MoveTemp(Marker));
            }
        }

        FlushRun();
    }

    // ---- Intent lanes: one span per witnessed phase ----

    const auto* SelectedLayer = _ViewModel->TryGet_SelectedLayer();
    if (SelectedLayer != nullptr)
    {
        const auto IntentLaneBase = ButtonLaneBase + ButtonLabels.Num();

        auto LaneByName = TMap<FName, int32>{};
        for (auto Index = 0; Index < SelectedLayer->Intents.Num(); ++Index)
        { LaneByName.Add(SelectedLayer->Intents[Index].Name, IntentLaneBase + Index); }

        for (const auto& Event : _ViewModel->Get_PhaseEvents())
        {
            if (Event.LayerPriority != SelectedLayer->Priority || Event.Phase == ECk_Intent_Phase::Idle)
            { continue; }

            const auto* Lane = LaneByName.Find(Event.IntentName);
            if (Lane == nullptr)
            { continue; }

            const auto IsTerminal =
                Event.Phase == ECk_Intent_Phase::Completed || Event.Phase == ECk_Intent_Phase::Failed;

            const auto EndFrame = Event.Get_IsOpen() ? LatestFrame : Event.EndFrame;

            const auto DrawnEndFrame = IsTerminal
                ? FMath::Min(EndFrame, Event.StartFrame + ck_intent_debugger_timeline::TerminalPhasePulseFrames)
                : EndFrame;

            auto Span = FCkDebug_TimelineSpan{};
            Span.LaneIndex = *Lane;
            Span.StartSeconds = static_cast<double>(Event.StartFrame);
            Span.EndSeconds = static_cast<double>(DrawnEndFrame) + 1.0;
            Span.Color = ck::intent_debugger::Get_PhaseColor(Event.Phase);
            Span.Tooltip = ck::Format_UE(TEXT("{} · {} · frames {}..{}"),
                Event.IntentName, ck::intent_debugger::Get_Label(Event.Phase), Event.StartFrame, EndFrame);

            Spans.Add(MoveTemp(Span));

            if (Event.Phase != ECk_Intent_Phase::Completed && Event.Phase != ECk_Intent_Phase::Failed)
            { continue; }

            auto Marker = FCkDebug_TimelineEvent{};
            Marker.LaneIndex = *Lane;
            Marker.TimeSeconds = static_cast<double>(Event.StartFrame);
            Marker.Shape = ECkDebug_TimelineMarker::Diamond;
            Marker.Color = ck::intent_debugger::Get_PhaseColor(Event.Phase);
            Marker.SelectionId = Event.StartFrame;
            Marker.Tooltip = ck::Format_UE(TEXT("{} {} on frame {}"),
                Event.IntentName, ck::intent_debugger::Get_Label(Event.Phase), Event.StartFrame);

            Events.Add(MoveTemp(Marker));
        }

        // ---- Blocked-by lane: the pending episodes, at the press frame each opened on ----

        const auto BlockedLane = IntentLaneBase + SelectedLayer->Intents.Num();

        for (const auto& Episode : SelectedLayer->Episodes)
        {
            auto Candidates = TArray<FString>{};
            for (const auto& Name : Episode.CandidateNames)
            { Candidates.Add(Name.ToString()); }

            auto Marker = FCkDebug_TimelineEvent{};
            Marker.LaneIndex = BlockedLane;
            Marker.TimeSeconds = static_cast<double>(Episode.PressFrame);
            Marker.Shape = ECkDebug_TimelineMarker::Diamond;
            Marker.Color = CkStyle::Warn();
            Marker.SelectionId = Episode.PressFrame;
            Marker.SideLabel = Episode.ButtonLabel;
            Marker.Tooltip = ck::Format_UE(
                TEXT("{} pressed on frame {} — held open by {}\nwaiting on: {}"),
                Episode.ButtonLabel,
                Episode.PressFrame,
                Episode.Get_CauseLabel(),
                Candidates.IsEmpty() ? FString{TEXT("nothing")} : FString::Join(Candidates, TEXT(", ")));

            Events.Add(MoveTemp(Marker));

            auto Span = FCkDebug_TimelineSpan{};
            Span.LaneIndex = BlockedLane;
            Span.StartSeconds = static_cast<double>(Episode.PressFrame);
            Span.EndSeconds = static_cast<double>(LatestFrame) + 1.0;
            Span.Color = CkStyle::WarnDim();
            Span.Tooltip = Marker.Tooltip;

            Spans.Add(MoveTemp(Span));
        }
    }

    _Timeline->Set_Content(FirstFrame, LastFrame + 1.0, MoveTemp(Events), MoveTemp(Spans));
}

// --------------------------------------------------------------------------------------------------------------------
