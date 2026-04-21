#include "CkDebuggerWindow_Main.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Styling/AppStyle.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_ViewportPicker.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_InspectorFilter.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Base.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Overview.h"

#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_EntityList.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_Inspector.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

const FName SCkDebuggerWindow_Main::WindowId = FName(TEXT("EcsDebugger"));

auto SCkDebuggerWindow_Main::Construct(const FArguments& InArgs) -> void
{
    Register_WithGate();

    SelectionModel = MakeShared<FCkDebuggerModel_EntitySelection>();
    WorldModel = MakeShared<FCkDebuggerModel_WorldContext>();
    ViewportPicker = MakeShared<FCkDebuggerModel_ViewportPicker>();
    ViewportPicker->Construct(SelectionModel, WorldModel);
    FilterModel = MakeShared<FCkDebuggerModel_InspectorFilter>();

    // Refresh the toolbar badge strip when the filter selection changes.
    if (FilterModel.IsValid())
    {
        auto WeakSelf = TWeakPtr<SCkDebuggerWindow_Main>(SharedThis(this));
        FilterChangedHandle = FilterModel->OnFilterChanged.AddLambda(
        [WeakSelf]()
        {
            if (const auto Pinned = WeakSelf.Pin())
            {
                Pinned->Refresh_FilterBadgeStrip();
            }
        });
    }

    const auto AvailableWorlds = WorldModel->Get_AvailableWorlds();
    if (AvailableWorlds.Num() > 0)
    {
        WorldModel->Set_SelectedWorld(AvailableWorlds[0]);
    }

    Pages.Add(MakeShared<FCkDebuggerPage_Overview>());
    Pages[0]->Set_IsActive(true);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                Build_Toolbar()
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)
                .PhysicalSplitterHandleSize(3.0f)
                .HitDetectionSplitterHandleSize(5.0f)
                .Style(FAppStyle::Get(), "Splitter")

                + SSplitter::Slot()
                .Value(0.2f)
                .MinSize(200.0f)
                [
                    SNew(SBox)
                    .MaxDesiredWidth(500.0f)
                    [
                        Build_LeftSidebar()
                    ]
                ]

                + SSplitter::Slot()
                .Value(0.5f)
                .MinSize(400.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Border"))
                    .BorderBackgroundColor(CkDebugStyle::Border())
                    .Padding(FMargin(1.0f, 0.0f))
                    [
                        SAssignNew(ContentAreaContainer, SBox)
                        [
                            Build_ContentArea()
                        ]
                    ]
                ]

                + SSplitter::Slot()
                .Value(0.3f)
                .MinSize(250.0f)
                [
                    SNew(SBox)
                    .MaxDesiredWidth(600.0f)
                    [
                        Build_InspectorPanel()
                    ]
                ]
            ]
        ]
    ];
}

auto SCkDebuggerWindow_Main::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    // Page ticks drive graph rebuilds — gate them behind the user's refresh
    // settings. Viewport-picker ticks stay ungated so input handling keeps
    // working even when the panel is paused.
    if (FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    {
        for (const auto& Page : Pages)
        {
            if (Page.IsValid() && Page->IsActive())
            {
                Page->Tick(InDeltaTime);
            }
        }
    }

    if (ViewportPicker.IsValid() && ViewportPicker->IsActive())
    {
        ViewportPicker->Tick(InDeltaTime);
    }
}

