#include "CkGoapDebugger/Window/SCkGoapDebugger_TimelineDock.h"

#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_HistoryModel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_goap_debugger_timeline_dock
{
    auto Tokens() -> FCkUiView::FTokens
    {
        return {{TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
                {TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM)},
                {TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL)},
                {TEXT("--timeline-text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex()},
                {TEXT("--timeline-text-mute"), TEXT("#") + CkStyle::TextMute().ToFColorSRGB().ToHex()}};
    }

    constexpr auto Lane_Ws     = 0;
    constexpr auto Lane_Replan = 1;
    constexpr auto Lane_Active = 2;

    auto Get_IsReplanKind(ECkGoapDebugger_HistoryEventKind InKind) -> bool
    {
        return InKind == ECkGoapDebugger_HistoryEventKind::Replanned
            || InKind == ECkGoapDebugger_HistoryEventKind::PlanFound
            || InKind == ECkGoapDebugger_HistoryEventKind::PlanFailed;
    }

    auto Get_OriginLabel(ECk_Goap_ReplanOrigin InOrigin) -> FString
    {
        switch (InOrigin)
        {
            case ECk_Goap_ReplanOrigin::PlanOnStart:            return TEXT("plan-on-start");
            case ECk_Goap_ReplanOrigin::Explicit:               return TEXT("explicit Request_Plan");
            case ECk_Goap_ReplanOrigin::WorldStateDirty:        return TEXT("world-state change");
            case ECk_Goap_ReplanOrigin::CostDirty:              return TEXT("cost change");
            case ECk_Goap_ReplanOrigin::WorldStateAndCostDirty: return TEXT("world-state + cost change");
            case ECk_Goap_ReplanOrigin::GoalChanged:            return TEXT("goal changed");
            case ECk_Goap_ReplanOrigin::CatalogChanged:         return TEXT("catalog changed");
            default:                                            return TEXT("unknown");
        }
    }

    // Event text with the action class name run through the shared name-depth
    // tuner. Titles are baked at record time with the FULL class name; the
    // event carries ActionClassName so display sites can re-shorten live.
    auto Get_DisplayText(const FCkGoapDebugger_HistoryEvent& InEvent, const FString& InText, int32 InDepth) -> FString
    {
        if (InEvent.ActionClassName.IsEmpty()) { return InText; }
        auto Result = InText;
        Result.ReplaceInline(
            *InEvent.ActionClassName,
            *SCkDebug_NameLabel::Get_ShortName(InEvent.ActionClassName, InDepth));
        return Result;
    }

    auto LeafOfTagString(const FString& InFull) -> FString
    {
        auto LastDot = int32{INDEX_NONE};
        return InFull.FindLastChar(TEXT('.'), LastDot) ? InFull.RightChop(LastDot + 1) : InFull;
    }
}

// ====================================================================================================================
// CONSTRUCT / LIFECYCLE
// ====================================================================================================================

auto
    SCkGoapDebugger_TimelineDock::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;
    _PauseOnReplan = InArgs._PauseOnReplan;
    _PauseOnPlanFailed = InArgs._PauseOnPlanFailed;
    _PauseExecution = InArgs._PauseExecution;

    // Preserve the specialized timeline as one native unit. At narrow dock
    // widths its fixed information density remains reachable by scrolling the
    // whole body horizontally instead of compressing or restacking controls.
    _NativeContent = SNew(SScrollBox)
            .Orientation(Orient_Horizontal)
            .ScrollBarVisibility(EVisibility::Visible)
            + SScrollBox::Slot()
                .FillSize(1.0f)
                .MinSize(800.0f)
            [
                SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SCkDebug_SectionHeader)
                                .Label(FText::FromString(TEXT("Timeline")))
                                .SubText(FText::FromString(TEXT("replans · world-state changes · activations")))
                                .Underline(true)
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
                        [
                            SAssignNew(_Timeline, SCkDebug_EventTimeline)
                                .LaneLabels({TEXT("WS"), TEXT("REPLAN"), TEXT("ACTIVE")})
                                .DesiredHeight(96.0f)
                                .SelectedId_Lambda([this]() -> int32
                                {
                                    if (NOT _ViewModel.IsValid()) { return INDEX_NONE; }
                                    return _ViewModel->GetScrubEventIndex();
                                })
                                .OnEventSelected(FOnCkDebug_TimelineEventSelected::CreateSP(
                                    this, &SCkGoapDebugger_TimelineDock::HandleScrubTo))
                        ]

                    // Controls — jump-to-replan buttons. Pause-on actions live in the common menu bar.
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, CkStyle::SpaceS))
                        [
                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Jump to replan")))
                                            .Font_Lambda([]() -> FSlateFontInfo
                                            { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                    ]

                                + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SScrollBox)
                                            .Orientation(Orient_Horizontal)
                                            .ScrollBarVisibility(EVisibility::Collapsed)

                                            + SScrollBox::Slot()
                                            [
                                                SAssignNew(_JumpButtons, SHorizontalBox)
                                            ]
                                    ]

                        ]

                    // Diff card | event log.
                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SSplitter)
                                .Orientation(Orient_Horizontal)

                                + SSplitter::Slot()
                                    .Value(0.45f)
                                    [
                                        SAssignNew(_DiffHost, SBox)
                                            .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
                                    ]

                                + SSplitter::Slot()
                                    .Value(0.55f)
                                    [
                                        SNew(SScrollBox)
                                            .Orientation(Orient_Vertical)

                                            + SScrollBox::Slot()
                                            .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
                                            [
                                                SAssignNew(_EventLog, SVerticalBox)
                                            ]
                                    ]
                        ]
                    ]
    ];

    ChildSlot
    [
        SAssignNew(_ContentHost, SBox)
        [
            _NativeContent.ToSharedRef()
        ]
    ];

    TryActivateAuthoredView();
    RefreshFromViewModel();
}

