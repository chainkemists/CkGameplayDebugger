#include "CkInputDebugger/Window/SCkInputDebuggerWindow.h"

#include "CkDebuggerCommon/Devices/SCkDebug_DevicesPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CopyableContainer.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include "Framework/Application/SlateApplication.h"

#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Styling/AppStyle.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Local style + helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_debugger
{
    // TextScale-aware counterparts of CkStyle::RegularFont / BoldFont / MonoFont. Bound through
    // .Font_Static below so a Style Lab flip resizes text that was built long before the flip.
    static auto Normal(int32 InSize) -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Regular", InSize); }
    static auto Bold(int32 InSize)   -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Bold", InSize); }
    static auto Mono(int32 InSize)   -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Mono", InSize); }

    static auto Font_Heading()  -> FSlateFontInfo { return Bold(CkStyle::FontSizeH3()); }
    static auto Font_RowLabel() -> FSlateFontInfo { return Bold(CkStyle::FontSizeH4()); }
    static auto Font_Body()     -> FSlateFontInfo { return Normal(CkStyle::FontSizeSmall()); }
    static auto Font_Value()    -> FSlateFontInfo { return Mono(CkStyle::FontSizeSmall()); }

    // RowDensity lands live on the mapping / action rows � SBorder::Padding is an attribute, so the
    // axis moves rows that were built long before the flip.
    static auto Get_MappingRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(
            FMargin{CkStyle::SpaceXL, CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceS});
    }

    static auto Get_ActionRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS});
    }

    static auto ActivityColor(ECkInputDebugger_ActionActivity InActivity) -> FLinearColor
    {
        switch (InActivity)
        {
            case ECkInputDebugger_ActionActivity::Active:    return CkStyle::Ok();
            case ECkInputDebugger_ActionActivity::Ongoing:   return CkStyle::Warn();
            case ECkInputDebugger_ActionActivity::Canceled:  return CkStyle::Err();
            case ECkInputDebugger_ActionActivity::Completed: return CkStyle::Accent();
            case ECkInputDebugger_ActionActivity::Idle:      return CkStyle::TextMute();
            case ECkInputDebugger_ActionActivity::NoInstance:
            default:                                         return CkStyle::TextMute();
        }
    }

}

// --------------------------------------------------------------------------------------------------------------------
// Construct
// --------------------------------------------------------------------------------------------------------------------

const FName SCkInputDebuggerWindow::WindowId = FName(TEXT("InputDebugger"));

auto
    SCkInputDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

    _SummaryText = SNew(STextBlock)
        .Font_Static(&ck_input_debugger::Font_Heading)
        .ColorAndOpacity(CkStyle::Text());

    _ContextListBox  = SNew(SVerticalBox);
    _ResolvedListBox = SNew(SVerticalBox);
    _BindingsListBox = SNew(SVerticalBox);
    _KeyStripBox     = SNew(SHorizontalBox);
    _TimelineHost    = SNew(SBox);

    // Passive application-wide observer for the live surfaces — every handler returns false, so
    // it can never starve viewport input (ck-slate-tools §3).
    if (FSlateApplication::IsInitialized())
    {
        _KeyObserver = MakeShared<FCkDebug_KeyActivityObserver>();
        FSlateApplication::Get().RegisterInputPreProcessor(_KeyObserver);
    }

#if WITH_EDITOR
    _EndPIEHandle = FEditorDelegates::EndPIE.AddSP(this, &SCkInputDebuggerWindow::OnEndPIE);
#endif

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TEXT("CkInputDebugger"))
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(TEXT("InputView"), FText::FromString(TEXT("Input view controls")),
            SNew(SCkDebug_IconToolbar)
            .Actions({
                FCkDebug_IconToggleAction{
                    TEXT("InputActiveActionsOnly"),
                    TEXT("Input"),
                    FText::FromString(TEXT("Active Actions Only")),
                    FText::FromString(TEXT("Show only resolved actions that are active or ongoing.")),
                    TAttribute<bool>::CreateLambda([this]() { return _ShowActiveActionsOnly; }),
                    FOnCkDebug_IconToggleChanged::CreateLambda([this](const bool InIsEnabled)
                    {
                        _ShowActiveActionsOnly = InIsEnabled;
                        ApplyFilterAndHighlight();
                    })},
                FCkDebug_IconToggleAction{
                    TEXT("InputHudOverlay"),
                    TEXT("World"),
                    FText::FromString(TEXT("Input HUD overlay")),
                    FText::FromString(TEXT("Toggle the on-screen QA input overlay (ck.InputOverlay).\n"
                         "Off (0) hides it, on (2) shows the auto device visual.")),
                    TAttribute<bool>::CreateLambda([]() -> bool
                    {
                        const auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay"));
                        return CVar != nullptr && CVar->GetInt() != 0;
                    }),
                    FOnCkDebug_IconToggleChanged::CreateLambda([](bool InIsOn)
                    {
                        if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay")))
                        { CVar->Set(InIsOn ? 2 : 0, ECVF_SetByConsole); }
                    }),
                    TAttribute<bool>::CreateLambda([]() -> bool
                    {
                        return IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay")) != nullptr;
                    })}
            })),
            FCkDebug_CommandGroup::Context(TEXT("InputContext"), FText::FromString(TEXT("Player and input search")), BuildToolbar())
        })
        .ShowRefreshControls(true)
        .Content()
        [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::Bg1())
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                [ _SummaryText.ToSharedRef() ]

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
                [ ck::debug_axes::Make_AxisSeparator() ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)

                    + SScrollBox::Slot().Padding(CkStyle::SpaceS)
                        [
                            SNew(SVerticalBox)

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS * 0.5f)
                                [
                                    BuildSection(
                                        FText::FromString(TEXT("Held & Recent Keys")),
                                        SNullWidget::NullWidget,
                                        _KeyStripBox.ToSharedRef())
                                ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS * 0.5f)
                                [
                                    BuildSection(
                                        FText::FromString(TEXT("Devices")),
                                        SNullWidget::NullWidget,
                                        BuildDevicesSection())
                                ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS * 0.5f)
                                [
                                    BuildSection(
                                        FText::FromString(TEXT("Timeline")),
                                        SNew(STextBlock)
                                            .Font_Static(&ck_input_debugger::Font_Body)
                                            .Text(FText::FromString(TEXT("frame axis · wheel zooms · right-drag pans · F = follow live · click a press marker to filter")))
                                            .ColorAndOpacity(CkStyle::TextMute()),
                                        _TimelineHost.ToSharedRef())
                                ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS * 0.5f)
                                [
                                    BuildSection(
                                        FText::FromString(TEXT("Player Bindings — default vs current")),
                                        BuildBindingsHeader(),
                                        _BindingsListBox.ToSharedRef())
                                ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS * 0.5f)
                                [
                                    BuildSection(
                                        FText::FromString(TEXT("Mapping Context Stack")),
                                        SNullWidget::NullWidget,
                                        _ContextListBox.ToSharedRef())
                                ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS * 0.5f)
                                [
                                    BuildSection(
                                        FText::FromString(TEXT("Resolved Bindings (live)")),
                                        SNullWidget::NullWidget,
                                        _ResolvedListBox.ToSharedRef())
                                ]
                        ]
                ]
        ]
        ]
    ];
}

