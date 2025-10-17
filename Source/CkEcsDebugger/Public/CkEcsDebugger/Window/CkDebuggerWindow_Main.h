#pragma once

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Base.h"

#include "SlateIM/Public/SlateIMWidgetBase.h"

class FCkDebuggerPage_Overview;

class FCkDebuggerWindow_Main : public FSlateIMWindowBase
{
public:
    FCkDebuggerWindow_Main(UWorld* InWorld);

    auto Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>
    {
        return SelectionModel;
    }

    auto Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>
    {
        return WorldModel;
    }

protected:
    auto DrawWindow(float InDeltaTime) -> void override;

private:
    auto Draw_LeftSidebar() -> void;
    auto Draw_PageButtons() -> void;
    auto Draw_EntityList() -> void;
    auto Draw_ContentArea() -> void;
    auto Draw_InspectorPanel() -> void;

    auto InitializePages() -> void;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TArray<TSharedPtr<ICkDebuggerPage_Base>> Pages;
    int32 ActivePageIndex = 0;

    // UI State
    float LeftPanelWidth = 300.0f;
    float RightPanelWidth = 350.0f;
};