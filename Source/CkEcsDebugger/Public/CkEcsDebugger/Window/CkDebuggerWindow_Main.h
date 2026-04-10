#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerModel_ViewportPicker;
class FCkDebuggerModel_InspectorFilter;
class ICkDebuggerPage_Base;
class SBox;
class SCkDebuggerPanel_Inspector;
class SCkDebuggerPanel_EntityList;
class SHorizontalBox;
class SMenuAnchor;

class SCkDebuggerWindow_Main : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerWindow_Main) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;
    auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;

    auto Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>;
    auto Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>;
    auto Get_ViewportPicker() const -> TSharedPtr<FCkDebuggerModel_ViewportPicker>;
    auto Get_FilterModel() const -> TSharedPtr<FCkDebuggerModel_InspectorFilter>;

private:
    auto Build_Toolbar() -> TSharedRef<SWidget>;
    auto Build_PickerSettingsPopover() -> TSharedRef<SWidget>;
    auto Build_FilterPopover() -> TSharedRef<SWidget>;
    auto Refresh_FilterBadgeStrip() -> void;
    auto Build_LeftSidebar() -> TSharedRef<SWidget>;
    auto Build_ContentArea() -> TSharedRef<SWidget>;
    auto Build_InspectorPanel() -> TSharedRef<SWidget>;

    auto OnPageSelected(int32 InPageIndex) -> void;
    auto RebuildContentArea() -> void;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TSharedPtr<FCkDebuggerModel_ViewportPicker> ViewportPicker;
    TSharedPtr<FCkDebuggerModel_InspectorFilter> FilterModel;
    TArray<TSharedPtr<ICkDebuggerPage_Base>> Pages;
    int32 ActivePageIndex = 0;

    TSharedPtr<SBox> ContentAreaContainer;
    TSharedPtr<SCkDebuggerPanel_EntityList> EntityListPanel;
    TSharedPtr<SCkDebuggerPanel_Inspector> InspectorPanel;

    TSharedPtr<SMenuAnchor> PickerSettingsAnchor;
    TSharedPtr<SMenuAnchor> FilterAnchor;
    TSharedPtr<SHorizontalBox> FilterBadgeStrip;
    FDelegateHandle FilterChangedHandle;
};