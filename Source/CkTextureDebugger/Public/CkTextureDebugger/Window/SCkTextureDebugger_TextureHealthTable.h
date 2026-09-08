#pragma once

#include "CkTextureDebugger/Data/CkTextureDebugger_Types.h"

#include "CoreMinimal.h"
#include "CkSlateLayout/CkUiDocument.h"
#include "Styling/SlateBrush.h"
#include "UObject/ObjectKey.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class STableViewBase;
class SCkDebug_DualSearchBar;
class UPrimitiveComponent;
class UTexture;
class FCkUiView;
class FCkUiCollection;
class SCkUiTable;

// --------------------------------------------------------------------------------------------------------------------

/** Copied selection data. Weak targets are navigation/preview capability only. */
struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_TextureHealthSelection
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    TWeakObjectPtr<UTexture> Texture;
    int32 SlotIndex = INDEX_NONE;
    FString DisplayName;
    FSoftObjectPath TexturePath;
    FString Provenance;
    FString Details;
    FCkTextureDebugger_TextureHealth Health;
};

DECLARE_DELEGATE_OneParam(
    FOnCkTextureDebugger_TextureHealthSelectionChanged,
    const FCkTextureDebugger_TextureHealthSelection&);

// --------------------------------------------------------------------------------------------------------------------

/**
 * Runtime-safe, searchable loaded-world texture table.
 *
 * Contextual component selection is a visual highlight only: it never narrows the source list. Rows retain stable
 * Slate identity across snapshots and only the actively previewed texture is strongly rooted.
 */
class CKTEXTUREDEBUGGER_API SCkTextureDebugger_TextureHealthTable final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkTextureDebugger_TextureHealthTable) : _UseYogaLayout(true) {}
        SLATE_ARGUMENT(bool, UseYogaLayout)
        SLATE_EVENT(FOnCkTextureDebugger_TextureHealthSelectionChanged, OnSelectionChanged)
    SLATE_END_ARGS()

    /** Designer reload entry points also used by the live file polling path. */
    auto TryReload_Layout(const FString& InMarkup, const FString& InStylesheet) -> FCkUiLoadResult;
    auto Reload_LayoutFiles(const FString& InMarkupPath, const FString& InStylesheetPath) -> FCkUiLoadResult;
    auto Poll_LayoutFiles() -> bool;
    auto Get_LayoutRevision() const -> int64;
    auto Get_LayoutError() const -> FText;
    auto Get_AuthoredTable() const -> TSharedPtr<SCkUiTable>;
    auto Get_AuthoredSplitter() const -> TSharedPtr<class SCkUiSplitter>;

    auto
        Construct(
            const FArguments& InArgs)
        -> void;

    /** Reconciles all loaded-world texture rows; InSelectedComponent only marks contextual rows. */
    auto Set_Snapshot(
        const FCkTextureDebugger_LoadedWorldSnapshot& InSnapshot,
        TWeakObjectPtr<UPrimitiveComponent> InSelectedComponent = {}) -> void;

    /** Drops the selected preview root and reports an empty selection to the owner. */
    auto Clear_Selection() -> void;

    /** Retains selection only when its exact component/slot/material/texture row remains visible. */
    auto Reconcile_Selection() -> void;

    auto Get_VisibleRowCount() const -> int32;
    auto Get_TotalRowCount() const -> int32;
    auto Get_Selection() const -> TOptional<FCkTextureDebugger_TextureHealthSelection>;

private:
    struct FRowKey
    {
        FObjectKey ComponentKey;
        FSoftObjectPath ComponentPath;
        int32 SlotIndex = INDEX_NONE;
        FObjectKey MaterialKey;
        FSoftObjectPath MaterialPath;
        FObjectKey TextureKey;
        FSoftObjectPath TexturePath;

        auto operator==(const FRowKey& InOther) const -> bool
        {
            return ComponentKey == InOther.ComponentKey
                && ComponentPath == InOther.ComponentPath
                && SlotIndex == InOther.SlotIndex
                && MaterialKey == InOther.MaterialKey
                && MaterialPath == InOther.MaterialPath
                && TextureKey == InOther.TextureKey
                && TexturePath == InOther.TexturePath;
        }

        friend auto GetTypeHash(const FRowKey& InKey) -> uint32
        {
            auto Result = GetTypeHash(InKey.ComponentKey);
            Result = HashCombine(Result, GetTypeHash(InKey.ComponentPath));
            Result = HashCombine(Result, GetTypeHash(InKey.SlotIndex));
            Result = HashCombine(Result, GetTypeHash(InKey.MaterialKey));
            Result = HashCombine(Result, GetTypeHash(InKey.MaterialPath));
            Result = HashCombine(Result, GetTypeHash(InKey.TextureKey));
            return HashCombine(Result, GetTypeHash(InKey.TexturePath));
        }
    };

