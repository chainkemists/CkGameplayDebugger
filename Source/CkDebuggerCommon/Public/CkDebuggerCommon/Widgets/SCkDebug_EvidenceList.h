#pragma once

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// ====================================================================================================================

/** One readable, cross-system evidence line. Key is caller-owned, stable, and never empty. */
struct CKDEBUGGERCOMMON_API FCkDebug_EvidenceItem
{
    FString   Key;
    ECk_Tone  Tone = ECk_Tone::Neutral;
    FText     Source;
    FText     Headline;
    FText     Detail;
    int32     IndentLevel = 0;
    FText     RightLabel;
    FString   CopyText;
    int32     SelectionId = INDEX_NONE;
};

DECLARE_DELEGATE_OneParam(FOnCkDebug_EvidenceSelected, int32 /* SelectionId */);

namespace ck::debug_evidence_list
{
    using FItemPtr = TSharedPtr<FCkDebug_EvidenceItem>;

    /**
     * Validates all incoming keys before mutating output, then preserves pointer identity for every retained key.
     * Caller order is retained; when capped, the newest (last) caller entries are retained in their original order.
     */
    CKDEBUGGERCOMMON_API auto Try_Reconcile(
        const TArray<FItemPtr>& InExisting,
        TArray<FCkDebug_EvidenceItem> InIncoming,
        int32 InMaxItems,
        TArray<FItemPtr>& OutItems,
        FString& OutError) -> bool;
}

// ====================================================================================================================

/** Bounded, selectable Common surface for current cross-system evidence. */
class CKDEBUGGERCOMMON_API SCkDebug_EvidenceList : public SCompoundWidget
{
public:
    using FItemPtr = ck::debug_evidence_list::FItemPtr;

    SLATE_BEGIN_ARGS(SCkDebug_EvidenceList)
        : _MaxItems(100)
        , _EmptyText(FText::FromString(TEXT("No current evidence.")))
    {}
        SLATE_ARGUMENT(int32, MaxItems)
        SLATE_ARGUMENT(FText, EmptyText)
        SLATE_EVENT(FOnCkDebug_EvidenceSelected, OnSelectionChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** Returns false without changing the visible list when any Key is empty or duplicated. */
    auto Set_Items(TArray<FCkDebug_EvidenceItem> InItems) -> bool;
    auto Clear_Items() -> void;

    auto Get_Items() const -> const TArray<FItemPtr>& { return _Items; }

private:
    auto GenerateRow(FItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto HandleSelectionChanged(FItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto HandleContextMenuOpening() -> TSharedPtr<SWidget>;
    auto GetEmptyVisibility() const -> EVisibility;

    TArray<FItemPtr> _Items;
    TSharedPtr<SListView<FItemPtr>> _List;
    int32 _MaxItems = 100;
    FText _EmptyText;
    FOnCkDebug_EvidenceSelected _OnSelectionChanged;
};

// ====================================================================================================================
