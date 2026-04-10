#include "CkDebuggerWindow_Main.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Styling/AppStyle.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_ViewportPicker.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Base.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Overview.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_EntityList.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_Inspector.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

auto SCkDebuggerWindow_Main::Construct(const FArguments& InArgs) -> void
{
    SelectionModel = MakeShared<FCkDebuggerModel_EntitySelection>();
    WorldModel = MakeShared<FCkDebuggerModel_WorldContext>();
    ViewportPicker = MakeShared<FCkDebuggerModel_ViewportPicker>();
    ViewportPicker->Construct(SelectionModel, WorldModel);

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
                    .BorderBackgroundColor(FCkDebuggerStyle::Color_Border)
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

    for (const auto& Page : Pages)
    {
        if (Page.IsValid() && Page->IsActive())
        {
            Page->Tick(InDeltaTime);
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

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(SButton)
                .ButtonColorAndOpacity_Lambda([this]() -> FLinearColor
                {
                    return ViewportPicker.IsValid() && ViewportPicker->IsActive()
                        ? FCkDebuggerStyle::Color_Selection
                        : FCkDebuggerStyle::Color_Background_Light;
                })
                .ForegroundColor_Lambda([this]() -> FSlateColor
                {
                    return ViewportPicker.IsValid() && ViewportPicker->IsActive()
                        ? FSlateColor(FCkDebuggerStyle::Color_Text_Highlight)
                        : FSlateColor(FCkDebuggerStyle::Color_Text_Secondary);
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

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(SSeparator)
                .Orientation(Orient_Vertical)
                .Thickness(1.0f)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
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
                    .ColorAndOpacity(FSlateColor(FCkDebuggerStyle::Color_Text_Secondary))
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
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
                    .ColorAndOpacity(FSlateColor(FCkDebuggerStyle::Color_Text_Secondary))
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Cull Radius:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(FSlateColor(FCkDebuggerStyle::Color_Text_Secondary))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(100.0f)
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
        ];
}

auto SCkDebuggerWindow_Main::Build_LeftSidebar() -> TSharedRef<SWidget>
{
    return SAssignNew(EntityListPanel, SCkDebuggerPanel_EntityList, SelectionModel, WorldModel);
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
                        ? FCkDebuggerStyle::Color_Selection
                        : FCkDebuggerStyle::Color_Background_Light)
                    .ForegroundColor(IsActive
                        ? FCkDebuggerStyle::Color_Text_Highlight
                        : FCkDebuggerStyle::Color_Text_Secondary)
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
            .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Muted)
        ];

    if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
    {
        auto Context = FCkDebuggerPageContext
        {
            SelectionModel,
            WorldModel
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