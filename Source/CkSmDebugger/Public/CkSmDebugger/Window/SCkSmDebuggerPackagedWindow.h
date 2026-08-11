#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

class FCkSmDebugger_ViewModel;
class FCkDebuggerModel_WorldSelector;
class SCkSmDebugger_HistoryList;
class SCkSmRuntimeGraph;
class STextBlock;
class STableViewBase;
class ITableRow;
template <typename OptionType> class SComboBox;
template <typename ItemType> class SListView;

// Packaged-game state-machine debugger. The editor keeps the graph canvas; this
// widget exposes the same collected state, transition and history information
// using runtime Slate only.
class SCkSmDebuggerPackagedWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkSmDebuggerPackagedWindow) {}
    SLATE_END_ARGS()

    ~SCkSmDebuggerPackagedWindow();

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto TargetEntity(const FCk_Handle& InEntity) -> void;
    auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
    auto SupportsKeyboardFocus() const -> bool override { return true; }

    auto Get_WindowId() const -> FName override { return WindowId; }
    auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK SM Debugger")); }

private:
    struct FIndexedItem
    {
        int32 Index = INDEX_NONE;
    };

    using FIndexedItemPtr = TSharedPtr<FIndexedItem>;

    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto RefreshSmSelector() -> void;
    auto RefreshDetailLists() -> void;
    auto HandleWorldChanged(UWorld* InWorld) -> void;
    auto HandleSessionInvalidated() -> void;
    auto GenerateSmOption(TSharedPtr<FString> InItem) -> TSharedRef<SWidget>;
    auto GenerateStateRow(FIndexedItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto GenerateTransitionRow(FIndexedItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto OnSmSelectionChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto OnStateSelectionChanged(FIndexedItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;

    TSharedPtr<FCkSmDebugger_ViewModel> _ViewModel;
    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TWeakObjectPtr<UWorld> _CachedWorld;
    TOptional<FCk_Entity> _PendingTarget;

    TArray<TSharedPtr<FString>> _SmSelectorItems;
    TArray<FCk_Handle_StateMachine> _SmSelectorHandles;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> _SmSelector;
    TSharedPtr<STextBlock> _SmSelectorLabel;

    TArray<FIndexedItemPtr> _StateItems;
    TArray<FIndexedItemPtr> _TransitionItems;
    TSharedPtr<SListView<FIndexedItemPtr>> _StateList;
    TSharedPtr<SListView<FIndexedItemPtr>> _TransitionList;
    TSharedPtr<SCkSmDebugger_HistoryList> _HistoryList;
    TSharedPtr<SCkSmRuntimeGraph> _RuntimeGraph;
    FCk_Handle_StateMachine _LastDetailHandle;
    int32 _LastStateCount = INDEX_NONE;
    int32 _LastTransitionCount = INDEX_NONE;

    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _SessionInvalidatedHandle;
};