auto
    SCkGoapDebugger_TimelineDock::
    Reset_ForWorldChange()
    -> void
{
    ++_AuthoredGeneration;
    ClearAuthoredNativePort();
    _AuthoredView.Reset();
    _TimelineBodyPort.Reset();
    ActivateNativeFallback();

    ClearHistoryPresentation();
}

auto SCkGoapDebugger_TimelineDock::ClearHistoryPresentation() -> void
{
    _SeenEventCount = 0;
    _LastHash = 0;

    if (_Timeline.IsValid())
    { _Timeline->Set_Content(0.0, 1.0, {}, {}); }
    if (_JumpButtons.IsValid())
    { _JumpButtons->ClearChildren(); }
    if (_DiffHost.IsValid())
    { _DiffHost->SetContent(SNullWidget::NullWidget); }
    if (_EventLog.IsValid())
    { _EventLog->ClearChildren(); }
}

auto SCkGoapDebugger_TimelineDock::ClearAuthoredNativePort() -> void
{
    if (_TimelineBodyPort.IsValid())
    {
        _TimelineBodyPort->SetContent(SNullWidget::NullWidget);
    }
}

auto SCkGoapDebugger_TimelineDock::ActivateNativeFallback() -> void
{
    if (_ContentHost.IsValid() && _NativeContent.IsValid())
    {
        _ContentHost->SetContent(_NativeContent.ToSharedRef());
    }
}

