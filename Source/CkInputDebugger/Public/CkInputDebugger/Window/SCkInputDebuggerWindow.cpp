#include "CkInputDebugger/Window/SCkInputDebuggerWindow.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CopyableContainer.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Local style constants (mirrors the other CK debugger windows)
// --------------------------------------------------------------------------------------------------------------------

namespace style
{
    static constexpr auto Pad_S = 4.0f;
    static constexpr auto Pad_M = 8.0f;
    static constexpr auto Pad_L = 16.0f;

    static const auto Bg_Dark   = FLinearColor(0.01f, 0.01f, 0.01f);
    static const auto Bg_Medium = FLinearColor(0.025f, 0.025f, 0.025f);

    static const auto Text_Primary   = FLinearColor(0.85f, 0.85f, 0.85f);
    static const auto Text_Secondary = FLinearColor(0.6f, 0.6f, 0.6f);
    static const auto Text_Muted     = FLinearColor(0.35f, 0.35f, 0.35f);
    static const auto Text_Highlight = FLinearColor(0.95f, 0.95f, 0.95f);

    static const auto Accent_Cyan    = FLinearColor(0.51f, 0.69f, 1.0f);
    static const auto Accent_Warning = FLinearColor(1.0f, 0.8f, 0.01f);
    static const auto Accent_Success = FLinearColor(0.25f, 0.75f, 0.25f);
    static const auto Accent_Error   = FLinearColor(0.9f, 0.25f, 0.25f);

    static auto Normal(int32 InSize = 9) -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Regular", InSize); }
    static auto Bold(int32 InSize = 9)   -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Bold", InSize); }
    static auto Mono(int32 InSize = 9)   -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Mono", InSize); }
}

