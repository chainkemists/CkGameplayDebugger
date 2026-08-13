#include "CkInputDebugger/Window/SCkInputDebuggerWindow.h"

#include "CkInputDebugger/Window/SCkInputDebugger_DeviceVisual.h"

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
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Styling/AppStyle.h"
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

    // Passive application-wide observer for the live surfaces — every handler returns false, so
    // it can never starve viewport input (ck-slate-tools §3).
    if (FSlateApplication::IsInitialized())
    {
        _KeyObserver = MakeShared<FCkInputDebugger_KeyActivityObserver>();
        FSlateApplication::Get().RegisterInputPreProcessor(_KeyObserver);
    }

#if WITH_EDITOR
    _EndPIEHandle = FEditorDelegates::EndPIE.AddSP(this, &SCkInputDebuggerWindow::OnEndPIE);
#endif

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TEXT("CkInputDebugger")).DisplayName(Get_WindowDisplayName())
        .MenuActionsContent()
        [
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
                    })}
            })
        ]
        .Content()
        [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::Bg1())
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()
                [ BuildToolbar() ]

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

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                                [
                                    SNew(SCkDebug_SectionHeader)
                                        .Label(FText::FromString(TEXT("Held & Recent Keys")))
                                ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                                [ _KeyStripBox.ToSharedRef() ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                                [
                                    SNew(SCkDebug_SectionHeader)
                                        .Label(FText::FromString(TEXT("Devices")))
                                ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                                [
                                    SNew(SCkInputDebugger_DeviceVisual)
                                        .IsKeyPressed_Lambda([this](const FKey& InKey) { return _KeyObserver.IsValid() && _KeyObserver->Get_IsHeld(InKey); })
                                        .IsKeyMapped_Lambda([this](const FKey& InKey) { return _MappedKeys.Contains(InKey); })
                                        .IsKeyRebound_Lambda([this](const FKey& InKey) { return _ReboundKeys.Contains(InKey); })
                                        .IsKeyFiltered_Lambda([this](const FKey& InKey) { return _KeyFilter.IsValid() && _KeyFilter == InKey; })
                                        .KeyTooltip_Lambda([this](const FKey& InKey) { return Get_KeyTooltip(InKey); })
                                        .OnKeyClicked_Lambda([this](const FKey& InKey) { HandleDeviceKeyClicked(InKey); })
                                ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM)
                                [ ck::debug_axes::Make_AxisSeparator() ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                                [
                                    SNew(SCkDebug_SectionHeader)
                                        .Label(FText::FromString(TEXT("Player Bindings — default vs current")))
                                ]

                            + SVerticalBox::Slot().AutoHeight()
                                [ _BindingsListBox.ToSharedRef() ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM)
                                [ ck::debug_axes::Make_AxisSeparator() ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                                [
                                    SNew(SCkDebug_SectionHeader)
                                        .Label(FText::FromString(TEXT("Mapping Context Stack")))
                                ]

                            + SVerticalBox::Slot().AutoHeight()
                                [ _ContextListBox.ToSharedRef() ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM)
                                [ ck::debug_axes::Make_AxisSeparator() ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                                [
                                    SNew(SCkDebug_SectionHeader)
                                        .Label(FText::FromString(TEXT("Resolved Bindings (live)")))
                                ]

                            + SVerticalBox::Slot().AutoHeight()
                                [ _ResolvedListBox.ToSharedRef() ]
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

            + SHorizontalBox::Slot().FillWidth(1.0f)

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

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebugger_RefreshControls)
                        .WindowId(SCkInputDebuggerWindow::WindowId)
                ]
        ];
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

    // ---- Held/recent key strip — rebuilt only when the observer's sets change ----

    if (_KeyObserver.IsValid() && _KeyObserver->Get_ActivityRevision() != _LastActivityRevision)
    {
        _LastActivityRevision = _KeyObserver->Get_ActivityRevision();
        RebuildKeyStrip();
    }

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

                + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Value)
                            .Text(FText::FromString(Mapping.ActionName))
                            .ColorAndOpacity(CkStyle::Text())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().FillWidth(0.32f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Value)
                            .Text(FText::FromString(Mapping.KeyName))
                            .ColorAndOpacity(CkStyle::Accent())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Body)
                            .Text(FText::FromString(ck::Format_UE(TEXT("({})"), Mapping.ValueType)))
                            .ColorAndOpacity(CkStyle::TextDim())
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
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

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [ Slot.ActivityDot.ToSharedRef() ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                    [ SNew(SBox).MinDesiredWidth(150.0f) [ Slot.NameText.ToSharedRef() ] ]

                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Value)
                            .Text(FText::FromString(InAction.KeysJoined))
                            .ColorAndOpacity(CkStyle::TextDim())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Body)
                            .Text(FText::FromString(ck::Format_UE(TEXT("({})"), InAction.ValueType)))
                            .ColorAndOpacity(CkStyle::TextMute())
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceS, 0.0f)
                    [ SNew(SBox).MinDesiredWidth(70.0f).HAlign(HAlign_Right) [ Slot.ValueText.ToSharedRef() ] ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ SNew(SBox).MinDesiredWidth(80.0f) [ Slot.TriggerText.ToSharedRef() ] ]
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

    for (auto& Binding : _BindingSlots)
    {
        if (NOT Binding.Root.IsValid())
        { continue; }

        Binding.Root->SetVisibility(MatchesFilter(Binding.SearchText)
            ? EVisibility::Visible
            : EVisibility::Collapsed);
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
    RebuildKeyStrip()
    -> void
{
    _KeyStripBox->ClearChildren();

    const auto MakeChip = [this](const FKey& InKey, bool InIsHeld, float InOpacity) -> TSharedRef<SWidget>
    {
        auto ActionLines = FString{};
        if (const auto* Actions = _ActionsByKey.Find(InKey))
        { ActionLines = FString::Join(*Actions, TEXT(" · ")); }

        return SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor((InIsHeld ? CkStyle::Bg3() : CkStyle::Bg2()).CopyWithNewOpacity(InOpacity))
            .Padding(FMargin(CkStyle::SpaceS, CkStyle::SpaceS * 0.6f))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(CkStyle::GetRoundedBrush_Small())
                            .BorderBackgroundColor(CkStyle::BgRoot().CopyWithNewOpacity(InOpacity))
                            .Padding(FMargin(6.0f, 2.0f))
                            [
                                SNew(STextBlock)
                                    .Font_Static(&ck_input_debugger::Font_RowLabel)
                                    .Text(FText::FromString(Get_KeyDisplay(InKey).ToUpper()))
                                    .ColorAndOpacity((InIsHeld ? CkStyle::Accent() : CkStyle::TextDim()).CopyWithNewOpacity(InOpacity))
                            ]
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font_Static(&ck_input_debugger::Font_Body)
                            .Text(FText::FromString(ActionLines.IsEmpty() ? TEXT("(no action)") : ActionLines))
                            .ColorAndOpacity((ActionLines.IsEmpty() ? CkStyle::TextMute() : CkStyle::Text()).CopyWithNewOpacity(InOpacity))
                    ]
            ];
    };

    auto AnyChip = false;

    if (_KeyObserver.IsValid())
    {
        for (const auto& Held : _KeyObserver->Get_HeldKeys())
        {
            constexpr auto FullOpacity = 1.0f;
            _KeyStripBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ MakeChip(Held.Key, true, FullOpacity) ];
            AnyChip = true;
        }

        auto RecentOpacity = 0.65f;
        for (const auto& Recent : _KeyObserver->Get_RecentKeys())
        {
            _KeyStripBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ MakeChip(Recent.Key, false, RecentOpacity) ];
            RecentOpacity = FMath::Max(0.25f, RecentOpacity - 0.12f);
            AnyChip = true;
        }
    }

    if (NOT AnyChip)
    {
        _KeyStripBox->AddSlot().AutoWidth().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_Body)
                    .Text(FText::FromString(TEXT("Press something — held keys pin left, releases fade right.")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ];
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
    _ReboundKeys.Reset();

    if (InBindings.CategoryOrder.IsEmpty())
    {
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

    for (const auto& Category : InBindings.CategoryOrder)
    {
        _BindingsListBox->AddSlot().AutoHeight().Padding(CkStyle::SpaceS, CkStyle::SpaceS, CkStyle::SpaceS, 2.0f)
            [
                SNew(STextBlock)
                    .Font_Static(&ck_input_debugger::Font_RowLabel)
                    .Text(FText::FromString(Category.IsEmpty() ? TEXT("(uncategorized)") : Category))
                    .ColorAndOpacity(CkStyle::Accent())
            ];

        for (const auto& Row : InBindings.RowsByCategory[Category])
        {
            if (Row.IsRebound && Row.CurrentKey.IsValid())
            { _ReboundKeys.Add(Row.CurrentKey); }

            auto Slot = FCkInputDebugger_BindingSlot{};
            Slot.SearchText = ck::Format_UE(TEXT("{} {} {} {} {}"),
                Row.DisplayName, Row.MappingName,
                Row.DefaultKey.GetDisplayName().ToString(),
                Row.CurrentKey.GetDisplayName().ToString(),
                Row.ScopeTags).ToLower();

            const auto RowWidget = SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor::Transparent)
                .Padding(ck_input_debugger::Get_ActionRowPadding())
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().FillWidth(0.34f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Font_Static(&ck_input_debugger::Font_Body)
                                .Text(FText::FromString(Row.DisplayName.IsEmpty() ? Row.MappingName.ToString() : Row.DisplayName))
                                .ColorAndOpacity(CkStyle::Text())
                        ]

                    + SHorizontalBox::Slot().FillWidth(0.22f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Font_Static(&ck_input_debugger::Font_Value)
                                .Text(FText::FromString(Row.MappingName.ToString()))
                                .ColorAndOpacity(CkStyle::TextMute())
                        ]

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

                    + SHorizontalBox::Slot().FillWidth(0.12f).VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Font_Static(&ck_input_debugger::Font_Body)
                                .Text(FText::FromString(Row.IsGamepad ? TEXT("Gamepad") : TEXT("KBM")))
                                .ColorAndOpacity(CkStyle::TextMute())
                        ]

                    + SHorizontalBox::Slot().FillWidth(0.2f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Font_Static(&ck_input_debugger::Font_Body)
                                .Text(FText::FromString(Row.ScopeTags))
                                .ColorAndOpacity(CkStyle::TextMute())
                        ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(SBorder)
                                .BorderImage(CkStyle::GetRoundedBrush_Small())
                                .BorderBackgroundColor(CkStyle::Warn().CopyWithNewOpacity(0.16f))
                                .Padding(FMargin(5.0f, 1.0f))
                                .Visibility(Row.IsRebound ? EVisibility::Visible : EVisibility::Collapsed)
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
