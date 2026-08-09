#include "CkIntentDebugger/Window/SCkIntentDebugger_TimelineDock.h"

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"
#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"
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
    constexpr auto LaneHeight = 16.0f;
    constexpr auto MinTimelineHeight = 96.0f;

    auto
        Get_LaneLabels(
            const FCkIntentDebugger_SourceSnapshot& InSource,
            const FCkIntentDebugger_LayerRow* InSelectedLayer)
        -> TArray<FString>
    {
        auto Labels = TArray<FString>{};

        for (const auto& Layer : InSource.Layers)
        { Labels.Add(ck::Format_UE(TEXT("L{}"), Layer.Priority)); }

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
                .SubText(FText::FromString(TEXT("axis is the sampler's logic-frame counter · click a marker to scrub")))
                .RightContent()
                [
                    SNew(SHorizontalBox)

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
                                { Panel->_ViewModel->Set_ScrubFrame(INDEX_NONE); }

                                return FReply::Handled();
                            })
                    ]
                ]
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(ck_intent_debugger_timeline::PadM, ck_intent_debugger_timeline::PadS)
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
    // rebuilding a widget tree is allowed. Everything else flows through Set_Content.
    if (Hash != _StructureHash || NOT _Timeline.IsValid())
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
                }));
    }

    DoRebuild_Timeline();
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

    // ---- Intent lanes: one span per witnessed phase ----

    const auto* SelectedLayer = _ViewModel->TryGet_SelectedLayer();
    if (SelectedLayer != nullptr)
    {
        const auto IntentLaneBase = Source->Layers.Num();

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

            const auto EndFrame = Event.Get_IsOpen() ? LatestFrame : Event.EndFrame;

            auto Span = FCkDebug_TimelineSpan{};
            Span.LaneIndex = *Lane;
            Span.StartSeconds = static_cast<double>(Event.StartFrame);
            Span.EndSeconds = static_cast<double>(EndFrame) + 1.0;
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