SCkInputDebuggerWindow::~SCkInputDebuggerWindow()
{
    if (_KeyObserver.IsValid() && FSlateApplication::IsInitialized())
    { FSlateApplication::Get().UnregisterInputPreProcessor(_KeyObserver); }

#if WITH_EDITOR
    if (_EndPIEHandle.IsValid())
    { FEditorDelegates::EndPIE.Remove(_EndPIEHandle); }
#endif
}

auto
    SCkInputDebuggerWindow::
    OnEndPIE(
        bool InIsSimulating)
    -> void
{
    // Keys held while PIE dies release into a dead world — never observed
    if (_KeyObserver.IsValid())
    { _KeyObserver->Clear(); }

    _KeyFilter = FKey{};

    _TimelineStructureHash = 0;
    _TimelineLaneLabels.Reset();
    _TimelineLaneKeys.Reset();
    _Timeline.Reset();

    if (_TimelineHost.IsValid())
    { _TimelineHost->SetContent(SNullWidget::NullWidget); }
}

// --------------------------------------------------------------------------------------------------------------------
// Toolbar
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::BgRoot())
        .Padding(FMargin(CkStyle::SpaceS))
        [
            SNew(SHorizontalBox)

            // World selector (shared across all CK debuggers)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_WorldSelector, _WorldModel)
                        .ShowHeaderLabel(false)
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Player:")))
                        .Font_Static(&ck_input_debugger::Font_Body)
                        .ColorAndOpacity(CkStyle::TextDim())
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SAssignNew(_PlayerSelectorBox, SHorizontalBox)
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox).MinDesiredWidth(260.0f)
                    [
                        SNew(SCkDebug_DualSearchBar)
                            .FilterHintText(FText::FromString(TEXT("Filter actions / keys / contexts…")))
                            .HighlightHintText(FText::FromString(TEXT("Highlight…")))
                            .OnFilterTextChanged_Lambda([this](const FString& InText)
                            {
                                if (_FilterString == InText) { return; }
                                _FilterString = InText;
                                ApplyFilterAndHighlight();
                            })
                            .OnHighlightTextChanged_Lambda([this](const FString& InText)
                            {
                                if (_HighlightString == InText) { return; }
                                _HighlightString = InText;
                                ApplyFilterAndHighlight();
                            })
                    ]
                ]

        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Section scaffolding
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    BuildSection(
        const FText& InLabel,
        const TSharedRef<SWidget>& InHeaderExtra,
        const TSharedRef<SWidget>& InBody)
    -> TSharedRef<SWidget>
{
    return SNew(SExpandableArea)
        .InitiallyCollapsed(false)
        .BorderBackgroundColor(CkStyle::Bg1())
        .HeaderPadding(FMargin(CkStyle::SpaceS, CkStyle::SpaceS * 0.5f))
        .Padding(FMargin(CkStyle::SpaceS, CkStyle::SpaceS * 0.5f, 0.0f, CkStyle::SpaceS * 0.5f))
        .HeaderContent()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SectionHeader)
                        .Label(InLabel)
                ]

            + SHorizontalBox::Slot().FillWidth(1.0f)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [ InHeaderExtra ]
        ]
        .BodyContent()
        [ InBody ];
}

auto
    SCkInputDebuggerWindow::
    BuildDevicesSection()
    -> TSharedRef<SWidget>
{
    // The shared panel keeps this section pixel-identical to the Intent debugger's Devices view.
    // One composed snapshot serves every device widget; the attribute refreshes it lazily, at most
    // once per frame, so the flash decay stays smooth regardless of the window's refresh gate.
    return SNew(SCkDebug_DevicesPanel)
        .Snapshot_Lambda([this]() { return Get_DeviceSnapshot(); })
        .OnKeyClicked_Lambda([this](const FKey& InKey) { HandleDeviceKeyClicked(InKey); })
        .KeyTooltip_Lambda([this](const FKey& InKey) { return Get_KeyTooltip(InKey); })
        .NoteText(FText::FromString(TEXT(
            "flash = press · fill = hold · outlined = mapped · amber = rebound · bright ring = filtered · hover = actions · click = filter the panes below")));
}

auto
    SCkInputDebuggerWindow::
    BuildBindingsHeader()
    -> TSharedRef<SWidget>
{
    const auto MakeModeButton = [this](const TCHAR* InLabel, ECkInputDebugger_BindingsFilterMode InMode) -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin(CkStyle::SpaceS, 1.0f))
            .ToolTipText(FText::FromString(TEXT("Filter the bindings rows below")))
            .OnClicked_Lambda([this, InMode]()
            {
                _BindingsFilterMode = InMode;
                ApplyFilterAndHighlight();
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Body)
                    .Text(FText::FromString(InLabel))
                    .ColorAndOpacity_Lambda([this, InMode]()
                    {
                        return _BindingsFilterMode == InMode
                            ? FSlateColor(CkStyle::Accent())
                            : FSlateColor(CkStyle::TextMute());
                    })
            ];
    };

    _ReboundCountText = SNew(STextBlock)
        .Font_Static(&ck_input_debugger::Font_Body)
        .ColorAndOpacity(CkStyle::Warn());

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [ _ReboundCountText.ToSharedRef() ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ MakeModeButton(TEXT("All"), ECkInputDebugger_BindingsFilterMode::All) ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ MakeModeButton(TEXT("Rebound"), ECkInputDebugger_BindingsFilterMode::ReboundOnly) ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ MakeModeButton(TEXT("Default"), ECkInputDebugger_BindingsFilterMode::DefaultOnly) ];
}