auto SCkDebuggerWindow_Main::Build_Toolbar() -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Medium"))
        .Padding(FMargin(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

            // ---- Pick toggle button ----
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(SButton)
                .ButtonColorAndOpacity_Lambda([this]() -> FLinearColor
                {
                    return ViewportPicker.IsValid() && ViewportPicker->IsActive()
                        ? CkDebugStyle::Selection()
                        : CkDebugStyle::Bg2();
                })
                .ForegroundColor_Lambda([this]() -> FSlateColor
                {
                    return ViewportPicker.IsValid() && ViewportPicker->IsActive()
                        ? FSlateColor(CkDebugStyle::TextStrong())
                        : FSlateColor(CkDebugStyle::TextDim());
                })
                .IsEnabled_Lambda([this]() -> bool
                {
                    if (NOT ViewportPicker.IsValid())
                    { return false; }

                    if (ViewportPicker->IsActive())
                    { return true; }

                    return ViewportPicker->CanActivate();
                })
                .ToolTipText_Lambda([this]() -> FText
                {
                    if (NOT ViewportPicker.IsValid())
                    { return FText::GetEmpty(); }

                    if (ViewportPicker->IsActive())
                    {
                        return FText::FromString(TEXT("Exit pick mode (Esc)"));
                    }

                    if (NOT ViewportPicker->CanActivate())
                    {
                        return FText::FromString(TEXT(
                            "Pick mode unavailable — select a running PIE or Game world first.\n"
                            "(Simulate-in-Editor is not supported.)"));
                    }

                    return FText::FromString(TEXT(
                        "Enter pick mode: click an entity in the viewport to select it in the debugger."));
                })
                .OnClicked_Lambda([this]() -> FReply
                {
                    if (ViewportPicker.IsValid())
                    {
                        ViewportPicker->Toggle();
                    }
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() -> FText
                    {
                        return ViewportPicker.IsValid() && ViewportPicker->IsActive()
                            ? FText::FromString(TEXT("Picking..."))
                            : FText::FromString(TEXT("Pick"));
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                ]
            ]

            // ---- Gear button (picker settings popover) ----
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SAssignNew(PickerSettingsAnchor, SMenuAnchor)
                .Placement(MenuPlacement_BelowAnchor)
                .OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
                {
                    return Build_PickerSettingsPopover();
                })
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(CkDebugStyle::Bg2())
                    .ToolTipText(FText::FromString(TEXT("Picker settings")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (PickerSettingsAnchor.IsValid())
                        {
                            PickerSettingsAnchor->SetIsOpen(NOT PickerSettingsAnchor->IsOpen());
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\u2699"))) // gear glyph
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                        .ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
                    ]
                ]
            ]

            // ---- Visual separator ----
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(SSeparator)
                .Orientation(Orient_Vertical)
                .Thickness(1.0f)
            ]

            // ---- Filter button (with active count) ----
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SAssignNew(FilterAnchor, SMenuAnchor)
                .Placement(MenuPlacement_BelowAnchor)
                .OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
                {
                    return Build_FilterPopover();
                })
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity_Lambda([this]() -> FLinearColor
                    {
                        return FilterModel.IsValid() && FilterModel->Get_HasActiveFilter()
                            ? CkDebugStyle::Selection()
                            : CkDebugStyle::Bg2();
                    })
                    .ForegroundColor_Lambda([this]() -> FSlateColor
                    {
                        return FilterModel.IsValid() && FilterModel->Get_HasActiveFilter()
                            ? FSlateColor(CkDebugStyle::TextStrong())
                            : FSlateColor(CkDebugStyle::TextDim());
                    })
                    .ToolTipText(FText::FromString(TEXT(
                        "Filter entities by which inspectors apply to them.\n"
                        "Non-matching entities are dimmed but remain selectable.")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (FilterAnchor.IsValid())
                        {
                            FilterAnchor->SetIsOpen(NOT FilterAnchor->IsOpen());
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() -> FText
                        {
                            if (NOT FilterModel.IsValid() || NOT FilterModel->Get_HasActiveFilter())
                            { return FText::FromString(TEXT("Filter")); }

                            const auto Count = FilterModel->Get_NumSelected();
                            const auto CountStr = Count > 9
                                ? FString(TEXT("9+"))
                                : ck::Format_UE(TEXT("{}"), Count);
                            return FText::FromString(ck::Format_UE(TEXT("Filter ({})"), CountStr));
                        })
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    ]
                ]
            ]

            // ---- Active-filter badge strip ----
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(FilterBadgeStrip, SHorizontalBox)
            ]

            // ---- Refresh mode + rate cap (right-aligned) ----
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkDebuggerStyle::Padding_Medium, 0.0f, 0.0f, 0.0f))
            [
                SNew(SCkDebugger_RefreshControls)
                .WindowId(SCkDebuggerWindow_Main::WindowId)
            ]
        ];
}

