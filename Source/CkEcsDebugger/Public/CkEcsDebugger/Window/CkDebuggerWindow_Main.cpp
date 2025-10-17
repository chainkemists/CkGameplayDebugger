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
        Draw_PageButtons();

        SlateIM::Spacer(FVector2D(0.0f, 8.0f));

        Draw_EntityList();
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

auto FCkDebuggerWindow_Main::Draw_EntityList() -> void
{
    SlateIM::Fill();
    SlateIM::BeginScrollBox(Orient_Vertical);
    {
        SlateIM::Text(TEXT("Entity List Coming Soon"));
    }
    SlateIM::EndScrollBox();
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