// --------------------------------------------------------------------------------------------------------------------
// Device snapshot composition
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    Get_DeviceSnapshot()
    -> const FCkDebug_DeviceSnapshot*
{
    if (GFrameCounter == _DeviceSnapshotFrame)
    { return &_DeviceSnapshot; }

    _DeviceSnapshotFrame = GFrameCounter;
    _DeviceSnapshot = FCkDebug_DeviceSnapshot{};

    if (_KeyObserver.IsValid())
    { _KeyObserver->Fill_DeviceSnapshot(_DeviceSnapshot); }

    for (const auto& MappedKey : _MappedKeys)
    {
        auto& State = _DeviceSnapshot.Keys.FindOrAdd(MappedKey);
        State.IsMinted = true;
        State.IsActionable = true;
    }

    for (const auto& ReboundKey : _ReboundKeys)
    { _DeviceSnapshot.Keys.FindOrAdd(ReboundKey).IsRebound = true; }

    if (_KeyFilter.IsValid())
    { _DeviceSnapshot.Keys.FindOrAdd(_KeyFilter).IsHighlighted = true; }

    return &_DeviceSnapshot;
}

// --------------------------------------------------------------------------------------------------------------------
// Tick
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _WorldModel->Ensure_AutoSelect();
    auto* World = _WorldModel->Get_SelectedWorld();

    auto PlayerLabel = FString{};
    auto NumLocalPlayers = 0;
    auto* Subsystem = ResolveSubsystem(World, PlayerLabel, NumLocalPlayers);
    _BoundSubsystem = Subsystem;

    RefreshPlayerSelector(NumLocalPlayers);

    // ---- No active player: show a placeholder once, then bail ----

    if (Subsystem == nullptr)
    {
        if (_LastSignature != TEXT("<none>"))
        {
            _LastSignature = TEXT("<none>");
            _ContextSlots.Reset();
            _ActionSlots.Reset();
            _ContextListBox->ClearChildren();
            _ResolvedListBox->ClearChildren();

            _ContextListBox->AddSlot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                [
                    SNew(STextBlock)
                        .Font_Static(&ck_input_debugger::Font_Body)
                        .Text(FText::FromString(TEXT("No active Enhanced Input player. Start PIE and possess a pawn.")))
                        .ColorAndOpacity(CkStyle::TextMute())
                ];
        }

        _SummaryText->SetText(FText::FromString(TEXT("No active Enhanced Input player.")));
        return;
    }

    // ---- Gather + rebuild structure only when the stack/mappings change ----

    auto Snapshot = FCkInputDebugger_Snapshot::Gather(Subsystem, PlayerLabel);

    if (Snapshot.StructuralSignature != _LastSignature)
    {
        _LastSignature = Snapshot.StructuralSignature;
        RebuildSections(Snapshot);
    }

    // ---- Bindings profile (default vs current) — its own cheap signature gate ----

    _BoundPlayerController = [&]() -> APlayerController*
    {
        const auto* LocalPlayer = Subsystem->GetLocalPlayer<ULocalPlayer>();
        return ck::IsValid(LocalPlayer, ck::IsValid_Policy_NullptrOnly{}) ? LocalPlayer->PlayerController : nullptr;
    }();

    if (auto Bindings = FCkInputDebugger_BindingsSnapshot::Gather(_BoundPlayerController.Get());
        Bindings.Signature != _LastBindingsSignature)
    {
        _LastBindingsSignature = Bindings.Signature;
        RebuildBindings(Bindings);
    }

    // ---- Held/recent key strip — updated in place only when the observer's sets change ----

    if (_KeyObserver.IsValid() && _KeyObserver->Get_ActivityRevision() != _LastActivityRevision)
    {
        _LastActivityRevision = _KeyObserver->Get_ActivityRevision();
        UpdateKeyStrip();
    }

    // The timeline re-supplies content every gated tick: open spans extend to the live edge and the
    // following view slides even when no new edge arrives.
    UpdateTimeline();

    UpdateLiveValues();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // Clearing the structural signature is this window's existing "rebuild the sections" lever —
    // the next gated tick re-runs BuildContextSlot / BuildActionSlot with the new selection.
    _LastSignature.Empty();
}

// --------------------------------------------------------------------------------------------------------------------
// Player resolution
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    ResolveSubsystem(
        UWorld* InWorld,
        FString& OutLabel,
        int32& OutNumLocalPlayers) const
    -> UEnhancedInputLocalPlayerSubsystem*
{
    OutLabel = FString{};
    OutNumLocalPlayers = 0;

    if (ck::Is_NOT_Valid(InWorld) || NOT InWorld->HasBegunPlay())
    { return nullptr; }

    auto* GameInstance = InWorld->GetGameInstance();

    if (ck::Is_NOT_Valid(GameInstance))
    { return nullptr; }

    const auto& LocalPlayers = GameInstance->GetLocalPlayers();
    OutNumLocalPlayers = LocalPlayers.Num();

    if (LocalPlayers.IsEmpty())
    { return nullptr; }

    const auto Index = LocalPlayers.IsValidIndex(_SelectedPlayerIndex) ? _SelectedPlayerIndex : 0;
    auto* LocalPlayer = LocalPlayers[Index];

    if (ck::Is_NOT_Valid(LocalPlayer))
    { return nullptr; }

    OutLabel = ck::Format_UE(TEXT("P{}"), Index);

    return LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
}

auto
    SCkInputDebuggerWindow::
    RefreshPlayerSelector(
        int32 InNumLocalPlayers)
    -> void
{
    if (NOT _PlayerSelectorBox.IsValid())
    { return; }

    if (InNumLocalPlayers == _LastNumLocalPlayers)
    { return; }

    _LastNumLocalPlayers = InNumLocalPlayers;
    _PlayerSelectorBox->ClearChildren();

    // Single (or zero) local player: a quiet static label, no buttons to pick from.
    if (InNumLocalPlayers <= 1)
    {
        _SelectedPlayerIndex = 0;

        _PlayerSelectorBox->AddSlot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Value)
                    .Text(FText::FromString(InNumLocalPlayers == 1 ? TEXT("P0") : TEXT("—")))
                    .ColorAndOpacity(CkStyle::TextDim())
            ];
        return;
    }

    if (_SelectedPlayerIndex >= InNumLocalPlayers)
    { _SelectedPlayerIndex = 0; }

    for (auto Index = 0; Index < InNumLocalPlayers; ++Index)
    {
        _PlayerSelectorBox->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(CkStyle::SpaceS, 2.0f))
                    .OnClicked_Lambda([this, Index]()
                    {
                        if (_SelectedPlayerIndex != Index)
                        {
                            _SelectedPlayerIndex = Index;
                            _LastSignature.Empty(); // force a rebuild for the new player
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Value)
                            .Text(FText::FromString(ck::Format_UE(TEXT("P{}"), Index)))
                            .ColorAndOpacity_Lambda([this, Index]()
                            {
                                return _SelectedPlayerIndex == Index
                                    ? FSlateColor(CkStyle::Accent())
                                    : FSlateColor(CkStyle::TextMute());
                            })
                    ]
            ];
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Structure rebuild (only on signature change)
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    RebuildSections(
        const FCkInputDebugger_Snapshot& InSnapshot)
    -> void
{
    _ContextListBox->ClearChildren();
    _ResolvedListBox->ClearChildren();
    _ContextSlots.Reset();
    _ActionSlots.Reset();

    // ---- Key lookup for the device visual + tooltips ----

    _MappedKeys.Reset();
    _ActionsByKey.Reset();

    for (const auto& Context : InSnapshot.Contexts)
    {
        for (const auto& Mapping : Context.Mappings)
        {
            _MappedKeys.Add(Mapping.Key);
            _ActionsByKey.FindOrAdd(Mapping.Key).AddUnique(Mapping.ActionName);
        }
    }

    // ---- Context stack ----

    if (InSnapshot.Contexts.IsEmpty())
    {
        _ContextListBox->AddSlot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Body)
                    .Text(FText::FromString(TEXT("No mapping contexts applied to this player.")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ];
    }
    else
    {
        for (const auto& Context : InSnapshot.Contexts)
        {
            BuildContextSlot(Context);
        }
    }

    // ---- Resolved bindings ----

    if (InSnapshot.ResolvedActions.IsEmpty())
    {
        _ResolvedListBox->AddSlot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Body)
                    .Text(FText::FromString(TEXT("No resolved action bindings.")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ];
    }
    else
    {
        for (const auto& Action : InSnapshot.ResolvedActions)
        {
            BuildActionSlot(Action);
        }
    }

    ApplyFilterAndHighlight();
}

