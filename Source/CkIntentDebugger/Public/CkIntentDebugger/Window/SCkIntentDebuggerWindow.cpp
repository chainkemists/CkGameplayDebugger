#include "CkIntentDebugger/Window/SCkIntentDebuggerWindow.h"

#include "CkIntentDebugger/CkIntentDebugger_Module.h"

#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_DevicesPanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_InputHudControls.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_KeyStatePanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_LayerStackPanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_NearMissPanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_ResolutionPanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_TimelineDock.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkInput/CkInputLayer_Utils.h"
#include "CkInput/CkInputSource_Utils.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/CkUiCollection.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/WidgetPath.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_window
{
    constexpr auto PadS = 4.0f;
    constexpr auto PadM = 8.0f;

    auto AuthoredStyleTokens() -> FCkUiView::FTokens
    {
        const auto Color = [](const FLinearColor& InColor) { return TEXT("#") + InColor.ToFColorSRGB().ToHex(); };
        return {{TEXT("--intent-surface"), Color(CkStyle::Bg2())}, {TEXT("--intent-border"), Color(CkStyle::Border())},
            {TEXT("--intent-text"), Color(CkStyle::Text())}, {TEXT("--intent-muted"), Color(CkStyle::TextMute())},
            {TEXT("--intent-accent"), Color(CkStyle::Accent())}, {TEXT("--intent-space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
            {TEXT("--intent-heading-size"), FString::FromInt(CkStyle::FontSizeSmall())}};
    }

    auto PathContainsWidget(const FWidgetPath& InPath, const TSharedRef<SWidget>& InRoot) -> bool
    {
        for (int32 WidgetIndex = 0; WidgetIndex < InPath.Widgets.Num(); ++WidgetIndex)
        {
            const FArrangedWidget& Arranged = InPath.Widgets[WidgetIndex];
            if (Arranged.Widget == InRoot) { return true; }
        }
        return false;
    }

    auto ReleaseOwnedSlateInput(const TSharedRef<SWidget>& InRoot) -> void
    {
        if (NOT FSlateApplication::IsInitialized()) { return; }
        FSlateApplication& Slate = FSlateApplication::Get();
        Slate.ForEachUser([&Slate, &InRoot](FSlateUser& InUser)
        {
            const auto IsUnderRoot = [&Slate, &InRoot](const TSharedPtr<SWidget>& InWidget)
            {
                FWidgetPath Path;
                return InWidget.IsValid() && Slate.GeneratePathToWidgetUnchecked(InWidget.ToSharedRef(), Path, EVisibility::All)
                    && PathContainsWidget(Path, InRoot);
            };
            const int32 UserIndex = InUser.GetUserIndex();
            if (IsUnderRoot(Slate.GetUserFocusedWidget(UserIndex))) { Slate.ClearUserFocus(UserIndex, EFocusCause::SetDirectly); }
            if (IsUnderRoot(InUser.GetCursorCaptor())) { InUser.ReleaseCursorCapture(); }
            TSet<uint32> PointerIndices{FSlateApplication::CursorPointerIndex};
            for (const auto& Entry : InUser.GetWidgetsUnderPointerLastEventByIndex()) { PointerIndices.Add(Entry.Key); }
            for (const uint32 PointerIndex : PointerIndices)
            {
                if (IsUnderRoot(InUser.GetPointerCaptor(PointerIndex))) { InUser.ReleaseCapture(PointerIndex); }
            }
        }, true);
    }

}

// --------------------------------------------------------------------------------------------------------------------

const FName SCkIntentDebuggerWindow::WindowId = FName(TEXT("IntentDebugger"));

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    Is_IntentDebuggerEntity(
        const FCk_Handle& InCandidate)
    -> bool
{
    if (ck::Is_NOT_Valid(InCandidate))
    { return false; }

    return UCk_Utils_InputLayer_UE::Has(InCandidate) || UCk_Utils_InputSource_UE::Has(InCandidate);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();
#if WITH_DEV_AUTOMATION_TESTS
    _TestResourceDirectory = InArgs._TestResourceDirectory;
#endif

    _ViewModel = MakeShared<FCkIntentDebugger_ViewModel>();

    // Shared viewport picker, specialized to input sources/layers: only those
    // entities (plus their owner chain up to the pawn/NPC representative) are
    // previewed and pickable. The pick routes through this module's registered
    // entity-target route, which reduces the target to plain values and
    // re-fronts the tab.
    _ViewportPicker = MakeShared<FCkDebug_ViewportPicker>();
    {
        auto PickerParams = FCkDebug_ViewportPicker::FParams{};
        PickerParams.Get_TargetWorld =
            [WeakViewModel = TWeakPtr<FCkIntentDebugger_ViewModel>(_ViewModel)]() -> UWorld*
            {
                const auto Pinned = WeakViewModel.Pin();
                if (NOT Pinned.IsValid())
                { return nullptr; }

                const auto WorldModel = Pinned->Get_WorldModel();
                return WorldModel.IsValid() ? WorldModel->Get_SelectedWorld() : nullptr;
            };
        PickerParams.TargetFilter =
            [](const FCk_Handle& InCandidate) { return Is_IntentDebuggerEntity(InCandidate); };
        PickerParams.OnEntityPicked =
            [](const FCk_Handle& InPicked)
            {
                ck::DebugSelectionSync::Broadcast(InPicked, TEXT("IntentDebugger"));
                FCkDebug_EntityTargetRegistry::Get().TryOpenAndTarget(
                    FName{TEXT("CkIntentDebugger")}, InPicked);
            };
        _ViewportPicker->Construct(MoveTemp(PickerParams));
    }

    _ViewModelChangedHandle = _ViewModel->OnChanged.AddSP(
        this, &SCkIntentDebuggerWindow::HandleViewModelChanged);

    const TSharedRef<bool> IsInputHudMenuOpen = MakeShared<bool>(false);
    const TWeakPtr<SCkIntentDebuggerWindow> WeakWindow = SharedThis(this);
    const TSharedRef<SCkIntentDebugger_InputHudControls> InputHudControls =
        SNew(SCkIntentDebugger_InputHudControls)
            .CanDispatchEvents_Lambda([IsInputHudMenuOpen, WeakWindow]
            {
                const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
                return *IsInputHudMenuOpen && Window.IsValid() && NOT Window->_PresentationReleased;
            });

    const auto InputHudCommandGroup = SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SCkDebug_IconToolbar)
                .Actions({
                    FCkDebug_IconToggleAction{
                        TEXT("InputHudOverlay"),
                        ECk_Icon::World,
                        FText::FromString(TEXT("Input HUD overlay")),
                        FText::FromString(TEXT("Toggle the on-screen QA input overlay (ck.InputOverlay).\n"
                             "Off (0) hides it, on (2) shows the auto device visual.")),
                        TAttribute<bool>::CreateLambda([WeakWindow]() -> bool
                        {
                            const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
                            if (NOT Window.IsValid() || Window->_PresentationReleased) { return false; }
                            const auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay"));
                            return CVar != nullptr && CVar->GetInt() != 0;
                        }),
                        FOnCkDebug_IconToggleChanged::CreateLambda([WeakWindow](bool InIsOn)
                        {
                            const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
                            if (NOT Window.IsValid() || Window->_PresentationReleased) { return; }
                            if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay")))
                            { CVar->Set(InIsOn ? 2 : 0, ECVF_SetByConsole); }
                        }),
                        TAttribute<bool>::CreateLambda([WeakWindow]() -> bool
                        {
                            const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
                            return Window.IsValid() && NOT Window->_PresentationReleased
                                && IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay")) != nullptr;
                        })}
                })
        ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f)
        [
            SAssignNew(_InputHudMenu, SComboButton)
                .OnMenuOpenChanged_Lambda([IsInputHudMenuOpen](const bool bIsOpen)
                {
                    // The menu anchor dismisses its popup and handles focus when it closes. The retained view's
                    // dispatch gate makes every held control inert until the next open event.
                    *IsInputHudMenuOpen = bIsOpen;
                })
                .ToolTipText(FText::FromString(TEXT("Input HUD readout, session and project behavior controls.")))
                .ButtonContent()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("HUD settings")))
                        .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                ]
                .MenuContent()
                [
                    InputHudControls
                ]
        ];

    ChildSlot
    [
        SAssignNew(_Chrome, SCkDebug_WindowChrome)
            .WindowId(Get_WindowId())
            .ToolTabId(TEXT("CkIntentDebugger"))
            .StatusText_Lambda([WeakWindow]()
            {
                const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
                return Window.IsValid() && NOT Window->_PresentationReleased ? Window->Get_StatusText() : FText::GetEmpty();
            })
            .CommandGroups({
                FCkDebug_CommandGroup::Primary(TEXT("IntentView"), FText::FromString(TEXT("Intent view controls")),
                    InputHudCommandGroup),
                FCkDebug_CommandGroup::Context(TEXT("IntentTarget"), FText::FromString(TEXT("Intent source and target")), Build_Toolbar())
            })
            .CommonActionsContent()
            [
                SNew(SCkDebug_ViewportPickerControls)
                    .Picker(_ViewportPicker)
                    .PickTooltip(FText::FromString(TEXT(
                        "Enter pick mode: click an entity carrying an input source or input layer.\n"
                        "Only those entities (and their owning pawn) are shown and pickable.")))
            ]
            .ShowRefreshControls(true)
            .Content()
            [
                SAssignNew(_BodyHost, SBox)
                [
                    Build_Body()
                ]
            ]
    ];

    Build_AuthoredBody();
}