public:
    struct FRow
    {
        FRowKey Key;
        FString UiKey;
        TWeakObjectPtr<UPrimitiveComponent> Component;
        TWeakObjectPtr<UTexture> Texture;
        FString ComponentLabel;
        FString MaterialLabel;
        FString Provenance;
        FCkTextureDebugger_TextureHealth Health;
        int32 ExactDuplicateCount = 1;
        bool IsContextComponent = false;
        bool IsHighlightMatch = true;
    };

private:
    auto Construct_Yoga(const FArguments& InArgs) -> void;
    auto Construct_Native(const FArguments& InArgs) -> void;
    auto Tick_LayoutFiles(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType;
    auto Rebuild_Rows() -> void;
    auto
        MatchesSearch(
            const FRow& InRow,
            const FString& InNeedle) const
        -> bool;
    auto Make_Row(
        const FCkTextureDebugger_ComponentRow& InComponent,
        const FCkTextureDebugger_MaterialSlotRow& InSlot,
        const FCkTextureDebugger_TextureRow& InTexture) const -> FRow;
    auto
        Make_Selection(
            const FRow& InRow) const
        -> FCkTextureDebugger_TextureHealthSelection;
    auto Make_Details(const FRow& InRow) const -> FString;
    auto Set_PreviewTexture(TWeakObjectPtr<UTexture> InTexture) -> void;
    auto Clear_PreviewTexture() -> void;
    auto Find_Row(const FRowKey& InKey) const -> TSharedPtr<FRow>;
    auto Get_EmptyStateText() const -> FText;
    auto Get_FilterText() const -> FText;
    auto Get_HighlightText() const -> FText;
    auto Get_RowCountText() const -> FText;
    auto Get_SelectedDetailsText() const -> FText;
    auto Get_PreviewBrush() const -> const FSlateBrush*;
    auto Has_Selection() const -> bool;
    auto On_FilterTextChanged(const FText& InText) -> void;
    auto On_HighlightTextChanged(const FText& InText) -> void;
    auto
        OnGenerateRow(
            TSharedPtr<FRow> InItem,
            const TSharedRef<STableViewBase>& InOwnerTable)
        -> TSharedRef<ITableRow>;
    auto
        OnSelectionChanged(
            TSharedPtr<FRow> InItem,
            ESelectInfo::Type InSelectInfo)
        -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;
    auto On_CopyTextureDetails(const FString& InKey) -> void;
    auto On_AuthoredSelectionChanged(TOptional<FString> InKey, ESelectInfo::Type InSelectInfo) -> void;

    FOnCkTextureDebugger_TextureHealthSelectionChanged _OnSelectionChanged;
    FCkTextureDebugger_LoadedWorldSnapshot _Snapshot;
    TWeakObjectPtr<UPrimitiveComponent> _SelectedComponent;
    TArray<TSharedPtr<FRow>> _AllRows;
    TArray<TSharedPtr<FRow>> _Rows;
    TSharedPtr<SListView<TSharedPtr<FRow>>> _ListView;
    TSharedPtr<SCkDebug_DualSearchBar> _SearchBar;
    TSharedPtr<FCkUiView> _LayoutView;
    TSharedPtr<FCkUiCollection> _UiCollection;
    TMap<FString, TWeakPtr<FRow>> _UiRows;
    FString _PublicationError;
    TOptional<FRowKey> _SelectedKey;
    TStrongObjectPtr<UTexture> _PreviewTextureRoot;
    FSlateBrush _PreviewBrush;
    FString _FilterString;
    FString _HighlightString;
    int32 _TotalRowCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------