auto SCkDebuggerWindow_Main::Build_PickerSettingsPopover() -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Medium"))
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SNew(SVerticalBox)

            // ---- Through Walls toggle ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([this]() -> ECheckBoxState
                {
                    return ViewportPicker.IsValid() && ViewportPicker->Get_DrawThroughWalls()
                        ? ECheckBoxState::Checked
                        : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                {
                    if (ViewportPicker.IsValid())
                    {
                        ViewportPicker->Set_DrawThroughWalls(InState == ECheckBoxState::Checked);
                    }
                })
                .ToolTipText(FText::FromString(TEXT(
                    "Draw pick markers and hover highlight on top of world geometry.\n"
                    "Enable this when entities are enclosed in meshes.")))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Through Walls")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
                ]
            ]

            // ---- Ignore Self toggle ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([this]() -> ECheckBoxState
                {
                    return ViewportPicker.IsValid() && ViewportPicker->Get_IgnoreLocalPawn()
                        ? ECheckBoxState::Checked
                        : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                {
                    if (ViewportPicker.IsValid())
                    {
                        ViewportPicker->Set_IgnoreLocalPawn(InState == ECheckBoxState::Checked);
                    }
                })
                .ToolTipText(FText::FromString(TEXT(
                    "Ignore entities that belong to the locally controlled pawn\n"
                    "(including attached actors and child ECS entities).\n"
                    "Useful to avoid picking your own first-person viewpoint entity.")))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Ignore Self")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
                ]
            ]

            // ---- Cull Radius spinbox ----
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Cull Radius:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(120.0f)
                    [
                        SNew(SSpinBox<float>)
                        .MinValue(100.0f)
                        .MaxValue(100000.0f)
                        .MinSliderValue(500.0f)
                        .MaxSliderValue(20000.0f)
                        .Delta(100.0f)
                        .Value_Lambda([this]() -> float
                        {
                            return ViewportPicker.IsValid() ? ViewportPicker->Get_CullRadius() : 0.0f;
                        })
                        .OnValueChanged_Lambda([this](float InValue)
                        {
                            if (ViewportPicker.IsValid())
                            {
                                ViewportPicker->Set_CullRadius(InValue);
                            }
                        })
                        .ToolTipText(FText::FromString(TEXT(
                            "Maximum distance (cm) from the camera at which entities are\n"
                            "drawn and considered for picking. Lower values reduce clutter\n"
                            "in large worlds.")))
                    ]
                ]
            ]
        ];
}

