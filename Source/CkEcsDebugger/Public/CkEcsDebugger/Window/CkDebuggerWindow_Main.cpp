#include "CkDebuggerWindow_Main.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Base.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Overview.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_EntityList.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_Inspector.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

auto SCkDebuggerWindow_Main::Construct(const FArguments& InArgs) -> void
{
    SelectionModel = MakeShared<FCkDebuggerModel_EntitySelection>();
    WorldModel = MakeShared<FCkDebuggerModel_WorldContext>();

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