auto
    SCkInputDebuggerWindow::
    BuildContextSlot(
        const FCkInputDebugger_ContextRow& InContext)
    -> void
{
    auto Slot = FCkInputDebugger_ContextSlot{};

    // Build a search string + a multi-line copy payload as we go.
    auto SearchText = InContext.ContextName;
    auto CopyText = ck::Format_UE(TEXT("{} (priority {})\n{}"),
        InContext.ContextName, InContext.Priority, InContext.ContextPath);

    // ---- Header ----

    Slot.NameText = SNew(STextBlock)
        .Font_Static(&ck_input_debugger::Font_RowLabel)
        .Text(FText::FromString(InContext.ContextName))
        .ColorAndOpacity(CkStyle::Text());

    auto Header =
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Value)
                    .Text(FText::FromString(ck::Format_UE(TEXT("[P{}]"), InContext.Priority)))
                    .ColorAndOpacity(CkStyle::Accent())
                    .ToolTipText(FText::FromString(TEXT("Stack priority (higher wins on key conflicts)")))
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [ Slot.NameText.ToSharedRef() ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Body)
                    .Text(FText::FromString(ck::Format_UE(TEXT("{} mappings"), InContext.Mappings.Num())))
                    .ColorAndOpacity(CkStyle::TextMute())
            ];

    // ---- Mapping rows ----

    auto BodyBox = SNew(SVerticalBox);

    Slot.MappingSlots.Reserve(InContext.Mappings.Num());

    for (auto MappingIdx = 0; MappingIdx < InContext.Mappings.Num(); ++MappingIdx)
    {
        const auto& Mapping = InContext.Mappings[MappingIdx];

        auto MSlot = FCkInputDebugger_MappingSlot{};
        MSlot.SearchText = Mapping.ActionName + TEXT(" ") + Mapping.KeyName;

        SearchText += TEXT(" ") + MSlot.SearchText;
        CopyText   += ck::Format_UE(TEXT("\n  {}  =  {}  ({}, T{} M{})"),
            Mapping.ActionName, Mapping.KeyName, Mapping.ValueType, Mapping.TriggerCount, Mapping.ModifierCount);

        const auto BgColor = (MappingIdx % 2 == 0) ? CkStyle::BgRoot() : CkStyle::Bg1();

        MSlot.Root =
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(BgColor)
            .Padding_Static(&ck_input_debugger::Get_MappingRowPadding)
            [
                SNew(SHorizontalBox)

                // All-fill columns so the key/type/count columns align down the whole context
                // (a trailing AutoWidth would shift every fill by that row's own text width).
                + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Value)
                            .Text(FText::FromString(Mapping.ActionName))
                            .ColorAndOpacity(CkStyle::Text())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().FillWidth(0.30f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Value)
                            .Text(FText::FromString(Mapping.KeyName))
                            .ColorAndOpacity(CkStyle::Accent())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().FillWidth(0.12f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Body)
                            .Text(FText::FromString(ck::Format_UE(TEXT("({})"), Mapping.ValueType)))
                            .ColorAndOpacity(CkStyle::TextDim())
                    ]

                + SHorizontalBox::Slot().FillWidth(0.16f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Body)
                            .Text(FText::FromString(ck::Format_UE(TEXT("T{} M{}"), Mapping.TriggerCount, Mapping.ModifierCount)))
                            .ColorAndOpacity(CkStyle::TextMute())
                            .ToolTipText(FText::FromString(TEXT("Trigger count / Modifier count on this mapping")))
                    ]
            ];

        BodyBox->AddSlot().AutoHeight()
            [ MSlot.Root.ToSharedRef() ];

        Slot.MappingSlots.Add(MoveTemp(MSlot));
    }

    Slot.SearchText = SearchText;

    Slot.ExpandableArea =
        SNew(SExpandableArea)
        .InitiallyCollapsed(false)
        .BorderBackgroundColor(CkStyle::BgRoot())
        .HeaderPadding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
        .HeaderContent()
        [
            SNew(SCkDebug_CopyableContainer)
                .CopyText(CopyText)
                [ Header ]
        ]
        .BodyContent()
        [ BodyBox ];

    _ContextListBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
        [ Slot.ExpandableArea.ToSharedRef() ];

    _ContextSlots.Add(MoveTemp(Slot));
}

