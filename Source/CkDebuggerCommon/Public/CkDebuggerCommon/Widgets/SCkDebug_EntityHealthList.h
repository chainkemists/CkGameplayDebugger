#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

struct CKDEBUGGERCOMMON_API FCkDebug_EntityHealthItem
{
    /** Stable physical row identity (for example, Agent.Handle); never substitute the conceptual selection target. */
    FCk_Handle RowIdentity;
    /** Entity selected when this row is activated; several physical rows may intentionally share this target. */
    FCk_Handle SelectionTarget;
    FText Name;
    FText Summary;
    /** Optional lower-density identity/context line (entity id, role, queue, provider, etc.). */
    FText Context;
    FText Status;
    ECk_Tone Tone = ECk_Tone::Neutral;
};

DECLARE_DELEGATE_OneParam(FOnCkDebug_EntityHealthSelected, const FCk_Handle&);

namespace ck::debug_entity_health_list
{
    using FItemPtr = TSharedPtr<FCkDebug_EntityHealthItem>;

    /**
     * Validates all incoming identities and selection targets before changing retained rows.
     * Retains pointer identity by RowIdentity and reports whether the list's rendered content or arrangement changed.
     */
    CKDEBUGGERCOMMON_API auto Try_Reconcile(
        const TArray<FItemPtr>& InExisting,
        TArray<FCkDebug_EntityHealthItem> InIncoming,
        TArray<FItemPtr>& OutItems,
        bool& OutRenderedContentChanged,
        FString& OutError) -> bool;
}

/** Common selectable health row for entity rosters. */
class CKDEBUGGERCOMMON_API SCkDebug_EntityHealthRow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_EntityHealthRow) {}
        SLATE_ARGUMENT(TSharedPtr<FCkDebug_EntityHealthItem>, Item)
        SLATE_ATTRIBUTE(FCk_Handle, SelectedEntity)
        SLATE_EVENT(FOnCkDebug_EntityHealthSelected, OnSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

/** Common stable-list surface for live entity-health rosters. */
class CKDEBUGGERCOMMON_API SCkDebug_EntityHealthList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_EntityHealthList) {}
        SLATE_ATTRIBUTE(FCk_Handle, SelectedEntity)
        SLATE_EVENT(FOnCkDebug_EntityHealthSelected, OnSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    /** Returns false without changing the visible list when a target is invalid or a row identity is invalid/duplicated. */
    auto Set_Items(TArray<FCkDebug_EntityHealthItem> InItems) -> bool;
    auto Clear_Items() -> void;

    auto Get_Items() const -> const TArray<TSharedPtr<FCkDebug_EntityHealthItem>>& { return _Items; }

private:
    auto GenerateRow(TSharedPtr<FCkDebug_EntityHealthItem> InItem, const TSharedRef<STableViewBase>& InOwner)
        -> TSharedRef<ITableRow>;

    TArray<TSharedPtr<FCkDebug_EntityHealthItem>> _Items;
    TSharedPtr<SListView<TSharedPtr<FCkDebug_EntityHealthItem>>> _List;
    TAttribute<FCk_Handle> _SelectedEntity;
    FOnCkDebug_EntityHealthSelected _OnSelected;
};

// --------------------------------------------------------------------------------------------------------------------
