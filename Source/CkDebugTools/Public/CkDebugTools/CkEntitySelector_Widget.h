//// CkEntitySelector_Widget.h
//#pragma once
//
//#include "CkDebugTools/CkDebugTools_Style.h"
//
//#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"
//#include "CkEcsDebugger/Windows/EntitySelection/CkEntitySelection_DebugConfig.h"
//
//#include <Widgets/SCompoundWidget.h>
//#include <Widgets/Views/STreeView.h>
//#include <Widgets/Input/SSearchBox.h>
//#include <Widgets/Input/SComboBox.h>
//#include <Framework/Commands/UIAction.h>
//
//// --------------------------------------------------------------------------------------------------------------------
//
//struct FCkEntityTreeItem
//{
//    FCk_Handle Entity;
//    TArray<TSharedPtr<FCkEntityTreeItem>> Children;
//    bool IsFiltered = false;
//
//    FCkEntityTreeItem(const FCk_Handle& InEntity) : Entity(InEntity) {}
//};
//
//// --------------------------------------------------------------------------------------------------------------------
//
//class CKDEBUGTOOLS_API SCkEntitySelector : public SCompoundWidget
//{
//public:
//    SLATE_BEGIN_ARGS(SCkEntitySelector) {}
//        SLATE_EVENT(FSimpleDelegate, OnSelectionChanged)
//    SLATE_END_ARGS()
//
//    auto Construct(const FArguments& InArgs) -> void;
//
//    // Get currently selected entities
//    auto Get_SelectedEntities() const -> TArray<FCk_Handle>;
//
//    // Force refresh of the entity tree
//    auto Request_RefreshEntityTree() -> void;
//
//private:
//    // UI Creation
//    auto DoCreateToolbar() -> TSharedRef<SWidget>;
//    auto DoCreateEntityTree() -> TSharedRef<SWidget>;
//
//    // Tree Management
//    auto DoRefreshEntityList(bool InForceRefresh = false) -> void;
//    auto DoSortEntities(TArray<FCk_Handle>& InEntities) -> void;
//    auto DoFilterEntities(const TArray<FCk_Handle>& InEntities) -> TArray<FCk_Handle>;
//    auto DoBuildTreeHierarchy(const TArray<FCk_Handle>& InEntities) -> TArray<TSharedPtr<FCkEntityTreeItem>>;
//    auto DoAddChildrenToTree(const TSharedPtr<FCkEntityTreeItem>& InParent, const TArray<FCk_Handle>& InAllEntities) -> void;
//
//    // Entity Discovery
//    auto DoGetAllEntitiesFromWorld() -> TArray<FCk_Handle>;
//    auto DoPassesFragmentFilter(const FCk_Handle& InEntity) -> bool;
//
//    // Tree View Callbacks
//    auto DoGenerateTreeRow(TSharedPtr<FCkEntityTreeItem> InItem, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>;
//    auto DoGetTreeChildren(TSharedPtr<FCkEntityTreeItem> InItem, TArray<TSharedPtr<FCkEntityTreeItem>>& OutChildren) -> void;
//    auto DoOnTreeSelectionChanged(TSharedPtr<FCkEntityTreeItem> InItem, ESelectInfo::Type SelectInfo) -> void;
//    auto DoOnTreeDoubleClick(TSharedPtr<FCkEntityTreeItem> InItem) -> void;
//
//    // Menu Actions
//    auto DoCreateDisplayMenu() -> TSharedRef<SWidget>;
//    auto DoCreateSortingMenu() -> TSharedRef<SWidget>;
//    auto DoCreateFilteringMenu() -> TSharedRef<SWidget>;
//    auto DoCreateUpdateMenu() -> TSharedRef<SWidget>;
//    auto DoCreateSelectionMenu() -> TSharedRef<SWidget>;
//
//    // Event Handlers
//    auto DoOnSearchTextChanged(const FText& InText) -> void;
//    auto DoOnDisplayPolicyChanged(ECkDebugger_EntitiesListDisplayPolicy InNewPolicy) -> void;
//    auto DoOnSortingPolicyChanged(ECkDebugger_EntitiesListSortingPolicy InNewPolicy) -> void;
//    auto DoOnFilteringChanged(ECkDebugger_EntitiesListFragmentFilteringTypes InNewFilter) -> void;
//    auto DoOnUpdatePolicyChanged(ECkDebugger_EntitiesListUpdatePolicy InNewPolicy) -> void;
//    auto DoOnClearSelection() -> void;
//
//    // Update Management
//    auto DoShouldUpdateThisFrame() -> bool;
//    auto Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;
//
//private:
//    // Configuration (mimicking the ImGui version)
//    ECkDebugger_EntitiesListDisplayPolicy _DisplayPolicy = ECkDebugger_EntitiesListDisplayPolicy::EntityHierarchy;
//    ECkDebugger_EntitiesListSortingPolicy _SortingPolicy = ECkDebugger_EntitiesListSortingPolicy::Alphabetical;
//    ECkDebugger_EntitiesListUpdatePolicy _UpdatePolicy = ECkDebugger_EntitiesListUpdatePolicy::PerFrame;
//    ECkDebugger_EntitiesListFragmentFilteringTypes _FragmentFilter = ECkDebugger_EntitiesListFragmentFilteringTypes::None;
//
//    // UI Components
//    TSharedPtr<STreeView<TSharedPtr<FCkEntityTreeItem>>> _EntityTreeView;
//    TSharedPtr<SSearchBox> _SearchBox;
//
//    // Data
//    TArray<TSharedPtr<FCkEntityTreeItem>> _TreeItems;
//    TArray<FCk_Handle> _CachedEntities;
//    FString _SearchFilter;
//
//    // Update timing
//    float _LastUpdateTime = 0.0f;
//    bool _NeedsRefresh = true;
//
//    // Events
//    FSimpleDelegate OnSelectionChanged;
//
//    // World context
//    auto Get_CurrentWorld() const -> UWorld*;
//    auto Get_DebuggerSubsystem() const -> UCk_EcsDebugger_Subsystem_UE*;
//    auto Get_TransientEntity() const -> FCk_Handle;
//};
