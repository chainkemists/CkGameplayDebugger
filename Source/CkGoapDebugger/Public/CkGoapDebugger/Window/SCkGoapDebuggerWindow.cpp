#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_Breadcrumb.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_PrimaryPane.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_Sidebar.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// ====================================================================================================================

const FName SCkGoapDebuggerWindow::WindowId = FName(TEXT("GoapDebugger"));

namespace
{
    // Stable display string for an entity in the picker.
    auto MakePickerLabel(const FCkGoapDebugger_EntitySnapshot& InSnap) -> FString
    {
        if (InSnap.DebugName.IsEmpty())
        { return FString::Printf(TEXT("(entity %u)"), ::GetTypeHash(InSnap.EntityHandle)); }
        return InSnap.DebugName;
    }

    // Build a small color-swatch + label for the legend row.
    auto MakeLegendItem(const FLinearColor& InColor, const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SBox)
                        .WidthOverride(10.0f)
                        .HeightOverride(10.0f)
                        [
                            SNew(SBorder)
                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                .BorderBackgroundColor(InColor)
                                .Padding(FMargin(0.0f))
                                [ SNew(SSpacer) ]
                        ]
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InText))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                ];
    }
}

// ====================================================================================================================
// LIFETIME
// ====================================================================================================================

SCkGoapDebuggerWindow::~SCkGoapDebuggerWindow()
{
    if (_OnEndPieHandle.IsValid())
    { FEditorDelegates::EndPIE.Remove(_OnEndPieHandle); }

    if (_OnBeginPieHandle.IsValid())
    { FEditorDelegates::BeginPIE.Remove(_OnBeginPieHandle); }
}

auto
    SCkGoapDebuggerWindow::
    HandleWorldTornDown()
    -> void
{
    _CachedWorld.Reset();

    _EntityPickerItems.Empty();
    _EntityPickerHandles.Empty();

    if (_Sidebar.IsValid())
    { _Sidebar->Reset_ForWorldChange(); }

    if (_ViewModel.IsValid())
    { _ViewModel->Reset_ForWorldChange(); }

    if (_EntityPicker.IsValid())
    { _EntityPicker->RefreshOptions(); }
}

// ====================================================================================================================
// CONSTRUCT
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _ViewModel = MakeShared<FCkGoapDebugger_ViewModel>();

    _OnBeginPieHandle = FEditorDelegates::BeginPIE.AddLambda([this](bool)
    { HandleWorldTornDown(); });

    _OnEndPieHandle = FEditorDelegates::EndPIE.AddLambda([this](bool)
    { HandleWorldTornDown(); });

    auto SidebarWidget   = SAssignNew(_Sidebar, SCkGoapDebugger_Sidebar, _ViewModel);
    auto CenterColumn    = BuildCenterColumn();
    auto WsRailStub      = BuildWsRailStub();

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    // Mode bar (top, fixed height)
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildModeBar()
                        ]

                    // Toolbar
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildToolbar()
                        ]

                    // Legend
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildLegend()
                        ]

                    // App body: sidebar | center | ws rail
                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SSplitter)
                                .Orientation(Orient_Horizontal)

                                + SSplitter::Slot()
                                    .Value(0.22f)
                                    .MinSize(220.0f)
                                    [
                                        SidebarWidget
                                    ]

                                + SSplitter::Slot()
                                    .Value(0.55f)
                                    [
                                        CenterColumn
                                    ]

                                + SSplitter::Slot()
                                    .Value(0.23f)
                                    .MinSize(220.0f)
                                    [
                                        WsRailStub
                                    ]
                        ]
            ]
    ];

    // Subscribe to ViewModel changes so the sidebar's structural-hash check
    // gets a chance to fire on selection / snapshot changes.
    if (_ViewModel.IsValid())
    {
        _ViewModel->OnChanged.AddLambda([this]()
        {
            if (_Sidebar.IsValid())
            { _Sidebar->RefreshFromViewModel(); }
            if (_Breadcrumb.IsValid())
            { _Breadcrumb->RefreshFromViewModel(); }
            if (_PrimaryPane.IsValid())
            { _PrimaryPane->RefreshFromViewModel(); }
        });
    }
}