auto
    SCkInputDebuggerWindow::
    BuildActionSlot(
        const FCkInputDebugger_ActionRow& InAction)
    -> void
{
    auto Slot = FCkInputDebugger_ActionSlot{};
    Slot.Action = InAction.Action;
    Slot.SearchText = InAction.ActionName + TEXT(" ") + InAction.KeysJoined;

    const auto CopyText = ck::Format_UE(TEXT("{}  [{}]  ({})"),
        InAction.ActionName, InAction.KeysJoined, InAction.ValueType);

    // A category swatch, not a pill: the activity is encoded by colour alone, with the trigger
    // column beside it carrying the state text. The dot's own brush stays white so the per-tick
    // tint below lands exactly on the role colour instead of multiplying with a base.
    Slot.ActivityDot = SNew(SCkDebug_CategoryDot).Diameter(8.0f);
    Slot.ActivityDot->SetColorAndOpacity(CkStyle::TextMute());

    Slot.NameText = SNew(STextBlock)
        .Font_Static(&ck_input_debugger::Font_RowLabel)
        .Text(FText::FromString(InAction.ActionName))
        .ColorAndOpacity(CkStyle::Text())
        .OverflowPolicy(ETextOverflowPolicy::Ellipsis);

    Slot.ValueText = SNew(STextBlock)
        .Font_Static(&ck_input_debugger::Font_Value)
        .Text(FText::FromString(TEXT("—")))
        .ColorAndOpacity(CkStyle::TextDim());

    Slot.TriggerText = SNew(STextBlock)
        .Font_Static(&ck_input_debugger::Font_Body)
        .Text(FText::FromString(TEXT("—")))
        .ColorAndOpacity(CkStyle::TextMute());

    const auto BgColor = (_ActionSlots.Num() % 2 == 0) ? CkStyle::BgRoot() : CkStyle::Bg1();

    Slot.Root =
        SNew(SCkDebug_CopyableContainer)
        .CopyText(CopyText)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(BgColor)
            .Padding_Static(&ck_input_debugger::Get_ActionRowPadding)
            [
                SNew(SHorizontalBox)

                // All-fill columns after the dot so name/keys/type/value/trigger align down the pane.
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [ Slot.ActivityDot.ToSharedRef() ]

                + SHorizontalBox::Slot().FillWidth(0.26f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                    [ Slot.NameText.ToSharedRef() ]

                + SHorizontalBox::Slot().FillWidth(0.40f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Value)
                            .Text(FText::FromString(InAction.KeysJoined))
                            .ColorAndOpacity(CkStyle::TextDim())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().FillWidth(0.10f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Body)
                            .Text(FText::FromString(ck::Format_UE(TEXT("({})"), InAction.ValueType)))
                            .ColorAndOpacity(CkStyle::TextMute())
                    ]

                + SHorizontalBox::Slot().FillWidth(0.12f).VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                    [ Slot.ValueText.ToSharedRef() ]

                + SHorizontalBox::Slot().FillWidth(0.12f).VAlign(VAlign_Center)
                    [ Slot.TriggerText.ToSharedRef() ]
            ]
        ];

    _ResolvedListBox->AddSlot().AutoHeight()
        [ Slot.Root.ToSharedRef() ];

    _ActionSlots.Add(MoveTemp(Slot));
}

// --------------------------------------------------------------------------------------------------------------------
// In-place live update (every gated tick)
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    UpdateLiveValues()
    -> void
{
    auto* Subsystem = _BoundSubsystem.Get();

    _SummaryText->SetText(FText::FromString(ck::Format_UE(
        TEXT("Contexts: {}   |   Resolved Actions: {}"),
        _ContextSlots.Num(), _ActionSlots.Num())));

    for (auto& Slot : _ActionSlots)
    {
        const auto* Action = Slot.Action.Get();
        const auto State = FCkInputDebugger_Snapshot::Gather_LiveActionState(Subsystem, Action);
        Slot.Activity = State.Activity;

        if (Slot.ValueText.IsValid())
        {
            Slot.ValueText->SetText(FText::FromString(State.ValueText));
            Slot.ValueText->SetColorAndOpacity(State.Magnitude > KINDA_SMALL_NUMBER
                ? CkStyle::TextStrong()
                : CkStyle::TextDim());
        }

        if (Slot.TriggerText.IsValid())
        {
            Slot.TriggerText->SetText(FText::FromString(State.TriggerEventText));
            Slot.TriggerText->SetColorAndOpacity(ck_input_debugger::ActivityColor(State.Activity));
        }

        if (Slot.ActivityDot.IsValid())
        {
            Slot.ActivityDot->SetColorAndOpacity(ck_input_debugger::ActivityColor(State.Activity));
        }
    }

    ApplyFilterAndHighlight();
}