auto SCkGoapDebugger_TimelineDock::TryActivateAuthoredView() -> void
{
    using namespace ck_goap_debugger_timeline_dock;

    if (_AuthoredView.IsValid() || !_ContentHost.IsValid() || !_NativeContent.IsValid())
    {
        return;
    }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (!RegistryResult.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(RegistryResult.Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid())
    {
        _AuthoredLoadFailure = TEXT("CkDebugger plugin is unavailable.");
        ActivateNativeFallback();
        return;
    }

    // Do not detach the visible fallback until candidate admission succeeds.
    _TimelineBodyPort = SNew(SBox)
    [
        SNullWidget::NullWidget
    ];

    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(TEXT("timeline-body"), _TimelineBodyPort);

    const TWeakPtr<SCkGoapDebugger_TimelineDock> WeakDock{SharedThis(this)};
    const uint64 Generation = ++_AuthoredGeneration;
    const auto GetSelectedEntity = [WeakDock, Generation]() -> FCk_Handle
    {
        const TSharedPtr<SCkGoapDebugger_TimelineDock> Dock = WeakDock.Pin();
        return Dock.IsValid() && Dock->_AuthoredGeneration == Generation && Dock->_ViewModel.IsValid()
            ? Dock->_ViewModel->GetSelectedEntity()
            : FCk_Handle{};
    };

    FCkUiView::FDataBindings Data;
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakDock, Generation]()
    {
        const TSharedPtr<SCkGoapDebugger_TimelineDock> Dock = WeakDock.Pin();
        return Dock.IsValid() && Dock->_AuthoredGeneration == Generation && Dock->_AuthoredView.IsValid();
    });
    Data.Visibility.Add(TEXT("timeline-empty-visible"), TAttribute<bool>::CreateLambda([GetSelectedEntity]()
    {
        return ck::Is_NOT_Valid(GetSelectedEntity());
    }));
    Data.Visibility.Add(TEXT("timeline-selected-visible"), TAttribute<bool>::CreateLambda([GetSelectedEntity]()
    {
        return !ck::Is_NOT_Valid(GetSelectedEntity());
    }));
    Data.Text.Add(TEXT("timeline-empty"), TAttribute<FText>::CreateLambda([]()
    {
        return FText::FromString(TEXT("Select an agent to inspect its GOAP event timeline."));
    }));
    Data.Text.Add(TEXT("timeline-title"), TAttribute<FText>::CreateLambda([]()
    {
        return FText::FromString(TEXT("Timeline"));
    }));
    Data.Text.Add(TEXT("timeline-status"), TAttribute<FText>::CreateLambda([GetSelectedEntity]()
    {
        const FCk_Handle Entity = GetSelectedEntity();
        if (ck::Is_NOT_Valid(Entity))
        {
            return FText::GetEmpty();
        }
        return FText::FromString(FString::Printf(TEXT("%d events"), FCkGoapDebugger_DataCollector::GetHistory(Entity).Num()));
    }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(MoveTemp(NativeBindings), FCkUiView::FActions{}, Tokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = Candidate->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Directory, TEXT("GoapDebuggerTimeline.ui.html")),
        FPaths::Combine(Directory, TEXT("GoapDebuggerTimeline.ui.css")));
    Candidate->PollFiles();
    if (!Candidate->GetLastResult().Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        ClearAuthoredNativePort();
        ActivateNativeFallback();
        return;
    }

    _AuthoredLoadFailure.Reset();
    _TimelineBodyPort->SetContent(_NativeContent.ToSharedRef());
    _AuthoredView = Candidate;
    _ContentHost->SetContent(Main);
}

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_TimelineDock::
    RefreshFromViewModel()
    -> void
{
    if (_AuthoredView.IsValid())
    {
        _AuthoredView->PollFiles(ck_goap_debugger_timeline_dock::Tokens());
        if (!_AuthoredView->GetLastResult().Succeeded)
        {
            // Rejected live admission leaves the accepted tree and retained timeline mounted.
            _AuthoredLoadFailure = FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n"));
        }
        else
        {
            _AuthoredLoadFailure.Reset();
        }
    }
    else
    {
        TryActivateAuthoredView();
    }

    if (NOT _ViewModel.IsValid()) { return; }

    const auto Entity = _ViewModel->GetSelectedEntity();
    if (ck::Is_NOT_Valid(Entity))
    {
        // Selection is transient. Keep the admitted authored shell and its
        // retained native port alive so its empty state can render; only an
        // actual world teardown releases that state through Reset_ForWorldChange.
        ClearHistoryPresentation();
        return;
    }

    const auto& History = FCkGoapDebugger_DataCollector::GetHistory(Entity);

    DoMaybePauseOnNewEvents(History);

    // Hash — event count + scrub index + last event frame. Selection renders
    // live inside the timeline; the log rows bind their own highlight.
    auto NewHash = uint32{0};
    NewHash = HashCombine(NewHash, GetTypeHash(Entity));
    NewHash = HashCombine(NewHash, ::GetTypeHash(History.Num()));
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->GetScrubEventIndex()));
    if (History.Num() > 0)
    { NewHash = HashCombine(NewHash, ::GetTypeHash(History.Last().FrameNumber)); }
    // Event-log + diff-card action names run through the shared name-depth tuner.
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth()));

    if (NewHash == _LastHash) { return; }
    _LastHash = NewHash;

    DoRebuildTimeline(History);
    DoRebuildJumpButtons(History);
    DoRebuildDiffCard(History);
    DoRebuildEventLog(History);
}