// ====================================================================================================================
// TICK
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    // Pick the most appropriate world: prefer PIE if available, else editor.
    {
        auto FoundWorld = static_cast<UWorld*>(nullptr);

        if (GEngine)
        {
            for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
            {
                if (It->WorldType == EWorldType::PIE && ck::IsValid(It->World()) && It->World()->HasBegunPlay())
                {
                    FoundWorld = It->World();
                    break;
                }
            }

            if (FoundWorld == nullptr && GEditor)
            {
                auto& EditorCtx = GEditor->GetEditorWorldContext();
                if (ck::IsValid(EditorCtx.World()))
                { FoundWorld = EditorCtx.World(); }
            }
        }

        _CachedWorld = FoundWorld;
    }

    auto* World = _CachedWorld.Get();
    if (ck::Is_NOT_Valid(World))
    { return; }

    _ViewModel->Tick(World);

    RefreshEntityPickerItems();
}

// ====================================================================================================================
// BUILD — MODE BAR
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildModeBar()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Black")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                // "Standalone Window" — active in D2
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Standalone Window")))
                            .IsEnabled(true)
                            .ToolTipText(FText::FromString(TEXT("Top-level debugger window (active)")))
                    ]

                // "ECS Inspector" — disabled stub
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("ECS Inspector")))
                            .IsEnabled(false)
                            .ToolTipText(FText::FromString(TEXT("Embeds the debugger inside the ECS Inspector (later phase)")))
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                // "Simulate PlanFailed" — visual stub for D2
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Simulate PlanFailed")))
                            .IsEnabled(false)
                            .ToolTipText(FText::FromString(TEXT("Force a plan-failed event for the selected entity (later phase)")))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — TOOLBAR
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    auto Picker = SAssignNew(_EntityPicker, SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&_EntityPickerItems)
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
        {
            return SNew(STextBlock)
                .Text(FText::FromString(InItem.IsValid() ? *InItem : FString{}))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
        })
        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> InItem, ESelectInfo::Type)
        {
            if (NOT InItem.IsValid() || NOT _ViewModel.IsValid()) { return; }
            const auto Idx = _EntityPickerItems.IndexOfByKey(InItem);
            if (Idx != INDEX_NONE && _EntityPickerHandles.IsValidIndex(Idx))
            { _ViewModel->SetSelectedEntity(_EntityPickerHandles[Idx]); }
        })
        [
            SAssignNew(_EntityPickerLabel, STextBlock)
                .Text(FText::FromString(TEXT("(no entities)")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
        ];

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Surface")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                // "Entity:" label
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Entity:")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                    ]

                // Entity picker
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        Picker
                    ]

                // EntityRef pill (ID|Version) — click navigates to ECS debugger
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(SCkDebug_EntityRef)
                            .Entity_Lambda([this]() -> FCk_Handle
                            {
                                if (NOT _ViewModel.IsValid()) { return FCk_Handle{}; }
                                return _ViewModel->GetSelectedEntity();
                            })
                    ]

                // Live / Scrub toggle (two buttons)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Live")))
                            .ToolTipText(FText::FromString(TEXT("Live mode — snapshots refresh every tick")))
                            .ButtonColorAndOpacity_Lambda([this]() -> FSlateColor
                            {
                                if (NOT _ViewModel.IsValid()) { return FSlateColor(FLinearColor::White); }
                                const auto Active = _ViewModel->GetMode() == FCkGoapDebugger_ViewModel::EMode::Live;
                                return Active
                                    ? FSlateColor(FCkGoapDebuggerStyle::Color_Status_Planning)
                                    : FSlateColor(FLinearColor::White);
                            })
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_ViewModel.IsValid())
                                { _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Live); }
                                return FReply::Handled();
                            })
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Scrub")))
                            .ToolTipText(FText::FromString(TEXT("Scrub mode — freeze on selected history event (full UI in D6)")))
                            .ButtonColorAndOpacity_Lambda([this]() -> FSlateColor
                            {
                                if (NOT _ViewModel.IsValid()) { return FSlateColor(FLinearColor::White); }
                                const auto Active = _ViewModel->GetMode() == FCkGoapDebugger_ViewModel::EMode::Scrub;
                                return Active
                                    ? FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected)
                                    : FSlateColor(FLinearColor::White);
                            })
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_ViewModel.IsValid())
                                { _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Scrub); }
                                return FReply::Handled();
                            })
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                // Force replan
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Force replan")))
                            .ToolTipText(FText::FromString(TEXT("Issue Request_Plan on the currently selected Action")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_ViewModel.IsValid())
                                { _ViewModel->ForceReplanOnSelected(); }
                                return FReply::Handled();
                            })
                    ]
        ];
}