// --------------------------------------------------------------------------------------------------------------------
// Filter + highlight (visibility / dim — no structural changes)
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    ApplyFilterAndHighlight()
    -> void
{
    // ---- Context stack ----

    for (auto& Ctx : _ContextSlots)
    {
        auto AnyMappingVisible = false;

        for (auto& Mapping : Ctx.MappingSlots)
        {
            if (NOT Mapping.Root.IsValid())
            { continue; }

            const auto Visible = MatchesFilter(Mapping.SearchText);
            Mapping.Root->SetVisibility(Visible ? EVisibility::Visible : EVisibility::Collapsed);
            AnyMappingVisible |= Visible;
        }

        const auto CtxVisible = _FilterString.IsEmpty() || MatchesFilter(Ctx.SearchText) || AnyMappingVisible;

        if (Ctx.ExpandableArea.IsValid())
        {
            Ctx.ExpandableArea->SetVisibility(CtxVisible ? EVisibility::Visible : EVisibility::Collapsed);
        }

        if (Ctx.NameText.IsValid())
        {
            Ctx.NameText->SetColorAndOpacity(MatchesHighlight(Ctx.SearchText)
                ? CkStyle::Text()
                : CkStyle::TextMute());
        }
    }

    // ---- Player bindings ----

    auto VisibleByCategory = TMap<FString, bool>{};

    for (auto& Binding : _BindingSlots)
    {
        if (NOT Binding.Root.IsValid())
        { continue; }

        const auto PassesMode =
            (_BindingsFilterMode == ECkInputDebugger_BindingsFilterMode::All) ||
            (_BindingsFilterMode == ECkInputDebugger_BindingsFilterMode::ReboundOnly && Binding.IsRebound) ||
            (_BindingsFilterMode == ECkInputDebugger_BindingsFilterMode::DefaultOnly && NOT Binding.IsRebound);

        const auto Visible = PassesMode && MatchesFilter(Binding.SearchText);

        Binding.Root->SetVisibility(Visible ? EVisibility::Visible : EVisibility::Collapsed);
        VisibleByCategory.FindOrAdd(Binding.Category) |= Visible;

        const auto Highlighted = NOT _HighlightString.IsEmpty() && MatchesHighlight(Binding.SearchText);

        Binding.Root->SetBorderBackgroundColor(Highlighted
            ? CkStyle::Accent().CopyWithNewOpacity(0.10f)
            : FLinearColor::Transparent);

        if (Binding.NameText.IsValid())
        {
            Binding.NameText->SetColorAndOpacity(MatchesHighlight(Binding.SearchText)
                ? CkStyle::Text()
                : CkStyle::TextMute());
        }
    }

    for (const auto& [Category, Header] : _BindingCategoryHeaders)
    {
        if (NOT Header.IsValid())
        { continue; }

        const auto* AnyVisible = VisibleByCategory.Find(Category);
        Header->SetVisibility(AnyVisible != nullptr && *AnyVisible ? EVisibility::Visible : EVisibility::Collapsed);
    }

    // ---- Resolved actions ----

    for (auto& Slot : _ActionSlots)
    {
        if (Slot.Root.IsValid())
        {
            const auto IsActive = Slot.Activity == ECkInputDebugger_ActionActivity::Active
                || Slot.Activity == ECkInputDebugger_ActionActivity::Ongoing;
            const auto IsVisible = MatchesFilter(Slot.SearchText)
                && (NOT _ShowActiveActionsOnly || IsActive);
            Slot.Root->SetVisibility(IsVisible ? EVisibility::Visible : EVisibility::Collapsed);
        }

        if (Slot.NameText.IsValid())
        {
            Slot.NameText->SetColorAndOpacity(MatchesHighlight(Slot.SearchText)
                ? CkStyle::Text()
                : CkStyle::TextMute());
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Live key strip + bindings pane
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    Get_KeyDisplay(
        const FKey& InKey) const
    -> FString
{
    return InKey.GetDisplayName().ToString();
}

auto
    SCkInputDebuggerWindow::
    EnsureKeyStripChips(
        int32 InCount)
    -> void
{
    if (NOT _KeyStripEmptyText.IsValid())
    {
        _KeyStripEmptyText = SNew(STextBlock)
            .Font_Static(&ck_input_debugger::Font_Body)
            .Text(FText::FromString(TEXT("Press something — held keys pin left, releases fade right.")))
            .ColorAndOpacity(CkStyle::TextMute());

        _KeyStripBox->AddSlot().AutoWidth().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [ _KeyStripEmptyText.ToSharedRef() ];
    }

    while (_KeyStripChips.Num() < InCount)
    {
        auto Chip = FCkInputDebugger_KeyChip{};

        Chip.KeyText = SNew(STextBlock)
            .Font_Static(&ck_input_debugger::Font_RowLabel);

        Chip.ActionText = SNew(STextBlock)
            .Font_Static(&ck_input_debugger::Font_Body);

        Chip.KeyBadge = SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush_Small())
            .Padding(FMargin(6.0f, 2.0f))
            [ Chip.KeyText.ToSharedRef() ];

        Chip.Root = SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .Padding(FMargin(CkStyle::SpaceS, CkStyle::SpaceS * 0.6f))
            .Visibility(EVisibility::Collapsed)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [ Chip.KeyBadge.ToSharedRef() ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ Chip.ActionText.ToSharedRef() ]
            ];

        _KeyStripBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [ Chip.Root.ToSharedRef() ];

        _KeyStripChips.Emplace(MoveTemp(Chip));
    }
}

auto
    SCkInputDebuggerWindow::
    UpdateKeyStrip()
    -> void
{
    static const auto EmptyHeld = TArray<FCkDebug_HeldKey>{};
    static const auto EmptyRecent = TArray<FCkDebug_RecentKey>{};
    const auto& Held = _KeyObserver.IsValid() ? _KeyObserver->Get_HeldKeys() : EmptyHeld;
    const auto& Recent = _KeyObserver.IsValid() ? _KeyObserver->Get_RecentKeys() : EmptyRecent;

    EnsureKeyStripChips(Held.Num() + Recent.Num());

    const auto SetChip = [this](int32 InChipIdx, const FKey& InKey, bool InIsHeld, float InOpacity) -> void
    {
        auto& Chip = _KeyStripChips[InChipIdx];

        auto ActionLines = FString{};
        if (const auto* Actions = _ActionsByKey.Find(InKey))
        { ActionLines = FString::Join(*Actions, TEXT(" · ")); }

        Chip.Root->SetVisibility(EVisibility::Visible);
        Chip.Root->SetBorderBackgroundColor((InIsHeld ? CkStyle::Bg3() : CkStyle::Bg2()).CopyWithNewOpacity(InOpacity));
        Chip.KeyBadge->SetBorderBackgroundColor(CkStyle::BgRoot().CopyWithNewOpacity(InOpacity));
        Chip.KeyText->SetText(FText::FromString(Get_KeyDisplay(InKey).ToUpper()));
        Chip.KeyText->SetColorAndOpacity((InIsHeld ? CkStyle::Accent() : CkStyle::TextDim()).CopyWithNewOpacity(InOpacity));
        Chip.ActionText->SetText(FText::FromString(ActionLines.IsEmpty() ? TEXT("(no action)") : ActionLines));
        Chip.ActionText->SetColorAndOpacity((ActionLines.IsEmpty() ? CkStyle::TextMute() : CkStyle::Text()).CopyWithNewOpacity(InOpacity));
    };

    auto ChipIdx = 0;

    for (const auto& HeldKey : Held)
    {
        constexpr auto FullOpacity = 1.0f;
        SetChip(ChipIdx++, HeldKey.Key, true, FullOpacity);
    }

    auto RecentOpacity = 0.65f;
    for (const auto& RecentKey : Recent)
    {
        SetChip(ChipIdx++, RecentKey.Key, false, RecentOpacity);
        RecentOpacity = FMath::Max(0.25f, RecentOpacity - 0.12f);
    }

    for (auto SpareIdx = ChipIdx; SpareIdx < _KeyStripChips.Num(); ++SpareIdx)
    { _KeyStripChips[SpareIdx].Root->SetVisibility(EVisibility::Collapsed); }

    if (_KeyStripEmptyText.IsValid())
    { _KeyStripEmptyText->SetVisibility(ChipIdx == 0 ? EVisibility::Visible : EVisibility::Collapsed); }
}

auto
    SCkInputDebuggerWindow::
    UpdateTimeline()
    -> void
{
    if (NOT _TimelineHost.IsValid() || NOT _KeyObserver.IsValid())
    { return; }

    const auto& Episodes = _KeyObserver->Get_EdgeHistory();
    const auto LiveFrame = _KeyObserver->Get_LiveFrame();

    // ---- Lane set: one lane per key witnessed in the ring, alphabetical (the Intent dock's button-lane rule) ----

    auto LaneKeys = TArray<FKey>{};
    for (const auto& Episode : Episodes)
    { LaneKeys.AddUnique(Episode.Key); }

    LaneKeys.Sort([this](const FKey& InA, const FKey& InB)
    { return Get_KeyDisplay(InA) < Get_KeyDisplay(InB); });

    auto Labels = TArray<FString>{};
    Labels.Reserve(LaneKeys.Num());
    for (const auto& LaneKey : LaneKeys)
    { Labels.Emplace(Get_KeyDisplay(LaneKey)); }

    auto Hash = GetTypeHash(Labels.Num());
    for (const auto& Label : Labels)
    { Hash = HashCombine(Hash, GetTypeHash(Label)); }

    // Lane labels are a construction argument, so a changed lane SET is the one sanctioned widget
    // rebuild — never mid-drag: the drag holds mouse capture on the widget a rebuild would destroy.
    // The hash stays stale, so the rebuild lands on the next refresh after the drag ends.
    const auto IsInteracting = _Timeline.IsValid() && _Timeline->Get_IsInteracting();
    const auto NeedsRebuild = (Hash != _TimelineStructureHash || NOT _Timeline.IsValid()) && NOT IsInteracting;

    // Lanes come and go with the ring, so a rebuild is routine — the user's pan/zoom must survive it.
    // Applied AFTER Set_Content below: a fresh widget's data range is 0..1 until then, and Set_View clamps.
    const auto CarriedViewStart = _Timeline.IsValid() ? _Timeline->Get_ViewStart() : 0.0;
    const auto CarriedViewDuration = _Timeline.IsValid() ? _Timeline->Get_ViewDuration() : 0.0;
    const auto CarriedFollow = NOT _Timeline.IsValid() || _Timeline->Get_IsFollowingLive();

    if (NeedsRebuild)
    {
        _TimelineStructureHash = Hash;
        _TimelineLaneLabels = Labels;
        _TimelineLaneKeys = LaneKeys;

        constexpr auto LaneHeight = 22.0f;
        constexpr auto MinTimelineHeight = 140.0f;
        constexpr auto InitialViewFrames = 600.0;

        _TimelineHost->SetContent(
            SAssignNew(_Timeline, SCkDebug_EventTimeline)
                .LaneLabels(_TimelineLaneLabels)
                .DesiredHeight(FMath::Max(
                    MinTimelineHeight,
                    static_cast<float>(_TimelineLaneLabels.Num()) * LaneHeight))
                .AllowPanZoom(true)
                .InitialViewDuration(InitialViewFrames)
                .SelectedId_Lambda([this]() -> int32
                {
                    return _TimelineLaneKeys.IndexOfByKey(_KeyFilter);
                })
                .OnEventSelected_Lambda([this](int32 InSelectionId)
                {
                    if (_TimelineLaneKeys.IsValidIndex(InSelectionId))
                    { HandleDeviceKeyClicked(_TimelineLaneKeys[InSelectionId]); }
                })
                .OnFormatTick_Lambda([](double InTime)
                {
                    return ck::Format_UE(TEXT("f{}"), FMath::RoundToInt32(InTime));
                }));
    }

    if (NOT _Timeline.IsValid())
    { return; }

    if (Episodes.IsEmpty())
    {
        _Timeline->Set_Content(0.0, 1.0, {}, {});
        return;
    }

    auto Events = TArray<FCkDebug_TimelineEvent>{};
    auto Spans = TArray<FCkDebug_TimelineSpan>{};
    auto TimeMin = static_cast<double>(Episodes[0].PressFrame);

    for (const auto& Episode : Episodes)
    {
        // A key that arrived while a drag deferred the lane rebuild has no lane yet — its episodes
        // join on the refresh that rebuilds.
        const auto LaneIndex = _TimelineLaneKeys.IndexOfByKey(Episode.Key);

        if (LaneIndex == INDEX_NONE)
        { continue; }

        TimeMin = FMath::Min(TimeMin, static_cast<double>(Episode.PressFrame));

        const auto IsOpen = Episode.ReleaseFrame == INDEX_NONE;
        const auto EndFrame = IsOpen ? LiveFrame : Episode.ReleaseFrame;
        const auto KeyLabel = _TimelineLaneLabels[LaneIndex];

        auto Span = FCkDebug_TimelineSpan{};
        Span.LaneIndex = LaneIndex;
        Span.StartSeconds = static_cast<double>(Episode.PressFrame);
        Span.EndSeconds = static_cast<double>(EndFrame) + 1.0;
        Span.Color = IsOpen ? CkStyle::Accent() : CkStyle::AccentDim();
        Span.Tooltip = IsOpen
            ? ck::Format_UE(TEXT("{} held since f{}"), KeyLabel, Episode.PressFrame)
            : ck::Format_UE(TEXT("{}  f{}..f{}  ({} frames)"),
                KeyLabel, Episode.PressFrame, Episode.ReleaseFrame,
                Episode.ReleaseFrame - Episode.PressFrame + 1);
        Spans.Add(MoveTemp(Span));

        auto Event = FCkDebug_TimelineEvent{};
        Event.LaneIndex = LaneIndex;
        Event.TimeSeconds = static_cast<double>(Episode.PressFrame);
        Event.Shape = ECkDebug_TimelineMarker::Diamond;
        Event.Color = CkStyle::Ok();
        Event.SelectionId = LaneIndex;
        Event.Tooltip = ck::Format_UE(TEXT("{} pressed @ f{} — click to filter the panes to this key"),
            KeyLabel, Episode.PressFrame);
        Events.Add(MoveTemp(Event));
    }

    _Timeline->Set_Content(TimeMin, FMath::Max(TimeMin + 1.0, static_cast<double>(LiveFrame)), Events, Spans);

    if (NeedsRebuild && CarriedViewDuration > 0.0)
    {
        _Timeline->Set_View(CarriedViewStart, CarriedViewDuration);
        _Timeline->Set_FollowLive(CarriedFollow);
    }
}

auto
    SCkInputDebuggerWindow::
    RebuildBindings(
        const FCkInputDebugger_BindingsSnapshot& InBindings)
    -> void
{
    _BindingsListBox->ClearChildren();
    _BindingSlots.Reset();
    _BindingCategoryHeaders.Reset();
    _ReboundKeys.Reset();

    if (InBindings.CategoryOrder.IsEmpty())
    {
        if (_ReboundCountText.IsValid())
        { _ReboundCountText->SetText(FText::GetEmpty()); }

        _BindingsListBox->AddSlot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Body)
                    .Text(FText::FromString(TEXT("No player-mappable bindings registered (no Enhanced Input user-settings profile rows).")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ];
        return;
    }

    const auto MakeKeyCap = [](const FKey& InKey, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush_Small())
            .BorderBackgroundColor(CkStyle::BgRoot())
            .Padding(FMargin(6.0f, 1.0f))
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Value)
                    .Text(FText::FromString(InKey.IsValid() ? InKey.GetDisplayName().ToString() : TEXT("—")))
                    .ColorAndOpacity(InColor)
            ];
    };

    auto ReboundCount = 0;

    for (const auto& Category : InBindings.CategoryOrder)
    {
        const auto CategoryHeader = SNew(STextBlock)
            .Font_Static(&ck_input_debugger::Font_RowLabel)
            .Text(FText::FromString(Category.IsEmpty() ? TEXT("(uncategorized)") : Category))
            .ColorAndOpacity(CkStyle::Accent());

        _BindingCategoryHeaders.Emplace(Category, CategoryHeader);

        _BindingsListBox->AddSlot().AutoHeight().Padding(CkStyle::SpaceS, CkStyle::SpaceS, CkStyle::SpaceS, 2.0f)
            [ CategoryHeader ];

        for (const auto& Row : InBindings.RowsByCategory[Category])
        {
            if (Row.IsRebound)
            { ++ReboundCount; }

            if (Row.IsRebound && Row.CurrentKey.IsValid())
            { _ReboundKeys.Add(Row.CurrentKey); }

            auto Slot = FCkInputDebugger_BindingSlot{};
            Slot.Category = Category;
            Slot.IsRebound = Row.IsRebound;
            Slot.SearchText = ck::Format_UE(TEXT("{} {} {} {} {}"),
                Row.DisplayName, Row.MappingName,
                Row.DefaultKey.GetDisplayName().ToString(),
                Row.CurrentKey.GetDisplayName().ToString(),
                Row.ScopeTags).ToLower();

            Slot.NameText = SNew(STextBlock)
                .Font_Static(&ck_input_debugger::Font_Body)
                .Text(FText::FromString(Row.DisplayName.IsEmpty() ? Row.MappingName.ToString() : Row.DisplayName))
                .ColorAndOpacity(CkStyle::Text())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis);

            // Every column is a fixed fill fraction with left-aligned content: rows divide their
            // width identically, so the columns line up down the whole pane (an AutoWidth key cap
            // or a sometimes-collapsed badge would shift every fill after it, per row).
            const auto RowWidget = SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor::Transparent)
                .Padding(ck_input_debugger::Get_ActionRowPadding())
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().FillWidth(0.20f).VAlign(VAlign_Center)
                        [ Slot.NameText.ToSharedRef() ]

                    + SHorizontalBox::Slot().FillWidth(0.18f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Font_Static(&ck_input_debugger::Font_Value)
                                .Text(FText::FromString(Row.MappingName.ToString()))
                                .ColorAndOpacity(CkStyle::TextMute())
                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]

                    + SHorizontalBox::Slot().FillWidth(0.30f).VAlign(VAlign_Center).HAlign(HAlign_Left)
                        [
                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [ MakeKeyCap(Row.DefaultKey, CkStyle::TextDim()) ]

                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f)
                                [
                                    SNew(STextBlock)
                                        .Font_Static(&ck_input_debugger::Font_Body)
                                        .Text(FText::FromString(TEXT("→")))
                                        .ColorAndOpacity(CkStyle::TextMute())
                                ]

                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [ MakeKeyCap(Row.CurrentKey, Row.IsRebound ? CkStyle::Warn() : CkStyle::Text()) ]
                        ]

                    + SHorizontalBox::Slot().FillWidth(0.08f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Font_Static(&ck_input_debugger::Font_Body)
                                .Text(FText::FromString(Row.IsGamepad ? TEXT("Gamepad") : TEXT("KBM")))
                                .ColorAndOpacity(CkStyle::TextMute())
                        ]

                    + SHorizontalBox::Slot().FillWidth(0.16f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Font_Static(&ck_input_debugger::Font_Body)
                                .Text(FText::FromString(Row.ScopeTags))
                                .ColorAndOpacity(CkStyle::TextMute())
                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]

                    + SHorizontalBox::Slot().FillWidth(0.08f).VAlign(VAlign_Center).HAlign(HAlign_Left)
                        [
                            SNew(SBorder)
                                .BorderImage(CkStyle::GetRoundedBrush_Small())
                                .BorderBackgroundColor(CkStyle::Warn().CopyWithNewOpacity(0.16f))
                                .Padding(FMargin(5.0f, 1.0f))
                                .Visibility(Row.IsRebound ? EVisibility::Visible : EVisibility::Hidden)
                                [
                                    SNew(STextBlock)
                                        .Font_Static(&ck_input_debugger::Font_Value)
                                        .Text(FText::FromString(TEXT("REBOUND")))
                                        .ColorAndOpacity(CkStyle::Warn())
                                ]
                        ]
                ];

            Slot.Root = RowWidget;
            _BindingSlots.Emplace(MoveTemp(Slot));

            _BindingsListBox->AddSlot().AutoHeight() [ RowWidget ];
        }
    }

    if (_ReboundCountText.IsValid())
    {
        _ReboundCountText->SetText(ReboundCount > 0
            ? FText::FromString(ck::Format_UE(TEXT("{} rebound"), ReboundCount))
            : FText::GetEmpty());
    }

    ApplyFilterAndHighlight();
}