auto SCkDebuggerWindow_Main::Build_FilterPopover() -> TSharedRef<SWidget>
{
    if (NOT FilterModel.IsValid())
    { return SNullWidget::NullWidget; }

    auto InspectorList = SNew(SVerticalBox);

    for (const auto& Entry : FilterModel->Get_AllEntries())
    {
        const auto EntryID    = Entry.ID;
        const auto BadgeColor = Entry.BadgeColor;
        const auto DisplayName = Entry.DisplayName;

        InspectorList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 1.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(SBox)
                .WidthOverride(14.0f)
                .HeightOverride(14.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
                    .BorderBackgroundColor(BadgeColor)
                    .Padding(0.0f)
                    [
                        SNullWidget::NullWidget
                    ]
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([this, EntryID]() -> ECheckBoxState
                {
                    return FilterModel.IsValid() && FilterModel->Get_IsSelected(EntryID)
                        ? ECheckBoxState::Checked
                        : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([this, EntryID](ECheckBoxState InState)
                {
                    if (FilterModel.IsValid())
                    {
                        FilterModel->Set_Selection(EntryID, InState == ECheckBoxState::Checked);
                    }
                })
                [
                    SNew(STextBlock)
                    .Text(DisplayName)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
                ]
            ]
        ];
    }

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Medium"))
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SNew(SBox)
            .WidthOverride(280.0f)
            .MaxDesiredHeight(500.0f)
            [
                SNew(SVerticalBox)

                // ---- Header row: Match mode + Clear all ----
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Match:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        .ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
                    [
                        SNew(SCheckBox)
                        .Style(&FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("RadioButton"))
                        .IsChecked_Lambda([this]() -> ECheckBoxState
                        {
                            return FilterModel.IsValid() && FilterModel->Get_MatchMode() == ECk_InspectorFilter_MatchMode::All
                                ? ECheckBoxState::Checked
                                : ECheckBoxState::Unchecked;
                        })
                        .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                        {
                            if (FilterModel.IsValid() && InState == ECheckBoxState::Checked)
                            {
                                FilterModel->Set_MatchMode(ECk_InspectorFilter_MatchMode::All);
                            }
                        })
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("All")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SCheckBox)
                        .Style(&FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("RadioButton"))
                        .IsChecked_Lambda([this]() -> ECheckBoxState
                        {
                            return FilterModel.IsValid() && FilterModel->Get_MatchMode() == ECk_InspectorFilter_MatchMode::Any
                                ? ECheckBoxState::Checked
                                : ECheckBoxState::Unchecked;
                        })
                        .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                        {
                            if (FilterModel.IsValid() && InState == ECheckBoxState::Checked)
                            {
                                FilterModel->Set_MatchMode(ECk_InspectorFilter_MatchMode::Any);
                            }
                        })
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Any")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .HAlign(HAlign_Right)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(CkDebugStyle::Bg2())
                        .ToolTipText(FText::FromString(TEXT("Remove every active filter.")))
                        .OnClicked_Lambda([this]() -> FReply
                        {
                            if (FilterModel.IsValid())
                            {
                                FilterModel->Clear_Selection();
                            }
                            return FReply::Handled();
                        })
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Clear all")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
                        ]
                    ]
                ]

                // ---- Header row 2: Badge style ----
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Badges:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        .ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SSegmentedControl<ECk_InspectorFilter_BadgeStyle>)
                        .Value_Lambda([this]() -> ECk_InspectorFilter_BadgeStyle
                        {
                            return FilterModel.IsValid()
                                ? FilterModel->Get_BadgeStyle()
                                : ECk_InspectorFilter_BadgeStyle::ColorAndText;
                        })
                        .OnValueChanged_Lambda([this](ECk_InspectorFilter_BadgeStyle InNewValue)
                        {
                            if (FilterModel.IsValid())
                            {
                                FilterModel->Set_BadgeStyle(InNewValue);
                            }
                        })
                        + SSegmentedControl<ECk_InspectorFilter_BadgeStyle>::Slot(ECk_InspectorFilter_BadgeStyle::ColorOnly)
                            .Text(FText::FromString(TEXT("Color")))
                        + SSegmentedControl<ECk_InspectorFilter_BadgeStyle>::Slot(ECk_InspectorFilter_BadgeStyle::TextOnly)
                            .Text(FText::FromString(TEXT("Text")))
                        + SSegmentedControl<ECk_InspectorFilter_BadgeStyle>::Slot(ECk_InspectorFilter_BadgeStyle::ColorAndText)
                            .Text(FText::FromString(TEXT("Both")))
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
                [
                    SNew(SSeparator).Thickness(1.0f)
                ]

                // ---- Scrollable inspector list ----
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    .Orientation(Orient_Vertical)
                    + SScrollBox::Slot()
                    [
                        InspectorList
                    ]
                ]
            ]
        ];
}

auto SCkDebuggerWindow_Main::Refresh_FilterBadgeStrip() -> void
{
    if (NOT FilterBadgeStrip.IsValid())
    { return; }

    FilterBadgeStrip->ClearChildren();

    if (NOT FilterModel.IsValid())
    { return; }

    const auto& AllEntries = FilterModel->Get_AllEntries();
    const auto BadgeStyle  = FilterModel->Get_BadgeStyle();

    for (const auto& Entry : AllEntries)
    {
        if (NOT FilterModel->Get_IsSelected(Entry.ID))
        { continue; }

        const auto EntryID    = Entry.ID;
        const auto BadgeColor = Entry.BadgeColor;
        const auto Label      = Entry.DisplayName.ToString();

        TSharedPtr<SWidget> BadgeContent;
        switch (BadgeStyle)
        {
            case ECk_InspectorFilter_BadgeStyle::ColorOnly:
            {
                BadgeContent = SNew(SBox)
                    .WidthOverride(14.0f)
                    .HeightOverride(14.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
                        .BorderBackgroundColor(BadgeColor)
                        .Padding(0.0f)
                        [
                            SNullWidget::NullWidget
                        ]
                    ];
                break;
            }
            case ECk_InspectorFilter_BadgeStyle::TextOnly:
            {
                BadgeContent = SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FSlateColor(BadgeColor));
                break;
            }
            case ECk_InspectorFilter_BadgeStyle::ColorAndText:
            default:
            {
                BadgeContent = SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
                    .BorderBackgroundColor(BadgeColor)
                    .Padding(FMargin(6.0f, 1.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Label))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        .ColorAndOpacity(FSlateColor(CkDebugStyle::TextStrong()))
                    ];
                break;
            }
        }

        FilterBadgeStrip->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(0.0f)
                .ToolTipText(FText::Format(
                    FText::FromString(TEXT("{0}  (click to remove)")),
                    Entry.DisplayName))
                .OnClicked_Lambda([this, EntryID]() -> FReply
                {
                    if (FilterModel.IsValid())
                    {
                        FilterModel->Toggle_Selection(EntryID);
                    }
                    return FReply::Handled();
                })
                [
                    BadgeContent.ToSharedRef()
                ]
            ];
    }
}