// ====================================================================================================================
// BUILD — LEGEND
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildLegend()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Action color:")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        MakeLegendItem(FCkGoapDebuggerStyle::Color_Status_PlanningBdr, TEXT("Leaf — atomic action"))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        MakeLegendItem(FCkGoapDebuggerStyle::Color_Status_Composite, TEXT("Composite — has children"))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        MakeLegendItem(FCkGoapDebuggerStyle::Color_Status_Failed, TEXT("Failure-blocked"))
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Hover any action to see full path")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — CENTER COLUMN (breadcrumb + primary + graph stub) / WS RAIL STUB
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildCenterColumn()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
        .Padding(FMargin(0.0f))
        [
            SNew(SSplitter)
                .Orientation(Orient_Vertical)

                // Breadcrumb (auto height — wraps to its content)
                + SSplitter::Slot()
                    .Value(0.08f)
                    .SizeRule(SSplitter::SizeToContent)
                    [
                        SAssignNew(_Breadcrumb, SCkGoapDebugger_Breadcrumb)
                            .ViewModel(_ViewModel)
                    ]

                // Primary pane (top of remaining space)
                + SSplitter::Slot()
                    .Value(0.42f)
                    [
                        SAssignNew(_PrimaryPane, SCkGoapDebugger_PrimaryPane)
                            .ViewModel(_ViewModel)
                    ]

                // Graph pane (bottom — stub for D5)
                + SSplitter::Slot()
                    .Value(0.50f)
                    [
                        BuildGraphStub()
                    ]
        ];
}

auto
    SCkGoapDebuggerWindow::
    BuildGraphStub()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Surface")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large))
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("D5 — graph goes here")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
        ];
}

auto
    SCkGoapDebuggerWindow::
    BuildWsRailStub()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Surface")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large))
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("D4 — WS rail goes here")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
        ];
}

// ====================================================================================================================
// ENTITY PICKER
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    RefreshEntityPickerItems()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const auto& Snapshots = _ViewModel->GetAllEntitySnapshots();

    // Detect set changes by (count, joined hash of handles).
    auto IdentityChanged = Snapshots.Num() != _EntityPickerHandles.Num();
    if (NOT IdentityChanged)
    {
        for (auto i = 0; i < Snapshots.Num(); ++i)
        {
            if (NOT (Snapshots[i].EntityHandle == _EntityPickerHandles[i]))
            { IdentityChanged = true; break; }
        }
    }

    if (IdentityChanged)
    {
        _EntityPickerItems.Empty(Snapshots.Num());
        _EntityPickerHandles.Empty(Snapshots.Num());

        for (const auto& Snap : Snapshots)
        {
            _EntityPickerItems.Add(MakeShared<FString>(MakePickerLabel(Snap)));
            _EntityPickerHandles.Add(Snap.EntityHandle);
        }

        if (_EntityPicker.IsValid())
        { _EntityPicker->RefreshOptions(); }
    }

    // Update the picker label to match the current selected entity.
    if (_EntityPickerLabel.IsValid())
    {
        if (Snapshots.Num() == 0)
        {
            _EntityPickerLabel->SetText(FText::FromString(TEXT("(no entities)")));
        }
        else
        {
            const auto Selected = _ViewModel->GetSelectedEntity();
            const auto SelIdx = _EntityPickerHandles.IndexOfByKey(Selected);
            if (SelIdx != INDEX_NONE && _EntityPickerItems.IsValidIndex(SelIdx))
            {
                _EntityPickerLabel->SetText(FText::FromString(*_EntityPickerItems[SelIdx]));
            }
            else
            {
                _EntityPickerLabel->SetText(FText::FromString(TEXT("(select entity)")));
            }
        }
    }
}

// ====================================================================================================================