// ====================================================================================================================
// REBUILD — TIMELINE
// ====================================================================================================================

auto
    SCkGoapDebugger_TimelineDock::
    DoRebuildTimeline(
        const TArray<FCkGoapDebugger_HistoryEvent>& InHistory)
    -> void
{
    using namespace ck_goap_debugger_timeline_dock;

    if (NOT _Timeline.IsValid()) { return; }

    auto Events = TArray<FCkDebug_TimelineEvent>{};
    auto Spans  = TArray<FCkDebug_TimelineSpan>{};

    auto TimeMin = TNumericLimits<double>::Max();
    auto TimeMax = TNumericLimits<double>::Lowest();

    auto LastActivationStart = -1.0;

    for (auto Index = 0; Index < InHistory.Num(); ++Index)
    {
        const auto& Event = InHistory[Index];
        TimeMin = FMath::Min(TimeMin, Event.WorldTimeSeconds);
        TimeMax = FMath::Max(TimeMax, Event.WorldTimeSeconds);

        switch (Event.Kind)
        {
            case ECkGoapDebugger_HistoryEventKind::WorldStateChanged:
            {
                auto Marker = FCkDebug_TimelineEvent{};
                Marker.LaneIndex = Lane_Ws;
                Marker.TimeSeconds = Event.WorldTimeSeconds;
                Marker.Shape = ECkDebug_TimelineMarker::Square;
                Marker.Color = CkStyle::Warn();
                Marker.Tooltip = FString::Printf(TEXT("%s\n%s"), *Event.Title, *Event.Meta);
                Events.Add(MoveTemp(Marker));
                break;
            }
            case ECkGoapDebugger_HistoryEventKind::Replanned:
            case ECkGoapDebugger_HistoryEventKind::PlanFound:
            case ECkGoapDebugger_HistoryEventKind::PlanFailed:
            {
                auto Marker = FCkDebug_TimelineEvent{};
                Marker.LaneIndex = Lane_Replan;
                Marker.TimeSeconds = Event.WorldTimeSeconds;
                Marker.Shape = ECkDebug_TimelineMarker::Diamond;
                Marker.Color = Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFailed
                    ? CkStyle::Err()
                    : CkStyle::TextDim();
                Marker.Tooltip = FString::Printf(TEXT("%s\n%s"), *Event.Title, *Event.Meta);
                Marker.SelectionId = Index;

                // Coalesce note — multiple WS changes folded into one replan.
                if (Event.Kind == ECkGoapDebugger_HistoryEventKind::Replanned &&
                    Event.CauseAtEvent.Get_ChangedKeys().Num() > 1)
                { Marker.SideLabel = FString::Printf(TEXT("\x00D7%d"), Event.CauseAtEvent.Get_ChangedKeys().Num()); }

                Events.Add(MoveTemp(Marker));
                break;
            }
            case ECkGoapDebugger_HistoryEventKind::ChainActivated:
            case ECkGoapDebugger_HistoryEventKind::ActionActivated:
            {
                if (LastActivationStart < 0.0)
                { LastActivationStart = Event.WorldTimeSeconds; }

                auto Marker = FCkDebug_TimelineEvent{};
                Marker.LaneIndex = Lane_Active;
                Marker.TimeSeconds = Event.WorldTimeSeconds;
                Marker.Shape = ECkDebug_TimelineMarker::Square;
                Marker.Color = CkStyle::Ok();
                Marker.Tooltip = Event.Title;
                Events.Add(MoveTemp(Marker));
                break;
            }
            case ECkGoapDebugger_HistoryEventKind::ActionDeactivated:
            case ECkGoapDebugger_HistoryEventKind::ChainReset:
            {
                if (LastActivationStart >= 0.0)
                {
                    auto Span = FCkDebug_TimelineSpan{};
                    Span.LaneIndex = Lane_Active;
                    Span.StartSeconds = LastActivationStart;
                    Span.EndSeconds = Event.WorldTimeSeconds;
                    Span.Color = CkStyle::OkDim();
                    Span.Tooltip = TEXT("active chain span");
                    Spans.Add(MoveTemp(Span));
                    LastActivationStart = -1.0;
                }
                break;
            }
            default:
                break;
        }
    }

    if (InHistory.Num() == 0)
    {
        _Timeline->Set_Content(0.0, 1.0, {}, {});
        return;
    }

    // Open activation span runs to the window's right edge.
    if (LastActivationStart >= 0.0)
    {
        auto Span = FCkDebug_TimelineSpan{};
        Span.LaneIndex = Lane_Active;
        Span.StartSeconds = LastActivationStart;
        Span.EndSeconds = TimeMax;
        Span.Color = CkStyle::OkDim();
        Span.Tooltip = TEXT("active chain (ongoing)");
        Spans.Add(MoveTemp(Span));
    }

    // Pad the window a touch so edge markers don't sit on the border.
    const auto Pad = FMath::Max(0.5, (TimeMax - TimeMin) * 0.03);
    _Timeline->Set_Content(TimeMin - Pad, TimeMax + Pad, MoveTemp(Events), MoveTemp(Spans));
}