// --------------------------------------------------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_debugger
{
    static auto ActivityColor(ECkInputDebugger_ActionActivity InActivity) -> FLinearColor
    {
        switch (InActivity)
        {
            case ECkInputDebugger_ActionActivity::Active:    return style::Accent_Success;
            case ECkInputDebugger_ActionActivity::Ongoing:   return style::Accent_Warning;
            case ECkInputDebugger_ActionActivity::Canceled:  return style::Accent_Error;
            case ECkInputDebugger_ActionActivity::Completed: return style::Accent_Cyan;
            case ECkInputDebugger_ActionActivity::Idle:      return style::Text_Muted;
            case ECkInputDebugger_ActionActivity::NoInstance:
            default:                                         return style::Text_Muted;
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
        .Font(style::Bold(10))
        .ColorAndOpacity(style::Text_Primary);

    _ContextListBox  = SNew(SVerticalBox);
    _ResolvedListBox = SNew(SVerticalBox);

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
        .BorderBackgroundColor(style::Bg_Medium)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()
                [ BuildToolbar() ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
                [ _SummaryText.ToSharedRef() ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_M, 0.0f)
                [ SNew(SSeparator).Thickness(1.0f) ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)

                    + SScrollBox::Slot().Padding(style::Pad_S)
                        [
                            SNew(SVerticalBox)

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, style::Pad_S)
                                [
                                    SNew(SCkDebug_SectionHeader)
                                        .Label(FText::FromString(TEXT("Mapping Context Stack")))
                                ]

                            + SVerticalBox::Slot().AutoHeight()
                                [ _ContextListBox.ToSharedRef() ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, style::Pad_M)
                                [ SNew(SSeparator).Thickness(1.0f) ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, style::Pad_S)
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
        .BorderBackgroundColor(style::Bg_Dark)
        .Padding(FMargin(style::Pad_S))
        [
            SNew(SHorizontalBox)

            // World selector (shared across all CK debuggers)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_S, 0.0f)
                [
                    SNew(SCkDebug_WorldSelector, _WorldModel)
                        .ShowHeaderLabel(false)
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_M, 0.0f, style::Pad_S, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Player:")))
                        .Font(style::Normal())
                        .ColorAndOpacity(style::Text_Secondary)
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

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_M, 0.0f, 0.0f, 0.0f)
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
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

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

            _ContextListBox->AddSlot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
                [
                    SNew(STextBlock)
                        .Font(style::Normal())
                        .Text(FText::FromString(TEXT("No active Enhanced Input player. Start PIE and possess a pawn.")))
                        .ColorAndOpacity(style::Text_Muted)
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

    UpdateLiveValues();
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
                    .Font(style::Mono())
                    .Text(FText::FromString(InNumLocalPlayers == 1 ? TEXT("P0") : TEXT("—")))
                    .ColorAndOpacity(style::Text_Secondary)
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
                    .ContentPadding(FMargin(style::Pad_S, 2.0f))
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
                            .Font(style::Mono())
                            .Text(FText::FromString(ck::Format_UE(TEXT("P{}"), Index)))
                            .ColorAndOpacity_Lambda([this, Index]()
                            {
                                return _SelectedPlayerIndex == Index
                                    ? FSlateColor(style::Accent_Cyan)
                                    : FSlateColor(style::Text_Muted);
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

    // ---- Context stack ----

    if (InSnapshot.Contexts.IsEmpty())
    {
        _ContextListBox->AddSlot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
            [
                SNew(STextBlock)
                    .Font(style::Normal())
                    .Text(FText::FromString(TEXT("No mapping contexts applied to this player.")))
                    .ColorAndOpacity(style::Text_Muted)
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
        _ResolvedListBox->AddSlot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
            [
                SNew(STextBlock)
                    .Font(style::Normal())
                    .Text(FText::FromString(TEXT("No resolved action bindings.")))
                    .ColorAndOpacity(style::Text_Muted)
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
        .Font(style::Bold())
        .Text(FText::FromString(InContext.ContextName))
        .ColorAndOpacity(style::Text_Primary);

    auto Header =
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, style::Pad_M, 0.0f)
            [
                SNew(STextBlock)
                    .Font(style::Mono())
                    .Text(FText::FromString(ck::Format_UE(TEXT("[P{}]"), InContext.Priority)))
                    .ColorAndOpacity(style::Accent_Cyan)
                    .ToolTipText(FText::FromString(TEXT("Stack priority (higher wins on key conflicts)")))
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, style::Pad_M, 0.0f)
            [ Slot.NameText.ToSharedRef() ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Font(style::Normal())
                    .Text(FText::FromString(ck::Format_UE(TEXT("{} mappings"), InContext.Mappings.Num())))
                    .ColorAndOpacity(style::Text_Muted)
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

        const auto BgColor = (MappingIdx % 2 == 0) ? style::Bg_Dark : style::Bg_Medium;

        MSlot.Root =
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(BgColor)
            .Padding(FMargin(style::Pad_L, style::Pad_S, style::Pad_M, style::Pad_S))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font(style::Mono())
                            .Text(FText::FromString(Mapping.ActionName))
                            .ColorAndOpacity(style::Text_Primary)
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().FillWidth(0.32f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font(style::Mono())
                            .Text(FText::FromString(Mapping.KeyName))
                            .ColorAndOpacity(style::Accent_Cyan)
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_M, 0.0f)
                    [
                        SNew(STextBlock)
                            .Font(style::Normal())
                            .Text(FText::FromString(ck::Format_UE(TEXT("({})"), Mapping.ValueType)))
                            .ColorAndOpacity(style::Text_Secondary)
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font(style::Normal())
                            .Text(FText::FromString(ck::Format_UE(TEXT("T{} M{}"), Mapping.TriggerCount, Mapping.ModifierCount)))
                            .ColorAndOpacity(style::Text_Muted)
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
        .BorderBackgroundColor(style::Bg_Dark)
        .HeaderPadding(FMargin(style::Pad_M, style::Pad_S))
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

    Slot.ActivityDot = SNew(SImage)
        .Image(FAppStyle::GetBrush("Icons.FilledCircle"))
        .DesiredSizeOverride(FVector2D(8.0f, 8.0f))
        .ColorAndOpacity(FSlateColor(style::Text_Muted));

    Slot.NameText = SNew(STextBlock)
        .Font(style::Bold())
        .Text(FText::FromString(InAction.ActionName))
        .ColorAndOpacity(style::Text_Primary)
        .OverflowPolicy(ETextOverflowPolicy::Ellipsis);

    Slot.ValueText = SNew(STextBlock)
        .Font(style::Mono())
        .Text(FText::FromString(TEXT("—")))
        .ColorAndOpacity(style::Text_Secondary);

    Slot.TriggerText = SNew(STextBlock)
        .Font(style::Normal())
        .Text(FText::FromString(TEXT("—")))
        .ColorAndOpacity(style::Text_Muted);

    const auto BgColor = (_ActionSlots.Num() % 2 == 0) ? style::Bg_Dark : style::Bg_Medium;

    Slot.Root =
        SNew(SCkDebug_CopyableContainer)
        .CopyText(CopyText)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(BgColor)
            .Padding(FMargin(style::Pad_M, style::Pad_S))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, style::Pad_S, 0.0f)
                    [ Slot.ActivityDot.ToSharedRef() ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, style::Pad_M, 0.0f)
                    [ SNew(SBox).MinDesiredWidth(150.0f) [ Slot.NameText.ToSharedRef() ] ]

                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Font(style::Mono())
                            .Text(FText::FromString(InAction.KeysJoined))
                            .ColorAndOpacity(style::Text_Secondary)
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_M, 0.0f)
                    [
                        SNew(STextBlock)
                            .Font(style::Normal())
                            .Text(FText::FromString(ck::Format_UE(TEXT("({})"), InAction.ValueType)))
                            .ColorAndOpacity(style::Text_Muted)
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(style::Pad_M, 0.0f, style::Pad_S, 0.0f)
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
                ? style::Text_Highlight
                : style::Text_Secondary);
        }

        if (Slot.TriggerText.IsValid())
        {
            Slot.TriggerText->SetText(FText::FromString(State.TriggerEventText));
            Slot.TriggerText->SetColorAndOpacity(ck_input_debugger::ActivityColor(State.Activity));
        }

        if (Slot.ActivityDot.IsValid())
        {
            Slot.ActivityDot->SetColorAndOpacity(FSlateColor(ck_input_debugger::ActivityColor(State.Activity)));
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
                ? style::Text_Primary
                : style::Text_Muted);
        }
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
                ? style::Text_Primary
                : style::Text_Muted);
        }
    }
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
