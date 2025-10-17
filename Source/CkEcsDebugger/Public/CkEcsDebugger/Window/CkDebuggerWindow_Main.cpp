#include "CkDebuggerWindow_Main.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Overview.h"

#include "SlateIM/Public/SlateIM.h"

FCkDebuggerWindow_Main::FCkDebuggerWindow_Main(UWorld* InWorld)
    : FSlateIMWindowBase(
        TEXT("CkFoundation ECS Debugger"),
        FVector2f(1600.0f, 900.0f),
        TEXT("ck.EcsDebugger.Toggle"),
        TEXT("Toggle the CkFoundation ECS Debugger"))
{
    SelectionModel = MakeShared<FCkDebuggerModel_EntitySelection>();
    WorldModel = MakeShared<FCkDebuggerModel_WorldContext>();

    if (ck::IsValid(InWorld))
    {
        WorldModel->Set_SelectedWorld(InWorld);
    }

    // Initialize widgets
    EntityTree.Initialize(SelectionModel, WorldModel);

    // Bind search callback
    SearchBar.OnSearchChanged.BindRaw(this, &FCkDebuggerWindow_Main::OnSearchChanged);

    InitializePages();
}

auto FCkDebuggerWindow_Main::DrawWindow(const float InDeltaTime) -> void
{
    if (Pages.IsValidIndex(ActivePageIndex))
    {
        Pages[ActivePageIndex]->Tick(InDeltaTime);
    }

    // Main horizontal layout: Left | Center | Right
    SlateIM::BeginHorizontalStack();
    {
        // Left Sidebar
        SlateIM::MinWidth(LeftPanelWidth);
        SlateIM::MaxWidth(LeftPanelWidth);
        SlateIM::BeginBorder(TEXT("ToolPanel.GroupBorder"), Orient_Vertical, true, FMargin(4.0f));
        {
            Draw_LeftSidebar();
        }
        SlateIM::EndBorder();

        // Center Content Area
        SlateIM::Fill();
        SlateIM::BeginBorder(TEXT("ToolPanel.GroupBorder"), Orient_Vertical, true, FMargin(4.0f));
        {
            Draw_ContentArea();
        }
        SlateIM::EndBorder();

        // Right Inspector Panel
        SlateIM::MinWidth(RightPanelWidth);
        SlateIM::MaxWidth(RightPanelWidth);
        SlateIM::BeginBorder(TEXT("ToolPanel.GroupBorder"), Orient_Vertical, true, FMargin(4.0f));
        {
            Draw_InspectorPanel();
        }
        SlateIM::EndBorder();
    }
    SlateIM::EndHorizontalStack();
}

auto FCkDebuggerWindow_Main::Draw_LeftSidebar() -> void
{
    SlateIM::BeginVerticalStack();
    {
        // Page buttons at the top
        Draw_PageButtons();

        SlateIM::Spacer(FVector2D(0.0f, 8.0f));

        // Toolbar with actions
        Draw_Toolbar();

        SlateIM::Spacer(FVector2D(0.0f, 4.0f));

        // Search bar
        SearchBar.Draw();

        SlateIM::Spacer(FVector2D(0.0f, 4.0f));

        // Entity tree (fills remaining space)
        EntityTree.Draw();

        // Status bar at bottom
        Draw_StatusBar();
    }
    SlateIM::EndVerticalStack();
}

auto FCkDebuggerWindow_Main::Draw_PageButtons() -> void
{
    SlateIM::BeginVerticalStack();
    {
        for (int32 i = 0; i < Pages.Num(); ++i)
        {
            const auto& Page = Pages[i];
            const auto IsActive = i == ActivePageIndex;

            SlateIM::Padding(FMargin(4.0f));

            if (SlateIM::Button(Page->Get_PageName().ToString()))
            {
                if (Pages.IsValidIndex(ActivePageIndex))
                {
                    Pages[ActivePageIndex]->SetActive(false);
                }

                ActivePageIndex = i;
                Pages[ActivePageIndex]->SetActive(true);
            }
        }
    }
    SlateIM::EndVerticalStack();
}

auto FCkDebuggerWindow_Main::Draw_Toolbar() -> void
{
    SlateIM::BeginHorizontalStack();
    {
        // Refresh button
        SlateIM::AutoSize();
        SlateIM::Padding(FMargin(2.0f));
        if (SlateIM::Button(TEXT("Refresh")))
        {
            EntityTree.RefreshTree();
        }

        SlateIM::Spacer(FVector2D(4.0f, 0.0f));

        // Expand All button
        SlateIM::AutoSize();
        SlateIM::Padding(FMargin(2.0f));
        if (SlateIM::Button(TEXT("Expand All")))
        {
            EntityTree.ExpandAll();
        }

        SlateIM::Spacer(FVector2D(4.0f, 0.0f));

        // Collapse All button
        SlateIM::AutoSize();
        SlateIM::Padding(FMargin(2.0f));
        if (SlateIM::Button(TEXT("Collapse All")))
        {
            EntityTree.CollapseAll();
        }

        // Spacer to push remaining buttons to the right
        SlateIM::Fill();
    }
    SlateIM::EndHorizontalStack();
}

auto FCkDebuggerWindow_Main::Draw_StatusBar() -> void
{
    SlateIM::Padding(FMargin(4.0f, 2.0f));

    const auto VisibleCount = EntityTree.Get_VisibleEntityCount();
    const auto TotalCount = EntityTree.Get_TotalEntityCount();

    FString StatusText;
    if (SearchBar.IsActive())
    {
        StatusText = FString::Printf(TEXT("%d entities (%d visible)"), TotalCount, VisibleCount);
    }
    else
    {
        StatusText = FString::Printf(TEXT("%d entities"), TotalCount);
    }

    SlateIM::Text(StatusText, FLinearColor(0.7f, 0.7f, 0.7f));
}

auto FCkDebuggerWindow_Main::Draw_EntityList() -> void
{
    // This method is now integrated into Draw_LeftSidebar
    EntityTree.Draw();
}

auto FCkDebuggerWindow_Main::Draw_ContentArea() -> void
{
    if (NOT Pages.IsValidIndex(ActivePageIndex))
    {
        SlateIM::Text(TEXT("No Page Selected"));
        return;
    }

    const FCkDebuggerPageContext Context
    {
        SelectionModel,
        WorldModel
    };

    Pages[ActivePageIndex]->Draw(Context);
}

auto FCkDebuggerWindow_Main::Draw_InspectorPanel() -> void
{
    SlateIM::Fill();
    SlateIM::BeginScrollBox(Orient_Vertical);
    {
        SlateIM::Text(TEXT("Inspector Panel Coming Soon"));
    }
    SlateIM::EndScrollBox();
}

auto FCkDebuggerWindow_Main::InitializePages() -> void
{
    Pages.Empty();

    auto OverviewPage = MakeShared<FCkDebuggerPage_Overview>();
    OverviewPage->SetActive(true);
    Pages.Add(OverviewPage);

    ActivePageIndex = 0;
}

auto FCkDebuggerWindow_Main::OnSearchChanged(const FString& InSearchText) -> void
{
    EntityTree.SetFilterText(InSearchText);
}