// ====================================================================================================================
// REBUILD — JUMP BUTTONS
// ====================================================================================================================

auto
    SCkGoapDebugger_TimelineDock::
    DoRebuildJumpButtons(
        const TArray<FCkGoapDebugger_HistoryEvent>& InHistory)
    -> void
{
    using namespace ck_goap_debugger_timeline_dock;

    if (NOT _JumpButtons.IsValid()) { return; }
    _JumpButtons->ClearChildren();

    auto ReplanNumber = 0;
    for (auto Index = 0; Index < InHistory.Num(); ++Index)
    {
        const auto& Event = InHistory[Index];
        if (NOT Get_IsReplanKind(Event.Kind)) { continue; }
        ++ReplanNumber;

        const auto EventIndex = Index;
        _JumpButtons->AddSlot()
            .AutoWidth()
            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f))
            [
                SNew(SButton)
                    .ContentPadding(FMargin(CkStyle::SpaceS, 1.0f))
                    .ToolTipText(FText::FromString(Event.Title))
                    .OnClicked_Lambda([this, EventIndex]() -> FReply
                    {
                        HandleScrubTo(EventIndex);
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(
                                TEXT("#%d · %.1fs"), ReplanNumber, Event.WorldTimeSeconds)))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity_Lambda([this, EventIndex]() -> FSlateColor
                            {
                                const auto Selected = _ViewModel.IsValid() &&
                                    _ViewModel->GetScrubEventIndex() == EventIndex;
                                return FSlateColor(Selected ? CkStyle::Accent() : CkStyle::TextDim());
                            })
                    ]
            ];
    }
}

// ====================================================================================================================
// REBUILD — DIFF CARD
// ====================================================================================================================

