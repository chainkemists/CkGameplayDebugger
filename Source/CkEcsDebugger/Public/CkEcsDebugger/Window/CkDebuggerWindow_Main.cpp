#include "CkDebuggerWindow_Main.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Base.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Overview.h"

auto SCkDebuggerWindow_Main::Construct(const FArguments& InArgs) -> void
{
    SelectionModel = MakeShared<FCkDebuggerModel_EntitySelection>();
    WorldModel = MakeShared<FCkDebuggerModel_WorldContext>();

    Pages.Add(MakeShared<FCkDebuggerPage_Overview>());
    Pages[0]->Set_IsActive(true);

    LeftSidebarWidget = Build_LeftSidebar();
    ContentAreaWidget = Build_ContentArea();
    InspectorWidget = Build_InspectorPanel();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(0.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)
            .PhysicalSplitterHandleSize(2.0f)

            + SSplitter::Slot()
            .Value(0.2f)
            .MinSize(200.0f)
            [
                SNew(SBox)
                .MaxDesiredWidth(500.0f)
                [
                    LeftSidebarWidget.ToSharedRef()
                ]
            ]

            + SSplitter::Slot()
            .Value(0.5f)
            .MinSize(400.0f)
            [
                ContentAreaWidget.ToSharedRef()
            ]

            + SSplitter::Slot()
            .Value(0.3f)
            .MinSize(250.0f)
            [
                SNew(SBox)
                .MaxDesiredWidth(600.0f)
                [
                    InspectorWidget.ToSharedRef()
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
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(4.0f)
        [
            SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Entity List - Coming Soon")))
            ]
        ];
}

auto SCkDebuggerWindow_Main::Build_ContentArea() -> TSharedRef<SWidget>
{
    if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
    {
        const auto Context = FCkDebuggerPageContext
        {
            SelectionModel,
            WorldModel
        };

        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(4.0f)
            [
                Pages[ActivePageIndex]->Build_Content(Context)
            ];
    }

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(4.0f)
        [
            SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("No Page Selected")))
            ]
        ];
}

auto SCkDebuggerWindow_Main::Build_InspectorPanel() -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(4.0f)
        [
            SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Inspector - Coming Soon")))
            ]
        ];
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

    if (Pages[ActivePageIndex].IsValid())
    {
        Pages[ActivePageIndex]->Set_IsActive(true);
    }

    RebuildContentArea();
}

auto SCkDebuggerWindow_Main::RebuildContentArea() -> void
{
    ContentAreaWidget = Build_ContentArea();
}

auto SCkDebuggerWindow_Main::Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>
{
    return SelectionModel;
}

auto SCkDebuggerWindow_Main::Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>
{
    return WorldModel;
}