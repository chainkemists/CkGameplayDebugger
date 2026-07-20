#include "CkUIDebugger/Window/SCkUIDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NameDepthCycler.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"
#include "CkUI/Layout/CkUI_Layout_Subsystem.h"
#include "CkUI/Layout/CkUI_PrimaryGameLayout.h"
#include "CkUI/Layout/CkUI_LayerStack.h"

#include "CommonActivatableWidget.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Local style constants
// --------------------------------------------------------------------------------------------------------------------

namespace style
{
    static constexpr auto Pad_S = 4.0f;
    static constexpr auto Pad_M = 8.0f;
    static constexpr auto Pad_L = 16.0f;

    static const auto Bg_Dark       = FLinearColor(0.01f, 0.01f, 0.01f);
    static const auto Bg_Medium     = FLinearColor(0.025f, 0.025f, 0.025f);

    static const auto Text_Primary   = FLinearColor(0.85f, 0.85f, 0.85f);
    static const auto Text_Secondary = FLinearColor(0.6f, 0.6f, 0.6f);
    static const auto Text_Muted     = FLinearColor(0.35f, 0.35f, 0.35f);
    static const auto Text_Highlight = FLinearColor(0.95f, 0.95f, 0.95f);

    static const auto Accent_Cyan    = FLinearColor(0.51f, 0.69f, 1.0f);
    static const auto Accent_Warning = FLinearColor(1.0f, 0.8f, 0.01f);
    static const auto Accent_Success = FLinearColor(0.25f, 0.75f, 0.25f);

    static auto Normal(int32 InSize = 9)  -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Regular", InSize); }
    static auto Bold(int32 InSize = 9)    -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Bold", InSize); }
    static auto Mono(int32 InSize = 9)    -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Mono", InSize); }
}

// --------------------------------------------------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------------------------------------------------

namespace ck_ui_debugger
{
    static auto InputModeToString(ECk_UI_InputMode InMode) -> FString
    {
        switch (InMode)
        {
            case ECk_UI_InputMode::GameOnly:   return TEXT("GameOnly");
            case ECk_UI_InputMode::GameAndUI:  return TEXT("GameAndUI");
            case ECk_UI_InputMode::UIOnly:     return TEXT("UIOnly");
            default:                           return TEXT("Unknown");
        }
    }