SCkIntentDebuggerWindow::
    ~SCkIntentDebuggerWindow()
{
    Release_Presentation();

    if (_ViewModel.IsValid() && _ViewModelChangedHandle.IsValid())
    {
        _ViewModel->OnChanged.Remove(_ViewModelChangedHandle);
        _ViewModelChangedHandle.Reset();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    Build_Toolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::Bg1())
        .Padding(FMargin{ck_intent_debugger_window::PadS})
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(ck_intent_debugger_window::PadS, 0.0f)
            [
                SNew(SCkDebug_WorldSelector, _ViewModel->Get_WorldModel())
                    .ShowHeaderLabel(false)
            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(ck_intent_debugger_window::PadM, 0.0f, ck_intent_debugger_window::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Source:")))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(CkStyle::TextDim())
            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SAssignNew(_SourceSelectorHost, SBox)
                [
                    SAssignNew(_SourceSelectorBox, SHorizontalBox)
                ]
            ]

        ];
}

// --------------------------------------------------------------------------------------------------------------------

// Everything at once, no tabs, and EVERY boundary is a splitter handle — a fixed proportion is a proportion the
// user cannot fix. The three tables share the TOP row because they are shallow-but-wide reads; the timeline and
// the key/state + devices pair get the full window width below, which is where the horizontal room is needed.
// The splitters also keep every list at a bounded height, which SListView needs.
auto
    SCkIntentDebuggerWindow::
    Build_Body()
    -> TSharedRef<SWidget>
{
    return SNew(SSplitter)
        .Orientation(Orient_Vertical)

        + SSplitter::Slot().Value(0.30f)
        [
            SNew(SSplitter)
                .Orientation(Orient_Horizontal)

                + SSplitter::Slot().Value(0.34f)
                [
                    SNew(SCkDebug_PaneHost)
                    [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_SectionHeader)
                            .Label(FText::FromString(TEXT("Layer stack")))
                            .SubText(FText::FromString(TEXT("top-down · first Consume wins")))
                            .Underline(true)
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SAssignNew(_LayerStackPanel, SCkIntentDebugger_LayerStackPanel)
                            .ViewModel(_ViewModel)
                    ]
                    ]
                ]

                + SSplitter::Slot().Value(0.36f)
                [
                    SNew(SCkDebug_PaneHost)
                    [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_SectionHeader)
                            .Label(FText::FromString(TEXT("Resolution table")))
                            .Underline(true)
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SAssignNew(_ResolutionPanel, SCkIntentDebugger_ResolutionPanel)
                            .ViewModel(_ViewModel)
                    ]
                    ]
                ]

                + SSplitter::Slot().Value(0.30f)
                [
                    SNew(SCkDebug_PaneHost)
                    [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_SectionHeader)
                            .Label(FText::FromString(TEXT("Near misses")))
                            .Underline(true)
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SAssignNew(_NearMissPanel, SCkIntentDebugger_NearMissPanel)
                            .ViewModel(_ViewModel)
                    ]
                    ]
                ]
        ]

        + SSplitter::Slot().Value(0.70f)
        [
            SNew(SSplitter)
                .Orientation(Orient_Vertical)

            + SSplitter::Slot().Value(0.35f)
            [
                SNew(SCkDebug_PaneHost)
                .ContentMode(ECkDebugPaneContent::OpaqueRenderer)
                [
                    SAssignNew(_TimelineDock, SCkIntentDebugger_TimelineDock)
                        .ViewModel(_ViewModel)
                ]
            ]

            + SSplitter::Slot().Value(0.65f)
            [
                SNew(SSplitter)
                    .Orientation(Orient_Horizontal)

                + SSplitter::Slot().Value(0.38f)
                [
                    SNew(SCkDebug_PaneHost)
                    [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_SectionHeader)
                            .Label(FText::FromString(TEXT("Key / State")))
                            .Underline(true)
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SAssignNew(_KeyStatePanel, SCkIntentDebugger_KeyStatePanel)
                            .ViewModel(_ViewModel)
                    ]
                    ]
                ]

                + SSplitter::Slot().Value(0.62f)
                [
                    SNew(SCkDebug_PaneHost)
                    [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_SectionHeader)
                            .Label(FText::FromString(TEXT("Devices")))
                            .SubText(FText::FromString(TEXT("flash = press · fill = hold toward the verdict · outline = the game listens to it · dim = not connected")))
                            .Underline(true)
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SAssignNew(_DevicesPanel, SCkIntentDebugger_DevicesPanel)
                            .ViewModel(_ViewModel)
                    ]
                    ]
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (_PresentationReleased) { return; }

    Poll_AuthoredBody(InCurrentTime);

    // Viewport-picker ticks stay ungated so input handling keeps working even
    // when the panel refresh is paused.
    if (_ViewportPicker.IsValid() && _ViewportPicker->IsActive())
    { _ViewportPicker->Tick(InDeltaTime); }

    if (NOT _ViewModel.IsValid())
    { return; }

    // BEFORE the gate: edge capture is truth, not presentation — a release sampled at a capped cadence is a
    // release missed, which latches a witnessed key down (the slice-11-1 stuck-key defect).
    _ViewModel->Tick_WitnessDeviceEdges();

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _ViewModel->Tick();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    Build_AuthoredBody()
    -> void
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid()
        || NOT _BodyHost.IsValid() || NOT _LayerStackPanel.IsValid() || NOT _TimelineDock.IsValid()
        || NOT _KeyStatePanel.IsValid() || NOT _ResolutionPanel.IsValid() || NOT _NearMissPanel.IsValid()
        || NOT _DevicesPanel.IsValid())
    { return; }

    const FCkUiLoadResult SourceRecordsCreated = FCkUiCollection::TryCreate({
        {TEXT("label"), ECkUiFieldKind::Text}, {TEXT("selected"), ECkUiFieldKind::Bool},
        {TEXT("color"), ECkUiFieldKind::Color}}, _SourceRecords);
    if (NOT SourceRecordsCreated.Succeeded || NOT _SourceRecords.IsValid())
    { return; }
    Refresh_SourceRecords();

    SAssignNew(_LayerStackMount, SBox);
    SAssignNew(_TimelineMount, SBox);
    SAssignNew(_KeyStateMount, SBox);
    SAssignNew(_ResolutionMount, SBox);
    SAssignNew(_NearMissMount, SBox);
    SAssignNew(_DevicesMount, SBox);

    auto Ports = FCkUiView::FNativeBindings{};
    Ports.Add(TEXT("intent-layer-stack"), _LayerStackMount);
    Ports.Add(TEXT("intent-resolution"), _ResolutionMount);
    Ports.Add(TEXT("intent-near-misses"), _NearMissMount);
    Ports.Add(TEXT("intent-timeline"), _TimelineMount);
    Ports.Add(TEXT("intent-key-state"), _KeyStateMount);
    Ports.Add(TEXT("intent-devices"), _DevicesMount);
    const TWeakPtr<SCkIntentDebuggerWindow> WeakWindow = SharedThis(this);
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    Data.Collections.Add(TEXT("intent-sources"), _SourceRecords);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() && NOT Window->_PresentationReleased;
    });
    Data.ItemActions.Add(TEXT("intent-source-select"), FCkUiOnItemAction::CreateLambda([WeakWindow](FString InKey)
    {
        const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
        if (NOT Window.IsValid() || Window->_PresentationReleased || NOT Window->_ViewModel.IsValid()) { return; }
        const int32 Index = FCString::Atoi(*InKey);
        if (Window->_ViewModel->Get_Snapshot().Sources.IsValidIndex(Index))
        { Window->_ViewModel->Set_SelectedSourceIndex(Index); }
    }));
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(MoveTemp(Ports), {}, ck_intent_debugger_window::AuthoredStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
#if WITH_DEV_AUTOMATION_TESTS
    if (NOT _TestResourceDirectory.IsEmpty()) { Directory = _TestResourceDirectory; }
#endif
    _AuthoredBodyMarkupPath = FPaths::Combine(Directory, TEXT("IntentDebugger.ui.html"));
    _AuthoredBodyStylesheetPath = FPaths::Combine(Directory, TEXT("IntentDebugger.ui.css"));
    Candidate->SetFiles(_AuthoredBodyMarkupPath, _AuthoredBodyStylesheetPath);
    Candidate->GetRegion(TEXT("actions"));
    Candidate->GetRegion(TEXT("main"));
    _AuthoredBody = Candidate;
    Candidate->PollFiles(ck_intent_debugger_window::AuthoredStyleTokens());
    if (NOT Candidate->GetLastResult().Succeeded)
    { return; }

    // Candidate admission is complete while fallback still owns every panel. Detach it before a port takes ownership.
    _BodyHost->SetContent(SNullWidget::NullWidget);
    _LayerStackMount->SetContent(_LayerStackPanel.ToSharedRef());
    _ResolutionMount->SetContent(_ResolutionPanel.ToSharedRef());
    _NearMissMount->SetContent(_NearMissPanel.ToSharedRef());
    _TimelineMount->SetContent(_TimelineDock.ToSharedRef());
    _KeyStateMount->SetContent(_KeyStatePanel.ToSharedRef());
    _DevicesMount->SetContent(_DevicesPanel.ToSharedRef());
    _BodyHost->SetContent(Candidate->GetRegion(TEXT("main")));
    if (_SourceSelectorHost.IsValid()) { _SourceSelectorHost->SetContent(Candidate->GetRegion(TEXT("actions"))); }
    _UsingNativeBodyFallback = false;
}

