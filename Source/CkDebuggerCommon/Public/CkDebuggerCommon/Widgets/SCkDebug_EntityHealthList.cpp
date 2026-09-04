#include "SCkDebug_EntityHealthList.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::debug_entity_health_list
{
    namespace
    {
        auto HasSameRenderedContent(
            const FCkDebug_EntityHealthItem& InLeft,
            const FCkDebug_EntityHealthItem& InRight) -> bool
        {
            return InLeft.RowIdentity == InRight.RowIdentity
                && InLeft.SelectionTarget == InRight.SelectionTarget
                && InLeft.Name.EqualTo(InRight.Name)
                && InLeft.Summary.EqualTo(InRight.Summary)
                && InLeft.Context.EqualTo(InRight.Context)
                && InLeft.Status.EqualTo(InRight.Status)
                && InLeft.Tone == InRight.Tone;
        }
    }

    auto Try_Reconcile(
        const TArray<FItemPtr>& InExisting,
        TArray<FCkDebug_EntityHealthItem> InIncoming,
        TArray<FItemPtr>& OutItems,
        bool& OutRenderedContentChanged,
        FString& OutError) -> bool
    {
        auto IncomingIdentities = TSet<FCk_Handle>{};
        for (const auto& Item : InIncoming)
        {
            if (ck::Is_NOT_Valid(Item.RowIdentity))
            {
                OutError = TEXT("Entity health row identity must be valid.");
                return false;
            }
            if (ck::Is_NOT_Valid(Item.SelectionTarget))
            {
                OutError = TEXT("Entity health selection target must be valid.");
                return false;
            }
            if (IncomingIdentities.Contains(Item.RowIdentity))
            {
                OutError = TEXT("Entity health row identity is duplicated.");
                return false;
            }
            IncomingIdentities.Add(Item.RowIdentity);
        }

        auto ExistingByIdentity = TMap<FCk_Handle, FItemPtr>{};
        for (const auto& Existing : InExisting)
        {
            if (Existing.IsValid() && ck::IsValid(Existing->RowIdentity))
            { ExistingByIdentity.FindOrAdd(Existing->RowIdentity) = Existing; }
        }

        auto Reconciled = TArray<FItemPtr>{};
        Reconciled.Reserve(InIncoming.Num());
        auto PublishedPointers = TSet<const FCkDebug_EntityHealthItem*>{};
        for (const auto& Incoming : InIncoming)
        {
            const auto* Existing = ExistingByIdentity.Find(Incoming.RowIdentity);
            const auto Item = Existing != nullptr ? *Existing : MakeShared<FCkDebug_EntityHealthItem>(Incoming);
            if (PublishedPointers.Contains(Item.Get()))
            {
                OutError = TEXT("Entity health reconciliation would publish a duplicate row pointer.");
                return false;
            }
            PublishedPointers.Add(Item.Get());
            Reconciled.Add(Item);
        }

        auto RenderedContentChanged = InExisting.Num() != Reconciled.Num();
        if (NOT RenderedContentChanged)
        {
            for (auto Index = 0; Index < Reconciled.Num(); ++Index)
            {
                RenderedContentChanged = InExisting[Index] != Reconciled[Index]
                    || NOT HasSameRenderedContent(*Reconciled[Index], InIncoming[Index]);
                if (RenderedContentChanged)
                { break; }
            }
        }

        // All rejection checks ran before this point, so retained row data is only changed as one accepted update.
        for (auto Index = 0; Index < Reconciled.Num(); ++Index)
        { *Reconciled[Index] = MoveTemp(InIncoming[Index]); }

        OutError.Reset();
        OutItems = MoveTemp(Reconciled);
        OutRenderedContentChanged = RenderedContentChanged;
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_EntityHealthRow::Construct(const FArguments& InArgs) -> void
{
    const auto Item = InArgs._Item;
    const auto Selected = InArgs._SelectedEntity;
    const auto OnSelected = InArgs._OnSelected;

    ChildSlot
    [
        SNew(SCkDebug_ToggleSurface)
        .IsOn_Lambda([Item, Selected] { return Item.IsValid() && Item->SelectionTarget == Selected.Get(); })
        .AccessibleText(Item.IsValid() ? Item->Name : FText::GetEmpty())
        .ToolTipText(Item.IsValid() ? Item->Summary : FText::GetEmpty())
        .OnStateChanged_Lambda([Item, OnSelected](bool InIsSelected)
        {
            if (InIsSelected && Item.IsValid() && OnSelected.IsBound())
            { OnSelected.Execute(Item->RowIdentity); }
        })
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [SNew(STextBlock).Text(Item.IsValid() ? Item->Name : FText::GetEmpty()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SCkDebug_StatusPill).Text(Item.IsValid() ? Item->Status : FText::GetEmpty()).Tone(Item.IsValid() ? Item->Tone : ECk_Tone::Neutral).ShowDot(true)]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(Item.IsValid() ? Item->Summary : FText::GetEmpty())
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(CkStyle::TextDim())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(Item.IsValid() ? Item->Context : FText::GetEmpty())
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(CkStyle::TextMute())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                .Visibility_Lambda([Item]
                {
                    return Item.IsValid() && NOT Item->Context.IsEmpty()
                        ? EVisibility::SelfHitTestInvisible
                        : EVisibility::Collapsed;
                })
            ]
        ]
    ];
}

auto SCkDebug_EntityHealthList::Construct(const FArguments& InArgs) -> void
{
    _SelectedEntity = InArgs._SelectedEntity;
    _OnSelected = InArgs._OnSelected;
    ChildSlot
    [
        SAssignNew(_List, SListView<TSharedPtr<FCkDebug_EntityHealthItem>>)
        .ListItemsSource(&_Items)
        .OnGenerateRow(this, &SCkDebug_EntityHealthList::GenerateRow)
        .SelectionMode(ESelectionMode::None)
    ];
}

auto SCkDebug_EntityHealthList::Set_Items(TArray<FCkDebug_EntityHealthItem> InItems) -> bool
{
    auto Reconciled = TArray<TSharedPtr<FCkDebug_EntityHealthItem>>{};
    auto RenderedContentChanged = false;
    auto Error = FString{};
    const auto ReconciliationSucceeded = ck::debug_entity_health_list::Try_Reconcile(
        _Items,
        MoveTemp(InItems),
        Reconciled,
        RenderedContentChanged,
        Error);
    CK_ENSURE_IF_NOT(ReconciliationSucceeded, TEXT("Cannot set entity health rows: {}"), Error)
    { return false; }

    _Items = MoveTemp(Reconciled);
    if (RenderedContentChanged && _List.IsValid()) { _List->RequestListRefresh(); }
    return true;
}

auto SCkDebug_EntityHealthList::Clear_Items() -> void
{
    const auto HadItems = NOT _Items.IsEmpty();
    _Items.Reset();
    if (HadItems && _List.IsValid()) { _List->RequestListRefresh(); }
}

auto SCkDebug_EntityHealthList::GenerateRow(
    TSharedPtr<FCkDebug_EntityHealthItem> InItem,
    const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>
{
    return SNew(STableRow<TSharedPtr<FCkDebug_EntityHealthItem>>, InOwner)
    [
        SNew(SCkDebug_EntityHealthRow)
        .Item(MoveTemp(InItem))
        .SelectedEntity(_SelectedEntity)
        .OnSelected(_OnSelected)
    ];
}

// --------------------------------------------------------------------------------------------------------------------
