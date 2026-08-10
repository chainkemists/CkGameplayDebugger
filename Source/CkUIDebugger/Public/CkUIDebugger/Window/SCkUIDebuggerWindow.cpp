#include "CkUIDebugger/Window/SCkUIDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameDepthCycler.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkUI/Layout/CkUI_Layout_Subsystem.h"
#include "CkUI/Layout/CkUI_PrimaryGameLayout.h"
#include "CkUI/Layout/CkUI_LayerStack.h"

#include "CommonActivatableWidget.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Local style + helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_ui_debugger
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

    static auto Get_WidgetRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(
            FMargin{CkStyle::SpaceXL, CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceS});
    }

    static auto Get_HistoryRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, 2.0f});
    }

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
        .Font_Static(&ck_ui_debugger::Font_Heading)
        .ColorAndOpacity(CkStyle::Text());

    _LayerListBox = SNew(SVerticalBox);
    _HistoryListBox = SNew(SVerticalBox);

    // ---- Search bar ----

    auto SearchBar =
        SNew(SBox)
        .HeightOverride(28.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(CkStyle::BgRoot())
            .Padding(FMargin(CkStyle::SpaceS))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [
                        SNew(SCkDebug_Icon)
                        .Brush(FAppStyle::GetBrush("Icons.Search"))
                        .Meaning(FText::FromString(TEXT("Filter the layer list by tag")))
                        .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                        .Size(FVector2D{16.0f, 16.0f})
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
                    .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
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
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            .DesiredSizeOverride_Lambda([]() -> TOptional<FVector2D>
                            {
                                const auto Size = ck::debug_axes::Apply_IconSize(12.0f);
                                return FVector2D{Size, Size};
                            })
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
            .ContentPadding(FMargin(CkStyle::SpaceS))
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush(InBrush))
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                .DesiredSizeOverride_Lambda([]() -> TOptional<FVector2D>
                {
                    const auto Size = ck::debug_axes::Apply_IconSize(16.0f);
                    return FVector2D{Size, Size};
                })
            ];
    };

    auto Toolbar =
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::BgRoot())
        .Padding(FMargin(CkStyle::SpaceS))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
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

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    MakeIconButton(TEXT("Icons.ChevronUp"),
                        FText::FromString(TEXT("Collapse All")),
                        FOnClicked::CreateLambda([this]() { DoCollapseAll(); return FReply::Handled(); }))
                ]

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    MakeIconButton(TEXT("Icons.Delete"),
                        FText::FromString(TEXT("Clear History")),
                        FOnClicked::CreateLambda([this]() { _HistoryEvents.Empty(); DoBuildHistoryList(); return FReply::Handled(); }))
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
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
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebugger_RefreshControls)
                        .WindowId(SCkUIDebuggerWindow::WindowId)
                ]
        ];

    // ---- History area ----

    _HistoryArea =
        SNew(SExpandableArea)
        .InitiallyCollapsed(true)
        .BorderBackgroundColor(CkStyle::BgRoot())
        .HeaderPadding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
        .HeaderContent()
        [
            SNew(STextBlock)
            .Font_Static(&ck_ui_debugger::Font_Heading)
            .Text(FText::FromString(TEXT("Event History")))
            .ColorAndOpacity(CkStyle::TextStrong())
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
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkUIDebugger"))
        .DisplayName(Get_WindowDisplayName())
        .MenuActionsContent()
        [
            SNew(SCkDebug_IconToolbar)
            .Actions({
                FCkDebug_IconToggleAction{
                    TEXT("UiActiveLayerOnly"),
                    TEXT("Target"),
                    FText::FromString(TEXT("Active Layer Only")),
                    FText::FromString(TEXT("Show only the layout's active layer.")),
                    TAttribute<bool>::CreateLambda([this]() { return _ShowActiveLayerOnly; }),
                    FOnCkDebug_IconToggleChanged::CreateLambda([this](const bool InIsEnabled)
                    {
                        _ShowActiveLayerOnly = InIsEnabled;
                        _IsDirty = true;
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
                [ Toolbar ]

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                [ _SummaryText.ToSharedRef() ]

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
                [ ck::debug_axes::Make_AxisSeparator() ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot().Padding(CkStyle::SpaceS)
                        [ _LayerListBox.ToSharedRef() ]
                ]

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS)
                [ _HistoryArea.ToSharedRef() ]
        ]
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
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

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

auto
    SCkUIDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // Structural axes reach this window only through the imperatively-built layer slots and history
    // rows; flagging the existing rebuild path re-stamps them on the next gated tick without
    // touching the widget tree from inside the revision poll.
    _StructureDirty = true;
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

        // A category swatch rather than a pill: the layer's state is encoded by colour alone here,
        // with the adjacent label carrying the layer TAG, not the state.
        Slot.StatusDot = SNew(SCkDebug_CategoryDot).Diameter(8.0f);

        Slot.TagText = SNew(STextBlock).Font_Static(&ck_ui_debugger::Font_RowLabel);
        Slot.PriorityText = SNew(STextBlock).Font_Static(&ck_ui_debugger::Font_Value);
        Slot.InputModeText = SNew(STextBlock).Font_Static(&ck_ui_debugger::Font_Body);
        Slot.WidgetCountText = SNew(STextBlock).Font_Static(&ck_ui_debugger::Font_Body);

        auto Header =
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ Slot.StatusDot.ToSharedRef() ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [ Slot.TagText.ToSharedRef() ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [ Slot.PriorityText.ToSharedRef() ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
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

            WSlot.StatusDot = SNew(SCkDebug_CategoryDot).Diameter(6.0f);

            WSlot.ClassNameText = SNew(STextBlock).Font_Static(&ck_ui_debugger::Font_Body)
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis);

            // The badge IS the row's state label, so it reads as a toned pill. Its text and tone
            // resolve from a shared cell because the pool's slot structs are relocatable.
            WSlot.IsWidgetActive = MakeShared<bool>(false);

            WSlot.Badge = SNew(SCkDebug_StatusPill)
                .ShowDot(false)
                .Text_Lambda([Cell = WSlot.IsWidgetActive]()
                {
                    return FText::FromString(*Cell ? TEXT("Active") : TEXT("Inactive"));
                })
                .Tone_Lambda([Cell = WSlot.IsWidgetActive]()
                {
                    return *Cell ? ECk_Tone::Ok : ECk_Tone::Neutral;
                });

            const auto BgColor = (WidgetIdx % 2 == 0) ? CkStyle::BgRoot() : CkStyle::Bg1();

            WSlot.Root = SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(BgColor)
                .Padding_Static(&ck_ui_debugger::Get_WidgetRowPadding)
                .Visibility(EVisibility::Collapsed)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [ WSlot.StatusDot.ToSharedRef() ]

                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [ WSlot.ClassNameText.ToSharedRef() ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                        [ WSlot.Badge.ToSharedRef() ]
                ];

            Slot.WidgetListBox->AddSlot().AutoHeight()
                [ WSlot.Root.ToSharedRef() ];
        }

        // ---- Expandable area ----

        Slot.ExpandableArea = SNew(SExpandableArea)
            .InitiallyCollapsed(false)
            .BorderBackgroundColor(CkStyle::BgRoot())
            .HeaderPadding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
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
        const auto IsVisible = MatchesFilter && (NOT _ShowActiveLayerOnly || IsActive);

        Slot.ExpandableArea->SetVisibility(IsVisible ? EVisibility::Visible : EVisibility::Collapsed);

        if (IsVisible)
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

    auto DotColor = CkStyle::TextMute();
    if (IsTransitioning)      { DotColor = CkStyle::Warn(); }
    else if (InIsActive)      { DotColor = CkStyle::Ok(); }
    else if (HasWidgetsFlag)  { DotColor = CkStyle::TextDim(); }

    InSlot.StatusDot->SetColorAndOpacity(DotColor);

    InSlot.TagText->SetText(FText::FromString(DoShortName(LayerTag.ToString())));
    InSlot.TagText->SetToolTipText(FText::FromString(LayerTag.ToString()));
    InSlot.TagText->SetColorAndOpacity(InIsActive ? CkStyle::TextStrong() : CkStyle::Text());

    InSlot.PriorityText->SetText(FText::FromString(FString::Printf(TEXT("[%d]"), Priority)));
    InSlot.PriorityText->SetColorAndOpacity(CkStyle::Accent());

    InSlot.InputModeText->SetText(FText::FromString(ck_ui_debugger::InputModeToString(InputMode)));
    InSlot.InputModeText->SetColorAndOpacity(CkStyle::TextDim());

    const auto& WidgetList = Stack->GetWidgetList();
    auto* ActiveWidget = Stack->GetActiveWidget();

    InSlot.WidgetCountText->SetText(FText::FromString(FString::Printf(TEXT("(%d)"), WidgetList.Num())));
    InSlot.WidgetCountText->SetColorAndOpacity(CkStyle::TextMute());

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
            WSlot.ClassNameText->SetColorAndOpacity(IsActiveWidget ? CkStyle::Text() : CkStyle::TextDim());

            WSlot.StatusDot->SetColorAndOpacity(IsActiveWidget ? CkStyle::Ok() : CkStyle::TextMute());

            // Writing the shared cell IS the badge update — the pill reads it through its
            // attributes on the next paint, so nothing is invalidated or rebuilt.
            if (WSlot.IsWidgetActive.IsValid())
            { *WSlot.IsWidgetActive = IsActiveWidget; }

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
        _HistoryListBox->AddSlot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(STextBlock).Font_Static(&ck_ui_debugger::Font_Body)
                .Text(FText::FromString(TEXT("No events yet.")))
                .ColorAndOpacity(CkStyle::TextMute())
            ];
        return;
    }

    for (auto Idx = 0; Idx < _HistoryEvents.Num(); ++Idx)
    {
        const auto BgColor = (Idx % 2 == 0) ? CkStyle::BgRoot() : CkStyle::Bg1();

        _HistoryListBox->AddSlot().AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(BgColor)
                .Padding_Static(&ck_ui_debugger::Get_HistoryRowPadding)
                [
                    SNew(STextBlock).Font_Static(&ck_ui_debugger::Font_Body)
                    .Text(FText::FromString(_HistoryEvents[Idx].Description))
                    .ColorAndOpacity(CkStyle::TextDim())
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
