#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

struct CKDEBUGGERCOMMON_API FCkDebug_EntityHealthItem
{
    FCk_Handle Entity;
    FText Name;
    FText Summary;
    /** Optional lower-density identity/context line (entity id, role, queue, provider, etc.). */
    FText Context;
    FText Status;
    ECk_Tone Tone = ECk_Tone::Neutral;
};

DECLARE_DELEGATE_OneParam(FOnCkDebug_EntityHealthSelected, const FCk_Handle&);

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
    auto Set_Items(TArray<FCkDebug_EntityHealthItem> InItems) -> void;
    auto Clear_Items() -> void;

private:
    auto GenerateRow(TSharedPtr<FCkDebug_EntityHealthItem> InItem, const TSharedRef<STableViewBase>& InOwner)
        -> TSharedRef<ITableRow>;

    TArray<TSharedPtr<FCkDebug_EntityHealthItem>> _Items;
    TSharedPtr<SListView<TSharedPtr<FCkDebug_EntityHealthItem>>> _List;
    TAttribute<FCk_Handle> _SelectedEntity;
    FOnCkDebug_EntityHealthSelected _OnSelected;
};

// --------------------------------------------------------------------------------------------------------------------
