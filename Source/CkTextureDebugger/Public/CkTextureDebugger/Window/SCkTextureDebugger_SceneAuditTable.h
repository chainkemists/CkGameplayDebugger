#pragma once

#include "CkEditorTools/Style/CkStyle.h"
#include "CkTextureDebugger/Data/CkTextureDebugger_Types.h"
#include "UObject/ObjectKey.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class STableViewBase;
template<typename T> class SListView;
class UPrimitiveComponent;

class SCkTextureDebugger_SceneAuditTable final : public SCompoundWidget
{
public:
    struct FRowKey
    {
        FObjectKey Component;
        FString ComponentPath;

        auto operator==(const FRowKey& InOther) const -> bool
        {
            return Component == InOther.Component
                && ComponentPath == InOther.ComponentPath;
        }

        friend auto GetTypeHash(const FRowKey& InKey) -> uint32
        {
            return HashCombine(
                GetTypeHash(InKey.Component),
                GetTypeHash(InKey.ComponentPath));
        }
    };

    struct FRow
    {
        FRowKey Key;
        FString ActorPath;
        FString Actor;
        FString Component;
        FString ClassName;
        FString Kind;
        FString State;
        FString Search;
        int32 Slots = 0;
        int32 Textures = 0;
        ECk_Tone Tone = ECk_Tone::Neutral;
        bool bHighlight = true;
        TWeakObjectPtr<UPrimitiveComponent> Target;
    };

    SLATE_BEGIN_ARGS(SCkTextureDebugger_SceneAuditTable) {}
        SLATE_ARGUMENT(TFunction<void(TWeakObjectPtr<UPrimitiveComponent>)>, OnComponentSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto SetSnapshot(const FCkTextureDebugger_LoadedWorldSnapshot& InSnapshot) -> void;
    auto SetSelectedComponent(TWeakObjectPtr<UPrimitiveComponent> InComponent) -> void;

    auto Get_VisibleRowCount() const -> int32;
    auto Get_TotalRowCount() const -> int32;
    auto Get_SelectedRowCount() const -> int32;

private:
    auto Reconcile(const FCkTextureDebugger_LoadedWorldSnapshot& InSnapshot) -> void;
    auto RebuildVisible() -> void;
    auto RestoreSelection() -> void;
    auto ClearSelection(bool bNotify) -> void;
    auto Matches(const FRow& InRow, const FString& InQuery) const -> bool;
    auto GetEmptyText() const -> FText;
    auto GetCountText() const -> FText;
    auto GetSelectionText() const -> FText;
    auto MakeCopyText(const FRow& InRow) const -> FString;
    auto OnFilter(const FString& InText) -> void;
    auto OnHighlight(const FString& InText) -> void;
    auto OnClear() -> FReply;
    auto OnContextMenu() -> TSharedPtr<SWidget>;
    auto OnGenerateRow(
        TSharedPtr<FRow> InItem,
        const TSharedRef<STableViewBase>& InOwner)
        -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(
        TSharedPtr<FRow> InItem,
        ESelectInfo::Type InType)
        -> void;

    TArray<TSharedPtr<FRow>> _All;
    TArray<TSharedPtr<FRow>> _Visible;
    TSharedPtr<SListView<TSharedPtr<FRow>>> _List;
    TWeakObjectPtr<UPrimitiveComponent> _Selected;
    TFunction<void(TWeakObjectPtr<UPrimitiveComponent>)> _OnSelected;
    FString _Filter;
    FString _Highlight;
    bool _HasWorld = false;
};
