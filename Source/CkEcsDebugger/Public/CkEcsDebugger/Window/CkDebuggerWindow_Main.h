#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class ICkDebuggerPage_Base;
class SCkDebuggerPanel_Inspector;
class SCkDebuggerPanel_EntityList;

class SCkDebuggerWindow_Main : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerWindow_Main) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>;
    auto Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>;

private:
    auto Build_LeftSidebar() -> TSharedRef<SWidget>;
    auto Build_ContentArea() -> TSharedRef<SWidget>;
    auto Build_InspectorPanel() -> TSharedRef<SWidget>;

    auto OnPageSelected(int32 InPageIndex) -> void;
    auto RebuildContentArea() -> void;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TArray<TSharedPtr<ICkDebuggerPage_Base>> Pages;
    int32 ActivePageIndex = 0;

    TSharedPtr<SWidget> ContentAreaWidget;
    TSharedPtr<SCkDebuggerPanel_EntityList> EntityListPanel;
    TSharedPtr<SCkDebuggerPanel_Inspector> InspectorPanel;
};