#include "SCkDebug_EntityHealthList.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_EntityHealthRow::Construct(const FArguments& InArgs) -> void
{
    const auto Item = InArgs._Item;
    const auto Selected = InArgs._SelectedEntity;
    const auto OnSelected = InArgs._OnSelected;

    ChildSlot
    [
        SNew(SCkDebug_ToggleSurface)
        .IsOn_Lambda([Item, Selected] { return Item.IsValid() && Item->Entity == Selected.Get(); })
        .AccessibleText(Item.IsValid() ? Item->Name : FText::GetEmpty())
        .ToolTipText(Item.IsValid() ? Item->Summary : FText::GetEmpty())
        .OnStateChanged_Lambda([Item, OnSelected](bool InIsSelected)
        {
            if (InIsSelected && Item.IsValid() && OnSelected.IsBound())
            { OnSelected.Execute(Item->Entity); }
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

auto SCkDebug_EntityHealthList::Set_Items(TArray<FCkDebug_EntityHealthItem> InItems) -> void
{
    auto Reconciled = TArray<TSharedPtr<FCkDebug_EntityHealthItem>>{};
    Reconciled.Reserve(InItems.Num());
    for (auto& Item : InItems)
    {
        const auto* Existing = _Items.FindByPredicate([&Item](const auto& Candidate)
        { return Candidate.IsValid() && Candidate->Entity == Item.Entity; });
        if (Existing != nullptr)
        {
            **Existing = MoveTemp(Item);
            Reconciled.Add(*Existing);
        }
        else
        { Reconciled.Add(MakeShared<FCkDebug_EntityHealthItem>(MoveTemp(Item))); }
    }
    _Items = MoveTemp(Reconciled);
    if (_List.IsValid()) { _List->RequestListRefresh(); }
}

auto SCkDebug_EntityHealthList::Clear_Items() -> void
{
    _Items.Reset();
    if (_List.IsValid()) { _List->RequestListRefresh(); }
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