auto SCkDebuggerWindow_Main::Build_LeftSidebar() -> TSharedRef<SWidget>
{
    return SAssignNew(EntityListPanel, SCkDebuggerPanel_EntityList, SelectionModel, WorldModel, FilterModel);
}

auto SCkDebuggerWindow_Main::Build_ContentArea() -> TSharedRef<SWidget>
{
    // Tab row
    auto TabRow = SNew(SHorizontalBox);

    for (auto i = 0; i < Pages.Num(); ++i)
    {
        if (NOT Pages[i].IsValid())
        { continue; }

        auto PageIndex = i;
        auto IsActive = (i == ActivePageIndex);

        TabRow->AddSlot()
            .AutoWidth()
            .Padding(1.0f, 0.0f)
            [
                SNew(SButton)
                    .ButtonColorAndOpacity(IsActive
                        ? CkDebugStyle::Selection()
                        : CkDebugStyle::Bg2())
                    .ForegroundColor(IsActive
                        ? CkDebugStyle::TextStrong()
                        : CkDebugStyle::TextDim())
                    .OnClicked_Lambda([this, PageIndex]()
                    {
                        OnPageSelected(PageIndex);
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                            .Text(Pages[i]->Get_PageName())
                            .Font(FCoreStyle::GetDefaultFontStyle(IsActive ? "Bold" : "Regular", 9))
                    ]
            ];
    }

    // Page content
    TSharedRef<SWidget> PageContent = SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Header"))
            .Text(FText::FromString(TEXT("No Page Selected")))
            .ColorAndOpacity(CkDebugStyle::TextMute())
        ];

    if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
    {
        auto Context = FCkDebuggerPageContext
        {
            SelectionModel,
            WorldModel,
            FilterModel
        };

        PageContent = Pages[ActivePageIndex]->Build_Content(Context);
    }

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    TabRow
                ]
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    PageContent
                ]
        ];
}

auto SCkDebuggerWindow_Main::Build_InspectorPanel() -> TSharedRef<SWidget>
{
    return SAssignNew(InspectorPanel, SCkDebuggerPanel_Inspector, SelectionModel);
}

auto SCkDebuggerWindow_Main::OnPageSelected(int32 InPageIndex) -> void
{
    if (NOT Pages.IsValidIndex(InPageIndex))
    { return; }

    if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
    {
        Pages[ActivePageIndex]->Set_IsActive(false);
    }

    ActivePageIndex = InPageIndex;

    if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
    {
        Pages[ActivePageIndex]->Set_IsActive(true);
    }

    RebuildContentArea();
}

auto SCkDebuggerWindow_Main::RebuildContentArea() -> void
{
    if (ContentAreaContainer.IsValid())
    {
        ContentAreaContainer->SetContent(Build_ContentArea());
    }
}

auto SCkDebuggerWindow_Main::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent) -> FReply
{
    if (SelectionModel.IsValid())
    {
        if (MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
        {
            if (SelectionModel->NavigateBack())
            {
                return FReply::Handled();
            }
        }

        if (MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
        {
            if (SelectionModel->NavigateForward())
            {
                return FReply::Handled();
            }
        }
    }

    return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

auto SCkDebuggerWindow_Main::Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>
{
    return SelectionModel;
}

auto SCkDebuggerWindow_Main::Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>
{
    return WorldModel;
}

auto SCkDebuggerWindow_Main::Get_ViewportPicker() const -> TSharedPtr<FCkDebuggerModel_ViewportPicker>
{
    return ViewportPicker;
}

auto SCkDebuggerWindow_Main::Get_FilterModel() const -> TSharedPtr<FCkDebuggerModel_InspectorFilter>
{
    return FilterModel;
}