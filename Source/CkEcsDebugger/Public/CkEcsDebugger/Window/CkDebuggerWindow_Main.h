#pragma once

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebug_ViewportPicker;
class FCkDebuggerModel_InspectorFilter;
class FCkUiView;
class ICkDebuggerPage_Base;
class SBox;
class SCkDebuggerPanel_Inspector;
class SCkDebuggerPanel_EntityList;
class SHorizontalBox;
class SMenuAnchor;
class SCkDebug_UnderlineTabs;
class SCkDebuggerSelectionGizmo;
class UGameViewportClient;
#if WITH_EDITOR
class SLevelViewport;
#endif

class SCkDebuggerWindow_Main : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkDebuggerWindow_Main) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkDebuggerWindow_Main() override;
    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;
    auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("ECS")); }

    auto Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>;
    auto Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>;
    auto Get_ViewportPicker() const -> TSharedPtr<FCkDebug_ViewportPicker>;
    auto Get_FilterModel() const -> TSharedPtr<FCkDebuggerModel_InspectorFilter>;
    auto Get_AuthoredShellView() const -> TSharedPtr<FCkUiView> { return AuthoredShellView; }
    auto Get_AuthoredCenterView() const -> TSharedPtr<FCkUiView> { return AuthoredCenterView; }
    auto Get_AuthoredShellLoadFailure() const -> const FString& { return AuthoredShellLoadFailure; }
    auto Get_PageTabsWidget() const -> TSharedPtr<SWidget>;
    auto Get_PageContentContainer() const -> TSharedPtr<SBox> { return PageContentContainer; }
    auto Get_AuthoredLeftPane() const -> TSharedPtr<SWidget> { return AuthoredLeftPane; }
    auto Get_AuthoredCenterPane() const -> TSharedPtr<SWidget> { return AuthoredCenterPane; }
    auto Get_AuthoredInspectorPane() const -> TSharedPtr<SWidget> { return AuthoredInspectorPane; }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    auto Build_MenuActions() -> TSharedRef<SWidget>;
    auto Build_TargetControls() -> TSharedRef<SWidget>;
    auto Build_OverlaySettingsControls() -> TSharedRef<SWidget>;
    auto Build_EntityFilterControls() -> TSharedRef<SWidget>;
    auto Build_PickerExtraSettings() -> TSharedRef<SWidget>;
    auto Build_OverlayPopover() -> TSharedRef<SWidget>;
    auto Build_FilterPopover() -> TSharedRef<SWidget>;
    auto Refresh_FilterBadgeStrip() -> void;
    auto Build_LeftSidebar() -> TSharedRef<SWidget>;
    auto Build_AuthoredShell(TSharedRef<SWidget> InLeftPane, TSharedRef<SWidget> InPageTabs,
        TSharedRef<SWidget> InPageContent, TSharedRef<SWidget> InInspectorPane) -> TSharedRef<SWidget>;
    auto Poll_AuthoredShellFiles(double InCurrentTime) -> void;
    auto Build_PageTabs() -> TSharedRef<SWidget>;
    auto Build_PageContent() -> TSharedRef<SWidget>;
    auto Build_InspectorPanel() -> TSharedRef<SWidget>;

    auto OnPageSelected(int32 InPageIndex) -> void;
    auto RebuildContentArea() -> void;
    auto HandleWorldChanged(UWorld* InWorld) -> void;
    auto HandleSessionInvalidated() -> void;
    auto Update_SelectionGizmo() -> void;
    auto Reset_SelectionGizmo() -> void;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TSharedPtr<FCkDebug_ViewportPicker> ViewportPicker;
    TSharedPtr<FCkDebuggerModel_InspectorFilter> FilterModel;
    TArray<TSharedPtr<ICkDebuggerPage_Base>> Pages;
    /** Parallel to Pages -- the tab id the shared strip knows each page by. */
    TArray<FName> PageTabIds;
    int32 ActivePageIndex = 0;

    TSharedPtr<SBox> AuthoredShellHost;
    TSharedPtr<SBox> AuthoredCenterHost;
    TSharedPtr<SWidget> AuthoredLeftPane;
    TSharedPtr<SWidget> AuthoredCenterPane;
    TSharedPtr<SWidget> AuthoredInspectorPane;
    TSharedPtr<FCkUiView> AuthoredShellView;
    TSharedPtr<FCkUiView> AuthoredCenterView;
    FString AuthoredShellLoadFailure;
    double NextAuthoredShellPollSeconds = 0.0;
    /** Only the page BODY is rebuilt on selection; the tab strip is built once and binds its active state. */
    TSharedPtr<SCkDebug_UnderlineTabs> PageTabsWidget;
    TSharedPtr<SBox> PageContentContainer;
    TSharedPtr<SCkDebuggerPanel_EntityList> EntityListPanel;
    TSharedPtr<SCkDebuggerPanel_Inspector> InspectorPanel;
    TSharedPtr<SCkDebuggerSelectionGizmo> SelectionGizmo;
    TWeakObjectPtr<UGameViewportClient> SelectionGizmoViewport;
#if WITH_EDITOR
    TWeakPtr<SLevelViewport> SelectionGizmoEditorViewport;
#endif

    TSharedPtr<SMenuAnchor> OverlayAnchor;
    TSharedPtr<SMenuAnchor> FilterAnchor;
    TSharedPtr<SHorizontalBox> FilterBadgeStrip;
    FDelegateHandle FilterChangedHandle;
    FDelegateHandle WorldChangedHandle;
    FDelegateHandle SessionInvalidatedHandle;
};