    static auto FindLayoutSubsystem() -> UCk_UI_Layout_Subsystem_UE*
    {
        if (NOT GEngine)
        { return nullptr; }

        for (const auto& Context : GEngine->GetWorldContexts())
        {
            auto* World = Context.World();

            if (ck::Is_NOT_Valid(World))
            { continue; }

            if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
            { continue; }

            auto* GameInstance = World->GetGameInstance();

            if (ck::Is_NOT_Valid(GameInstance))
            { continue; }

            const auto& LocalPlayers = GameInstance->GetLocalPlayers();

            if (LocalPlayers.IsEmpty())
            { continue; }

            auto* LocalPlayer = LocalPlayers[0];

            if (ck::Is_NOT_Valid(LocalPlayer))
            { continue; }

            auto* LayoutSubsystem = LocalPlayer->GetSubsystem<UCk_UI_Layout_Subsystem_UE>();

            if (ck::IsValid(LayoutSubsystem) && LayoutSubsystem->Has_Layout())
            { return LayoutSubsystem; }
        }

        return nullptr;
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------------------------------------------------

SCkUIDebuggerWindow::~SCkUIDebuggerWindow()
{
    DoUnbindLayoutEvents();
}

// --------------------------------------------------------------------------------------------------------------------
// Construct
// --------------------------------------------------------------------------------------------------------------------

const FName SCkUIDebuggerWindow::WindowId = FName(TEXT("UIDebugger"));

auto
    SCkUIDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _SummaryText = SNew(STextBlock)
        .Font(style::Bold(10))
        .ColorAndOpacity(style::Text_Primary);

    _LayerListBox = SNew(SVerticalBox);
    _HistoryListBox = SNew(SVerticalBox);

    // ---- Search bar ----

    auto SearchBar =
        SNew(SBox)
        .HeightOverride(28.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(style::Bg_Dark)
            .Padding(FMargin(style::Pad_S))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, style::Pad_S, 0.0f)
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Search"))
                        .ColorAndOpacity(FSlateColor(style::Text_Muted))
                        .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(_SearchTextBox, SEditableTextBox)
                        .HintText(FText::FromString(TEXT("Filter layers...")))
                        .OnTextChanged_Lambda([this](const FText& InText)
                        {
                            _SearchFilter = InText.ToString();
                            DoUpdateAllSlots();
                        })
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(style::Pad_S, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .OnClicked_Lambda([this]()
                        {
                            _SearchTextBox->SetText(FText::GetEmpty());
                            _SearchFilter.Empty();
                            DoUpdateAllSlots();
                            return FReply::Handled();
                        })
                        .Visibility_Lambda([this]() -> EVisibility
                        {
                            return _SearchFilter.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
                        })
                        [
                            SNew(SImage)
                            .Image(FAppStyle::GetBrush("Icons.X"))
                            .ColorAndOpacity(FSlateColor(style::Text_Secondary))
                            .DesiredSizeOverride(FVector2D(12.0f, 12.0f))
                        ]
                    ]
            ]
        ];

    // ---- Toolbar ----

    auto MakeIconButton = [](const TCHAR* InBrush, const FText& InTooltip, FOnClicked InOnClicked) -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .OnClicked(InOnClicked)
            .ToolTipText(InTooltip)
            .ContentPadding(FMargin(style::Pad_S))
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush(InBrush))
                .ColorAndOpacity(FSlateColor(style::Text_Secondary))
                .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
            ];
    };

    auto Toolbar =
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(style::Bg_Dark)
        .Padding(FMargin(style::Pad_S))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, style::Pad_S, 0.0f)
                [
                    MakeIconButton(TEXT("Icons.Refresh"),
                        FText::FromString(TEXT("Force Refresh")),
                        FOnClicked::CreateLambda([this]() { _StructureDirty = true; return FReply::Handled(); }))
                ]

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 2.0f, 0.0f)
                [
                    MakeIconButton(TEXT("Icons.ChevronDown"),
                        FText::FromString(TEXT("Expand All")),
                        FOnClicked::CreateLambda([this]() { DoExpandAll(); return FReply::Handled(); }))
                ]

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, style::Pad_S, 0.0f)
                [
                    MakeIconButton(TEXT("Icons.ChevronUp"),
                        FText::FromString(TEXT("Collapse All")),
                        FOnClicked::CreateLambda([this]() { DoCollapseAll(); return FReply::Handled(); }))
                ]

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, style::Pad_S, 0.0f)
                [
                    MakeIconButton(TEXT("Icons.Delete"),
                        FText::FromString(TEXT("Clear History")),
                        FOnClicked::CreateLambda([this]() { _HistoryEvents.Empty(); DoBuildHistoryList(); return FReply::Handled(); }))
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(style::Pad_M, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_NameDepthCycler)
                        .Depth_Lambda([this]() -> int32 { return _NameDepth; })
                        .MaxDepth_Lambda([this]() -> int32 { return _MaxNameSegments; })
                        .OnDepthChanged(FOnCkDebug_NameDepthChanged::CreateLambda([this](int32 InNewDepth)
                        {
                            _NameDepth = InNewDepth;
                            _IsDirty = true;   // layer/widget slot texts re-stamp on the next update pass
                        }))
                ]

            + SHorizontalBox::Slot().FillWidth(1.0f)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox).MinDesiredWidth(200.0f) [ SearchBar ]
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(style::Pad_M, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebugger_RefreshControls)
                        .WindowId(SCkUIDebuggerWindow::WindowId)
                ]
        ];

    // ---- History area ----

    _HistoryArea =
        SNew(SExpandableArea)
        .InitiallyCollapsed(true)
        .BorderBackgroundColor(style::Bg_Dark)
        .HeaderPadding(FMargin(style::Pad_M, style::Pad_S))
        .HeaderContent()
        [
            SNew(STextBlock)
            .Font(style::Bold(10))
            .Text(FText::FromString(TEXT("Event History")))
            .ColorAndOpacity(style::Text_Highlight)
        ]
        .BodyContent()
        [
            SNew(SBox).MaxDesiredHeight(200.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot() [ _HistoryListBox.ToSharedRef() ]
            ]
        ];

    // ---- Root layout ----

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(style::Bg_Medium)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()
                [ Toolbar ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
                [ _SummaryText.ToSharedRef() ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_M, 0.0f)
                [ SNew(SSeparator).Thickness(1.0f) ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot().Padding(style::Pad_S)
                        [ _LayerListBox.ToSharedRef() ]
                ]

            + SVerticalBox::Slot().AutoHeight().Padding(style::Pad_S)
                [ _HistoryArea.ToSharedRef() ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------
// Tick
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    // ---- Resolve layout subsystem (PIE may start/stop) ----

    auto* LayoutSubsystem = ck_ui_debugger::FindLayoutSubsystem();
    auto* Layout = ck::IsValid(LayoutSubsystem)
        ? LayoutSubsystem->Get_Layout()
        : static_cast<UCk_UI_PrimaryGameLayout_UE*>(nullptr);

    if (Layout != _BoundLayout.Get())
    {
        DoUnbindLayoutEvents();

        if (ck::IsValid(Layout))
        {
            DoBindLayoutEvents(Layout);
        }

        _StructureDirty = true;
    }

    // ---- Rebuild structure if layout changed ----

    if (_StructureDirty)
    {
        _StructureDirty = false;
        _IsDirty = false;
        DoBuildLayerSlots();
        DoUpdateAllSlots();
        DoBuildHistoryList();
        return;
    }

    // ---- Update in-place if content changed ----

    if (_IsDirty)
    {
        _IsDirty = false;
        DoUpdateAllSlots();
        DoBuildHistoryList();
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Event Binding
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    DoBindLayoutEvents(
        UCk_UI_PrimaryGameLayout_UE* InLayout)
    -> void
{
    if (ck::Is_NOT_Valid(InLayout))
    { return; }

    _BoundLayout = InLayout;

    InLayout->OnWidgetPushed.AddRaw(this, &SCkUIDebuggerWindow::HandleWidgetPushed);
    InLayout->OnWidgetPopped.AddRaw(this, &SCkUIDebuggerWindow::HandleWidgetPopped);
    InLayout->OnLayerCleared.AddRaw(this, &SCkUIDebuggerWindow::HandleLayerCleared);
    InLayout->OnActiveLayerChanged.AddRaw(this, &SCkUIDebuggerWindow::HandleActiveLayerChanged);
    InLayout->OnInputModeChanged.AddRaw(this, &SCkUIDebuggerWindow::HandleInputModeChanged);
}

auto
    SCkUIDebuggerWindow::
    DoUnbindLayoutEvents()
    -> void
{
    if (NOT _BoundLayout.IsValid())
    { return; }

    auto* Layout = _BoundLayout.Get();
    Layout->OnWidgetPushed.RemoveAll(this);
    Layout->OnWidgetPopped.RemoveAll(this);
    Layout->OnLayerCleared.RemoveAll(this);
    Layout->OnActiveLayerChanged.RemoveAll(this);
    Layout->OnInputModeChanged.RemoveAll(this);

    _BoundLayout.Reset();
}

// --------------------------------------------------------------------------------------------------------------------
// Event Handlers
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    HandleWidgetPushed(
        FGameplayTag InLayerTag,
        UCommonActivatableWidget* InWidget)
    -> void
{
    const auto ClassName = ck::IsValid(InWidget, ck::IsValid_Policy_NullptrOnly{})
        ? DoShortName(InWidget->GetClass()->GetName())
        : FString(TEXT("Unknown"));

    _HistoryEvents.Insert(FCkUIDebugger_HistoryEvent{
        FPlatformTime::Seconds(),
        FString::Printf(TEXT("[Push] %s -> %s"), *ClassName, *DoShortName(InLayerTag.ToString()))
    }, 0);

    if (_HistoryEvents.Num() > MaxHistoryEvents) { _HistoryEvents.SetNum(MaxHistoryEvents); }

    _IsDirty = true;
}

auto
    SCkUIDebuggerWindow::
    HandleWidgetPopped(
        FGameplayTag InLayerTag,
        UCommonActivatableWidget* InWidget)
    -> void
{
    const auto ClassName = ck::IsValid(InWidget, ck::IsValid_Policy_NullptrOnly{})
        ? DoShortName(InWidget->GetClass()->GetName())
        : FString(TEXT("Unknown"));

    _HistoryEvents.Insert(FCkUIDebugger_HistoryEvent{
        FPlatformTime::Seconds(),
        FString::Printf(TEXT("[Pop] %s <- %s"), *ClassName, *DoShortName(InLayerTag.ToString()))
    }, 0);

    if (_HistoryEvents.Num() > MaxHistoryEvents) { _HistoryEvents.SetNum(MaxHistoryEvents); }

    _IsDirty = true;
}

auto
    SCkUIDebuggerWindow::
    HandleLayerCleared(
        FGameplayTag InLayerTag)
    -> void
{
    _HistoryEvents.Insert(FCkUIDebugger_HistoryEvent{
        FPlatformTime::Seconds(),
        FString::Printf(TEXT("[Cleared] %s"), *DoShortName(InLayerTag.ToString()))
    }, 0);

    if (_HistoryEvents.Num() > MaxHistoryEvents) { _HistoryEvents.SetNum(MaxHistoryEvents); }

    _IsDirty = true;
}

auto
    SCkUIDebuggerWindow::
    HandleActiveLayerChanged(
        FGameplayTag InNewActiveTag)
    -> void
{
    _HistoryEvents.Insert(FCkUIDebugger_HistoryEvent{
        FPlatformTime::Seconds(),
        FString::Printf(TEXT("Active Layer -> %s"), *DoShortName(InNewActiveTag.ToString()))
    }, 0);

    if (_HistoryEvents.Num() > MaxHistoryEvents) { _HistoryEvents.SetNum(MaxHistoryEvents); }

    _IsDirty = true;
}

auto
    SCkUIDebuggerWindow::
    HandleInputModeChanged(
        ECk_UI_InputMode InNewMode)
    -> void
{
    _HistoryEvents.Insert(FCkUIDebugger_HistoryEvent{
        FPlatformTime::Seconds(),
        FString::Printf(TEXT("Input Mode -> %s"), *ck_ui_debugger::InputModeToString(InNewMode))
    }, 0);

    if (_HistoryEvents.Num() > MaxHistoryEvents) { _HistoryEvents.SetNum(MaxHistoryEvents); }

    _IsDirty = true;
}

// --------------------------------------------------------------------------------------------------------------------
// Structure Building (one-time when layout binds)
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    DoBuildLayerSlots()
    -> void
{
    _LayerListBox->ClearChildren();
    _LayerSlots.Empty();
    _WidgetSlotPools.Empty();

    if (NOT _BoundLayout.IsValid())
    {
        _SummaryText->SetText(FText::FromString(TEXT("No active layout. Start PIE to see layer data.")));
        return;
    }

    auto* Layout = _BoundLayout.Get();

    // ---- Collect layers sorted by priority ----

    struct FEntry { UCk_UI_LayerStack_UE* Stack; };
    auto Entries = TArray<FEntry>{};

    Layout->ForEachLayer([&Entries](UCk_UI_LayerStack_UE* InStack, bool /*InIsTransitioning*/)
    {
        Entries.Add(FEntry{ InStack });
    });

    Entries.Sort([](const FEntry& A, const FEntry& B)
    {
        if (A.Stack->Get_Priority() != B.Stack->Get_Priority()) { return A.Stack->Get_Priority() > B.Stack->Get_Priority(); }
        return A.Stack->Get_LayerTag().ToString() < B.Stack->Get_LayerTag().ToString();
    });

    // ---- Create a slot for each layer ----

    _LayerSlots.SetNum(Entries.Num());
    _WidgetSlotPools.SetNum(Entries.Num());

    for (auto LayerIdx = 0; LayerIdx < Entries.Num(); ++LayerIdx)
    {
        auto& Slot = _LayerSlots[LayerIdx];
        Slot.Stack = Entries[LayerIdx].Stack;

        // ---- Header widgets ----

        Slot.StatusDot = SNew(SImage)
            .Image(FAppStyle::GetBrush("Icons.FilledCircle"))
            .DesiredSizeOverride(FVector2D(8.0f, 8.0f));

        Slot.TagText = SNew(STextBlock).Font(style::Bold());
        Slot.PriorityText = SNew(STextBlock).Font(style::Mono());
        Slot.InputModeText = SNew(STextBlock).Font(style::Normal());
        Slot.WidgetCountText = SNew(STextBlock).Font(style::Normal());

        auto Header =
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, style::Pad_S, 0.0f)
                [ Slot.StatusDot.ToSharedRef() ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, style::Pad_M, 0.0f)
                [ Slot.TagText.ToSharedRef() ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, style::Pad_M, 0.0f)
                [ Slot.PriorityText.ToSharedRef() ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, style::Pad_M, 0.0f)
                [ Slot.InputModeText.ToSharedRef() ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [ Slot.WidgetCountText.ToSharedRef() ];

        // ---- Widget list with pre-allocated rows ----

        Slot.WidgetListBox = SNew(SVerticalBox);
        auto& WidgetPool = _WidgetSlotPools[LayerIdx];
        WidgetPool.SetNum(MaxWidgetsPerLayer);

        for (auto WidgetIdx = 0; WidgetIdx < MaxWidgetsPerLayer; ++WidgetIdx)
        {
            auto& WSlot = WidgetPool[WidgetIdx];

            WSlot.StatusDot = SNew(SImage)
                .Image(FAppStyle::GetBrush("Icons.FilledCircle"))
                .DesiredSizeOverride(FVector2D(6.0f, 6.0f));

            WSlot.ClassNameText = SNew(STextBlock).Font(style::Normal())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis);

            WSlot.BadgeText = SNew(STextBlock).Font(style::Normal());

            const auto BgColor = (WidgetIdx % 2 == 0) ? style::Bg_Dark : style::Bg_Medium;

            WSlot.Root = SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(BgColor)
                .Padding(FMargin(style::Pad_L, style::Pad_S, style::Pad_M, style::Pad_S))
                .Visibility(EVisibility::Collapsed)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, style::Pad_S, 0.0f)
                        [ WSlot.StatusDot.ToSharedRef() ]

                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [ WSlot.ClassNameText.ToSharedRef() ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        .Padding(style::Pad_S, 0.0f, 0.0f, 0.0f)
                        [ WSlot.BadgeText.ToSharedRef() ]
                ];

            Slot.WidgetListBox->AddSlot().AutoHeight()
                [ WSlot.Root.ToSharedRef() ];
        }

        // ---- Expandable area ----

        Slot.ExpandableArea = SNew(SExpandableArea)
            .InitiallyCollapsed(false)
            .BorderBackgroundColor(style::Bg_Dark)
            .HeaderPadding(FMargin(style::Pad_M, style::Pad_S))
            .HeaderContent() [ Header ]
            .BodyContent() [ Slot.WidgetListBox.ToSharedRef() ];

        _LayerListBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
            [ Slot.ExpandableArea.ToSharedRef() ];
    }
}

// --------------------------------------------------------------------------------------------------------------------
// In-place Update (no widget destruction)
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    DoUpdateAllSlots()
    -> void
{
    if (NOT _BoundLayout.IsValid())
    {
        _SummaryText->SetText(FText::FromString(TEXT("No active layout. Start PIE to see layer data.")));
        return;
    }

    auto* Layout = _BoundLayout.Get();
    const auto ActiveTag = Layout->Get_ActiveLayerTag();
    const auto EffectiveMode = Layout->Get_EffectiveInputMode();

    _SummaryText->SetText(FText::FromString(FString::Printf(
        TEXT("Active: %s   |   Input: %s   |   Layers: %d"),
        *ActiveTag.ToString(),
        *ck_ui_debugger::InputModeToString(EffectiveMode),
        _LayerSlots.Num())));

    for (auto& Slot : _LayerSlots)
    {
        const auto IsActive = ck::IsValid(Slot.Stack) && Slot.Stack->Get_LayerTag() == ActiveTag;
        const auto MatchesFilter = ck::IsValid(Slot.Stack) && DoMatchesFilter(Slot.Stack->Get_LayerTag().ToString());

        Slot.ExpandableArea->SetVisibility(MatchesFilter ? EVisibility::Visible : EVisibility::Collapsed);

        if (MatchesFilter)
        {
            DoUpdateLayerSlot(Slot, IsActive);
        }
    }
}

auto
    SCkUIDebuggerWindow::
    DoUpdateLayerSlot(
        FCkUIDebugger_LayerSlot& InSlot,
        bool InIsActive)
    -> void
{
    auto* Stack = InSlot.Stack;

    if (ck::Is_NOT_Valid(Stack))
    { return; }

    const auto LayerTag = Stack->Get_LayerTag();
    const auto Priority = Stack->Get_Priority();
    const auto InputMode = Stack->Get_DefaultInputMode();
    const auto HasWidgetsFlag = Stack->HasWidgets();

    // ---- Determine dot color from current ForEachLayer data ----
    // We read IsTransitioning fresh from ForEachLayer for this specific stack.

    auto IsTransitioning = false;

    if (_BoundLayout.IsValid())
    {
        _BoundLayout->ForEachLayer([&](UCk_UI_LayerStack_UE* InStack, bool InIsTransitioning)
        {
            if (InStack == Stack) { IsTransitioning = InIsTransitioning; }
        });
    }

    auto DotColor = style::Text_Muted;
    if (IsTransitioning)      { DotColor = style::Accent_Warning; }
    else if (InIsActive)      { DotColor = style::Accent_Success; }
    else if (HasWidgetsFlag)  { DotColor = style::Text_Secondary; }

    InSlot.StatusDot->SetColorAndOpacity(FSlateColor(DotColor));

    InSlot.TagText->SetText(FText::FromString(DoShortName(LayerTag.ToString())));
    InSlot.TagText->SetToolTipText(FText::FromString(LayerTag.ToString()));
    InSlot.TagText->SetColorAndOpacity(InIsActive ? style::Text_Highlight : style::Text_Primary);

    InSlot.PriorityText->SetText(FText::FromString(FString::Printf(TEXT("[%d]"), Priority)));
    InSlot.PriorityText->SetColorAndOpacity(style::Accent_Cyan);

    InSlot.InputModeText->SetText(FText::FromString(ck_ui_debugger::InputModeToString(InputMode)));
    InSlot.InputModeText->SetColorAndOpacity(style::Text_Secondary);

    const auto& WidgetList = Stack->GetWidgetList();
    auto* ActiveWidget = Stack->GetActiveWidget();

    InSlot.WidgetCountText->SetText(FText::FromString(FString::Printf(TEXT("(%d)"), WidgetList.Num())));
    InSlot.WidgetCountText->SetColorAndOpacity(style::Text_Muted);

    // ---- Find this layer's index to access widget pool ----

    const auto LayerIdx = _LayerSlots.IndexOfByPredicate([&InSlot](const FCkUIDebugger_LayerSlot& S)
    {
        return &S == &InSlot;
    });

    if (LayerIdx == INDEX_NONE || LayerIdx >= _WidgetSlotPools.Num())
    { return; }

    auto& WidgetPool = _WidgetSlotPools[LayerIdx];

    for (auto WidgetIdx = 0; WidgetIdx < MaxWidgetsPerLayer; ++WidgetIdx)
    {
        auto& WSlot = WidgetPool[WidgetIdx];

        if (WidgetIdx < WidgetList.Num() && ck::IsValid(WidgetList[WidgetIdx]))
        {
            auto* Widget = WidgetList[WidgetIdx];
            const auto IsActiveWidget = Widget == ActiveWidget;

            WSlot.ClassNameText->SetText(FText::FromString(DoShortName(Widget->GetClass()->GetName())));
            WSlot.ClassNameText->SetToolTipText(FText::FromString(Widget->GetClass()->GetName()));
            WSlot.ClassNameText->SetColorAndOpacity(IsActiveWidget ? style::Text_Primary : style::Text_Secondary);

            WSlot.StatusDot->SetColorAndOpacity(FSlateColor(IsActiveWidget ? style::Accent_Success : style::Text_Muted));

            WSlot.BadgeText->SetText(FText::FromString(IsActiveWidget ? TEXT("Active") : TEXT("Inactive")));
            WSlot.BadgeText->SetColorAndOpacity(IsActiveWidget ? style::Accent_Success : style::Text_Muted);

            WSlot.Root->SetVisibility(EVisibility::Visible);
        }
        else
        {
            WSlot.Root->SetVisibility(EVisibility::Collapsed);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// History List
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    DoBuildHistoryList()
    -> void
{
    _HistoryListBox->ClearChildren();

    if (_HistoryEvents.IsEmpty())
    {
        _HistoryListBox->AddSlot().AutoHeight().Padding(style::Pad_M, style::Pad_S)
            [
                SNew(STextBlock).Font(style::Normal())
                .Text(FText::FromString(TEXT("No events yet.")))
                .ColorAndOpacity(style::Text_Muted)
            ];
        return;
    }

    for (auto Idx = 0; Idx < _HistoryEvents.Num(); ++Idx)
    {
        const auto BgColor = (Idx % 2 == 0) ? style::Bg_Dark : style::Bg_Medium;

        _HistoryListBox->AddSlot().AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(BgColor)
                .Padding(FMargin(style::Pad_M, 2.0f))
                [
                    SNew(STextBlock).Font(style::Normal())
                    .Text(FText::FromString(_HistoryEvents[Idx].Description))
                    .ColorAndOpacity(style::Text_Secondary)
                ]
            ];
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Toolbar Actions
// --------------------------------------------------------------------------------------------------------------------

auto SCkUIDebuggerWindow::DoExpandAll() -> void
{
    for (auto& Slot : _LayerSlots)
    {
        if (Slot.ExpandableArea.IsValid()) { Slot.ExpandableArea->SetExpanded(true); }
    }
}

auto SCkUIDebuggerWindow::DoCollapseAll() -> void
{
    for (auto& Slot : _LayerSlots)
    {
        if (Slot.ExpandableArea.IsValid()) { Slot.ExpandableArea->SetExpanded(false); }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Search
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    DoMatchesFilter(
        const FString& InLayerTag) const
    -> bool
{
    if (_SearchFilter.IsEmpty())
    { return true; }

    return ck::fuzzy::Match(_SearchFilter, InLayerTag, {}).Get_IsMatch();
}

auto
    SCkUIDebuggerWindow::
    DoShortName(
        const FString& InFullName)
    -> FString
{
    _MaxNameSegments = FMath::Max(_MaxNameSegments, SCkDebug_NameLabel::Get_SegmentCount(InFullName));
    return SCkDebug_NameLabel::Get_ShortName(InFullName, _NameDepth);
}

// --------------------------------------------------------------------------------------------------------------------