auto
    SCkGoapDebugger_TimelineDock::
    DoRebuildDiffCard(
        const TArray<FCkGoapDebugger_HistoryEvent>& InHistory)
    -> void
{
    using namespace ck_goap_debugger_timeline_dock;

    if (NOT _DiffHost.IsValid()) { return; }

    // Selected replan (scrub) or the most recent replan-kind event.
    auto FocusIndex = _ViewModel->GetScrubEventIndex();
    if (NOT InHistory.IsValidIndex(FocusIndex) || NOT Get_IsReplanKind(InHistory[FocusIndex].Kind))
    {
        FocusIndex = INDEX_NONE;
        for (auto Index = InHistory.Num() - 1; Index >= 0; --Index)
        {
            if (Get_IsReplanKind(InHistory[Index].Kind)) { FocusIndex = Index; break; }
        }
    }

    if (FocusIndex == INDEX_NONE)
    {
        _DiffHost->SetContent(
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("No replans yet — the diff card fills in after the first plan.")))
                .Font_Lambda([]() -> FSlateFontInfo
                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim())));
        return;
    }

    const auto& Focus = InHistory[FocusIndex];
    const auto DiffNameDepth = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;

    // Trigger line — cause (origin + changed keys + coalescing) when present.
    auto TriggerText = FString{};
    if (Focus.Kind == ECkGoapDebugger_HistoryEventKind::Replanned)
    {
        const auto& Cause = Focus.CauseAtEvent;
        auto KeysText = FString{};
        for (auto Index = 0; Index < Cause.Get_ChangedKeys().Num(); ++Index)
        {
            const auto& Change = Cause.Get_ChangedKeys()[Index];
            if (Index > 0) { KeysText += TEXT(" · "); }
            KeysText += FString::Printf(TEXT("%s → %s"),
                *LeafOfTagString(Change.Get_Key().ToString()),
                Change.Get_NewValue() ? TEXT("TRUE") : TEXT("FALSE"));
        }
        TriggerText = FString::Printf(TEXT("Replan #%d — trigger: %s · origin: %s"),
            Cause.Get_AttemptNumber(),
            KeysText.IsEmpty() ? TEXT("(none recorded)") : *KeysText,
            *Get_OriginLabel(Cause.Get_Origin()));
        if (Cause.Get_ChangedKeys().Num() > 1)
        {
            TriggerText += FString::Printf(TEXT(" · %d changes coalesced into one replan"),
                Cause.Get_ChangedKeys().Num());
        }
    }
    else
    {
        TriggerText = FString::Printf(TEXT("%s · %s"), *Focus.Title, *Focus.Meta);
    }

    // Old plan — the nearest earlier plan-carrying event.
    auto OldText = FString(TEXT("—"));
    for (auto Index = FocusIndex - 1; Index >= 0; --Index)
    {
        const auto& Event = InHistory[Index];
        if (Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFound ||
            Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFailed)
        {
            OldText = FString::Printf(TEXT("%s (%s)"),
                *Get_DisplayText(Event, Event.Title, DiffNameDepth),
                *Get_DisplayText(Event, Event.Meta, DiffNameDepth));
            break;
        }
    }

    const auto NewText = FString::Printf(TEXT("%s %s"),
        *Get_DisplayText(Focus, Focus.Title, DiffNameDepth),
        *Get_DisplayText(Focus, Focus.Meta, DiffNameDepth));

    _DiffHost->SetContent(
        SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
            .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
            [
                SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(TriggerText))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 0.0f, 0.0f, 2.0f))
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(FString::Printf(TEXT("old   %s"), *OldText)))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(FString::Printf(TEXT("new   %s"), *NewText)))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                        ]
            ]);
}

// ====================================================================================================================
// REBUILD — EVENT LOG
// ====================================================================================================================