auto
    SCkIntentDebuggerWindow::
    Poll_AuthoredBody(
        const double InCurrentTime)
    -> void
{
    if (_PresentationReleased || NOT _AuthoredBody.IsValid() || InCurrentTime < _NextAuthoredBodyPollSeconds)
    { return; }

    _NextAuthoredBodyPollSeconds = InCurrentTime + 0.5;
    const bool Changed = _AuthoredBody->PollFiles(ck_intent_debugger_window::AuthoredStyleTokens());
    if (_UsingNativeBodyFallback && Changed && _AuthoredBody->GetLastResult().Succeeded && _BodyHost.IsValid()
        && _LayerStackMount.IsValid() && _ResolutionMount.IsValid() && _NearMissMount.IsValid()
        && _TimelineMount.IsValid() && _KeyStateMount.IsValid() && _DevicesMount.IsValid()
        && _LayerStackPanel.IsValid() && _ResolutionPanel.IsValid() && _NearMissPanel.IsValid()
        && _TimelineDock.IsValid() && _KeyStatePanel.IsValid() && _DevicesPanel.IsValid())
    {
        _BodyHost->SetContent(SNullWidget::NullWidget);
        _LayerStackMount->SetContent(_LayerStackPanel.ToSharedRef());
        _ResolutionMount->SetContent(_ResolutionPanel.ToSharedRef());
        _NearMissMount->SetContent(_NearMissPanel.ToSharedRef());
        _TimelineMount->SetContent(_TimelineDock.ToSharedRef());
        _KeyStateMount->SetContent(_KeyStatePanel.ToSharedRef());
        _DevicesMount->SetContent(_DevicesPanel.ToSharedRef());
        _BodyHost->SetContent(_AuthoredBody->GetRegion(TEXT("main")));
        if (_SourceSelectorHost.IsValid()) { _SourceSelectorHost->SetContent(_AuthoredBody->GetRegion(TEXT("actions"))); }
        _UsingNativeBodyFallback = false;
    }
}