auto
    SCkInputDebuggerWindow::
    Get_KeyTooltip(
        const FKey& InKey) const
    -> FText
{
    auto Lines = TArray<FString>{};
    Lines.Emplace(Get_KeyDisplay(InKey));

    if (const auto* Actions = _ActionsByKey.Find(InKey))
    {
        for (const auto& ActionName : *Actions)
        { Lines.Emplace(ck::Format_UE(TEXT("  {}"), ActionName)); }
    }
    else
    { Lines.Emplace(TEXT("  (no mapped action)")); }

    if (_ReboundKeys.Contains(InKey))
    { Lines.Emplace(TEXT("  rebound from default")); }

    Lines.Emplace(TEXT("Click to filter the panes below to this key."));

    return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

auto
    SCkInputDebuggerWindow::
    HandleDeviceKeyClicked(
        const FKey& InKey)
    -> void
{
    _KeyFilter = (_KeyFilter == InKey) ? FKey{} : InKey;
    ApplyFilterAndHighlight();
}

// --------------------------------------------------------------------------------------------------------------------
// Search predicates
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebuggerWindow::
    MatchesFilter(
        const FString& InSearchText) const
    -> bool
{
    // The device-visual key filter composes WITH the text filter: search strings carry the key
    // display names, so containment is the match.
    if (_KeyFilter.IsValid() &&
        NOT InSearchText.Contains(Get_KeyDisplay(_KeyFilter), ESearchCase::IgnoreCase))
    { return false; }

    if (_FilterString.IsEmpty())
    { return true; }

    return ck::fuzzy::Match(_FilterString, InSearchText, {}).Get_IsMatch();
}

auto
    SCkInputDebuggerWindow::
    MatchesHighlight(
        const FString& InSearchText) const
    -> bool
{
    if (_HighlightString.IsEmpty())
    { return true; }

    return ck::fuzzy::Match(_HighlightString, InSearchText, {}).Get_IsMatch();
}

// --------------------------------------------------------------------------------------------------------------------