auto
    SCkGoapDebugger_TimelineDock::
    DoRebuildEventLog(
        const TArray<FCkGoapDebugger_HistoryEvent>& InHistory)
    -> void
{
    using namespace ck_goap_debugger_timeline_dock;

    if (NOT _EventLog.IsValid()) { return; }
    _EventLog->ClearChildren();

    const auto LogNameDepth = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;

    if (InHistory.Num() == 0)
    {
        _EventLog->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(no events yet)")))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
            ];
        return;
    }

    // Newest first — the designer's eye lands on "what just happened".
    for (auto Index = InHistory.Num() - 1; Index >= 0; --Index)
    {
        const auto& Event = InHistory[Index];
        const auto EventIndex = Index;

        auto RowBgAttr = TAttribute<FSlateColor>::Create(
            TAttribute<FSlateColor>::FGetter::CreateLambda(
                [this, EventIndex]() -> FSlateColor
                {
                    const auto Selected = _ViewModel.IsValid() &&
                        _ViewModel->GetScrubEventIndex() == EventIndex;
                    return FSlateColor(Selected ? CkStyle::AccentDim() : FLinearColor::Transparent);
                }));

        _EventLog->AddSlot()
            .AutoHeight()
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(RowBgAttr)
                    .Padding(FMargin(0.0f))
                    [
                        SNew(SButton)
                            .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                            .ContentPadding(ck_goap_debugger_axes::Live_RowDensity(
                                FMargin{CkStyle::SpaceS, 1.0f}))
                            .OnClicked_Lambda([this, EventIndex]() -> FReply
                            {
                                HandleScrubTo(EventIndex);
                                return FReply::Handled();
                            })
                            [
                                SNew(SHorizontalBox)

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(ck_goap_debugger_history_model::Format_Timestamp(Event.WorldTimeSeconds)))
                                                .Font_Lambda([]() -> FSlateFontInfo
                                                { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                        ]

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
                                        [
                                            SNew(SBox)
                                                .MinDesiredWidth(38.0f)
                                                [
                                                    SNew(STextBlock)
                                                        .Text(FText::FromString(ck_goap_debugger_history_model::KindTag(Event.Kind)))
                                                        .Font_Lambda([]() -> FSlateFontInfo
                                                        { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                                                        .ColorAndOpacity(FSlateColor(
                                                            Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFailed
                                                                ? CkStyle::Err()
                                                                : Event.Kind == ECkGoapDebugger_HistoryEventKind::WorldStateChanged
                                                                    ? CkStyle::Warn()
                                                                    : CkStyle::Accent()))
                                                ]
                                        ]

                                    + SHorizontalBox::Slot()
                                        .FillWidth(1.0f)
                                        .VAlign(VAlign_Center)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(Event.Meta.IsEmpty()
                                                    ? Get_DisplayText(Event, Event.Title, LogNameDepth)
                                                    : FString::Printf(TEXT("%s — %s"),
                                                          *Get_DisplayText(Event, Event.Title, LogNameDepth),
                                                          *Get_DisplayText(Event, Event.Meta, LogNameDepth))))
                                                .Font_Lambda([]() -> FSlateFontInfo
                                                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                                        ]
                            ]
                    ]
            ];
    }
}

// ====================================================================================================================
// HANDLERS
// ====================================================================================================================

auto
    SCkGoapDebugger_TimelineDock::
    HandleScrubTo(
        int32 InEventIndex)
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Scrub);
    _ViewModel->SetScrubEventIndex(InEventIndex);
    _ViewModel->Broadcast_Changed();
}

auto
    SCkGoapDebugger_TimelineDock::
    DoMaybePauseOnNewEvents(
        const TArray<FCkGoapDebugger_HistoryEvent>& InHistory)
    -> void
{
    using namespace ck_goap_debugger_timeline_dock;

    // Only new events past the high-water mark, and only while live.
    const auto FirstNew = _SeenEventCount;
    _SeenEventCount = InHistory.Num();

    const auto PauseOnReplan = _PauseOnReplan.Get(false);
    const auto PauseOnPlanFailed = _PauseOnPlanFailed.Get(false);
    if (NOT PauseOnReplan && NOT PauseOnPlanFailed) { return; }
    if (NOT _ViewModel.IsValid() || _ViewModel->GetMode() != FCkGoapDebugger_ViewModel::EMode::Live) { return; }

    for (auto Index = FirstNew; Index < InHistory.Num(); ++Index)
    {
        const auto& Event = InHistory[Index];
        const auto Match =
            (PauseOnReplan && Event.Kind == ECkGoapDebugger_HistoryEventKind::Replanned) ||
            (PauseOnPlanFailed && Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFailed);

        if (NOT Match) { continue; }

        _PauseExecution.ExecuteIfBound();
        break;
    }
}

// ====================================================================================================================