auto
    SCkIntentDebuggerWindow::
    Release_Presentation()
    -> void
{
    if (_PresentationReleased)
    { return; }

    _PresentationReleased = true;
    if (_ViewportPicker.IsValid()) { _ViewportPicker->Deactivate(); }
    if (_ViewModel.IsValid() && _ViewModelChangedHandle.IsValid())
    {
        _ViewModel->OnChanged.Remove(_ViewModelChangedHandle);
        _ViewModelChangedHandle.Reset();
    }
    if (_ViewModel.IsValid()) { _ViewModel->Reset_ForWorldChange(); }
    if (_AuthoredBody.IsValid())
    {
        _AuthoredBody->ReleaseOwnerInteractions();
    }
    if (_LayerStackPanel.IsValid()) { _LayerStackPanel->ReleaseContextMenu(); }
    if (_ResolutionPanel.IsValid()) { _ResolutionPanel->ReleaseContextMenu(); }
    if (_NearMissPanel.IsValid()) { _NearMissPanel->ReleaseContextMenu(); }
    if (_Chrome.IsValid()) { ck_intent_debugger_window::ReleaseOwnedSlateInput(_Chrome.ToSharedRef()); }
    if (_InputHudMenu.IsValid()) { _InputHudMenu->SetIsOpen(false); }
    if (_BodyHost.IsValid()) { _BodyHost->SetContent(SNullWidget::NullWidget); }
    if (_SourceSelectorHost.IsValid()) { _SourceSelectorHost->SetContent(SNullWidget::NullWidget); }
    if (_LayerStackMount.IsValid()) { _LayerStackMount->SetContent(SNullWidget::NullWidget); }
    if (_ResolutionMount.IsValid()) { _ResolutionMount->SetContent(SNullWidget::NullWidget); }
    if (_NearMissMount.IsValid()) { _NearMissMount->SetContent(SNullWidget::NullWidget); }
    if (_TimelineMount.IsValid()) { _TimelineMount->SetContent(SNullWidget::NullWidget); }
    if (_KeyStateMount.IsValid()) { _KeyStateMount->SetContent(SNullWidget::NullWidget); }
    if (_DevicesMount.IsValid()) { _DevicesMount->SetContent(SNullWidget::NullWidget); }
    ChildSlot[SNullWidget::NullWidget];
    _AuthoredBody.Reset();
    _LayerStackPanel.Reset();
    _TimelineDock.Reset();
    _KeyStatePanel.Reset();
    _ResolutionPanel.Reset();
    _NearMissPanel.Reset();
    _DevicesPanel.Reset();
    _ViewportPicker.Reset();
    _ViewModel.Reset();
    _SourceSelectorBox.Reset();
    _SourceSelectorHost.Reset();
    _InputHudMenu.Reset();
    _Chrome.Reset();
    _SourceRecords.Reset();
    _LastSourceCount = INDEX_NONE;
    _PendingLocalPlayerIndex = INDEX_NONE;
    _PendingLayerPriority = MIN_int32;
    _HasPendingTarget = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    OpenForEntity(
        const FCk_Handle& InEntity)
    -> void
{
    if (ck::Is_NOT_Valid(InEntity))
    { return; }

    auto Entity = InEntity;

    auto LocalPlayerIndex = static_cast<int32>(INDEX_NONE);
    auto LayerPriority = MIN_int32;

    if (const auto Layer = UCk_Utils_InputLayer_UE::Cast(Entity);
        ck::IsValid(Layer))
    {
        LayerPriority = UCk_Utils_InputLayer_UE::Get_Priority(Layer);

        const auto Source = UCk_Utils_InputLayer_UE::Get_InputSource(Layer);
        if (ck::IsValid(Source))
        { LocalPlayerIndex = UCk_Utils_InputSource_UE::Get_LocalPlayerIndex(Source); }
    }
    else if (const auto Source = UCk_Utils_InputSource_UE::Cast(Entity);
             ck::IsValid(Source))
    {
        LocalPlayerIndex = UCk_Utils_InputSource_UE::Get_LocalPlayerIndex(Source);
    }

    if (LocalPlayerIndex == INDEX_NONE)
    { return; }

    // The module's OnSpawnDebuggerTab is what creates the live window and stores it, so this MUST run before we
    // ask the module for the window pointer.
    ck::debugger_tabs::Invoke_DebuggerTab(FCkIntentDebuggerModule::Get_TabName());

    const auto Window = FCkIntentDebuggerModule::Get().Get_DebuggerWindow();
    if (NOT Window.IsValid())
    { return; }

    Window->Set_PendingTarget(LocalPlayerIndex, LayerPriority);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    Set_PendingTarget(
        int32 InLocalPlayerIndex,
        int32 InLayerPriority)
    -> void
{
    _PendingLocalPlayerIndex = InLocalPlayerIndex;
    _PendingLayerPriority = InLayerPriority;
    _HasPendingTarget = true;

    DoApply_PendingTarget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    DoApply_PendingTarget()
    -> void
{
    if (NOT _HasPendingTarget || NOT _ViewModel.IsValid())
    { return; }

    const auto& Sources = _ViewModel->Get_Snapshot().Sources;

    auto SourceIndex = static_cast<int32>(INDEX_NONE);
    for (auto Index = 0; Index < Sources.Num(); ++Index)
    {
        if (Sources[Index].LocalPlayerIndex != _PendingLocalPlayerIndex)
        { continue; }

        SourceIndex = Index;
        break;
    }

    // The collector may not have run yet on a just-opened tab. Keep waiting rather than dropping the request.
    if (SourceIndex == INDEX_NONE)
    { return; }

    const auto WantedLayerPriority = _PendingLayerPriority;

    // Resolved against the snapshot BEFORE any setter runs — a setter broadcasts, and reading a snapshot
    // reference across a broadcast is how a dangling read starts.
    auto HasWantedLayer = false;
    for (const auto& Layer : Sources[SourceIndex].Layers)
    {
        if (Layer.Priority != WantedLayerPriority)
        { continue; }

        HasWantedLayer = true;
        break;
    }

    // Cleared BEFORE the setters, because each one broadcasts OnChanged straight back into this function.
    _HasPendingTarget = false;
    _PendingLocalPlayerIndex = INDEX_NONE;
    _PendingLayerPriority = MIN_int32;

    _ViewModel->Set_SelectedSourceIndex(SourceIndex);

    if (HasWantedLayer)
    { _ViewModel->Set_SelectedLayerPriority(WantedLayerPriority); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    HandleViewModelChanged()
    -> void
{
    DoApply_PendingTarget();

    Refresh_SourceSelector();

    if (_LayerStackPanel.IsValid())
    { _LayerStackPanel->RefreshFromViewModel(); }

    if (_TimelineDock.IsValid())
    { _TimelineDock->RefreshFromViewModel(); }

    if (_ResolutionPanel.IsValid())
    { _ResolutionPanel->RefreshFromViewModel(); }

    if (_NearMissPanel.IsValid())
    { _NearMissPanel->RefreshFromViewModel(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    Refresh_SourceSelector()
    -> void
{
    if (NOT _SourceSelectorBox.IsValid() || NOT _ViewModel.IsValid())
    { return; }

    const auto Count = _ViewModel->Get_Snapshot().Sources.Num();
    if (Count == _LastSourceCount)
    { Refresh_SourceRecords(); return; }

    _LastSourceCount = Count;
    Refresh_SourceRecords();
    _SourceSelectorBox->ClearChildren();

    if (Count == 0)
    {
        _SourceSelectorBox->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(CkStyle::TextMute())
                .Text(FText::FromString(TEXT("none")))
        ];

        return;
    }

    for (auto Index = 0; Index < Count; ++Index)
    {
        const auto Label = _ViewModel->Get_Snapshot().Sources[Index].Label;

        _SourceSelectorBox->AddSlot().AutoWidth().VAlign(VAlign_Center)
            .Padding(ck_intent_debugger_window::PadS, 0.0f)
        [
            SNew(SButton)
                .Text(FText::FromString(Label))
                .OnClicked_Lambda([WeakWindow = TWeakPtr<SCkIntentDebuggerWindow>{SharedThis(this)}, Index]()
                {
                    const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
                    if (NOT Window.IsValid() || Window->_PresentationReleased || NOT Window->_ViewModel.IsValid())
                    { return FReply::Unhandled(); }
                    Window->_ViewModel->Set_SelectedSourceIndex(Index);
                    return FReply::Handled();
                })
                .ButtonColorAndOpacity_Lambda([WeakWindow = TWeakPtr<SCkIntentDebuggerWindow>{SharedThis(this)}, Index]()
                {
                    const TSharedPtr<SCkIntentDebuggerWindow> Window = WeakWindow.Pin();
                    return FSlateColor{Window.IsValid() && NOT Window->_PresentationReleased && Window->_ViewModel.IsValid()
                        && Window->_ViewModel->Get_SelectedSourceIndex() == Index
                        ? CkStyle::Selection()
                        : CkStyle::Bg3()};
                })
        ];
    }
}

auto
    SCkIntentDebuggerWindow::
    Refresh_SourceRecords()
    -> void
{
    if (NOT _SourceRecords.IsValid() || NOT _ViewModel.IsValid())
    { return; }

    auto Records = TArray<FCkUiRecordData>{};
    const auto& Sources = _ViewModel->Get_Snapshot().Sources;
    Records.Reserve(Sources.Num());
    for (int32 Index = 0; Index < Sources.Num(); ++Index)
    {
        auto Record = FCkUiRecordData{};
        Record.Key = FString::FromInt(Index);
        auto Label = FCkUiFieldValue{};
        Label.Kind = ECkUiFieldKind::Text;
        Label.Text = FText::FromString(Sources[Index].Label);
        Record.Fields.Add(TEXT("label"), MoveTemp(Label));
        auto Selected = FCkUiFieldValue{};
        Selected.Kind = ECkUiFieldKind::Bool;
        Selected.Bool = _ViewModel->Get_SelectedSourceIndex() == Index;
        Record.Fields.Add(TEXT("selected"), MoveTemp(Selected));
        auto Color = FCkUiFieldValue{};
        Color.Kind = ECkUiFieldKind::Color;
        Color.Color = _ViewModel->Get_SelectedSourceIndex() == Index ? CkStyle::Accent() : CkStyle::Text();
        Record.Fields.Add(TEXT("color"), MoveTemp(Color));
        Records.Add(MoveTemp(Record));
    }
    if (NOT _SourceRecords->TrySetRecords(MoveTemp(Records)).Succeeded)
    { _SourceRecords->TrySetRecords({}); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebuggerWindow::
    Get_StatusText() const
    -> FText
{
    if (NOT _ViewModel.IsValid())
    { return FText::GetEmpty(); }

    const auto& Snapshot = _ViewModel->Get_Snapshot();

    if (NOT Snapshot.HasWorld)
    { return FText::FromString(TEXT("No running world. Start PIE to record intents.")); }

    const auto* Source = _ViewModel->TryGet_SelectedSource();
    if (Source == nullptr)
    { return FText::FromString(TEXT("No input source. The subsystem creates one once the local player has a controller.")); }

    if (NOT Source->HasSampler)
    {
        return FText::FromString(TEXT(
            "Input source has no IntentSampler — no frame record is being written on this source."));
    }

    return FText::FromString(ck::Format_UE(
        TEXT("{} · {} frame(s) of {} · {} layer(s) · scan diagnostics {}"),
        Source->Label,
        Source->FrameCount,
        Source->RingCapacity,
        Source->Layers.Num(),
        Snapshot.ScanDiagnosticsEnabled ? TEXT("ON") : TEXT("OFF")));
}

// --------------------------------------------------------------------------------------------------------------------
