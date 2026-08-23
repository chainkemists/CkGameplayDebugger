#include "SCkDebug_EvidenceList.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

// ====================================================================================================================

namespace ck::debug_evidence_list
{
    auto Try_Reconcile(
        const TArray<FItemPtr>& InExisting,
        TArray<FCkDebug_EvidenceItem> InIncoming,
        int32 InMaxItems,
        TArray<FItemPtr>& OutItems,
        FString& OutError) -> bool
    {
        auto Keys = TSet<FString>{};
        for (const auto& Item : InIncoming)
        {
            if (Item.Key.IsEmpty())
            {
                OutError = TEXT("Evidence item Key must not be empty.");
                return false;
            }
            if (Keys.Contains(Item.Key))
            {
                OutError = FString::Printf(TEXT("Evidence item Key is duplicated: %s"), *Item.Key);
                return false;
            }
            Keys.Add(Item.Key);
        }

        const auto MaxItems = FMath::Max(1, InMaxItems);
        const auto StartIndex = FMath::Max(0, InIncoming.Num() - MaxItems);

        auto ExistingByKey = TMap<FString, FItemPtr>{};
        for (const auto& Item : InExisting)
        {
            if (Item.IsValid() && !Item->Key.IsEmpty())
            { ExistingByKey.FindOrAdd(Item->Key) = Item; }
        }

        auto Reconciled = TArray<FItemPtr>{};
        Reconciled.Reserve(InIncoming.Num() - StartIndex);
        for (auto Index = StartIndex; Index < InIncoming.Num(); ++Index)
        {
            auto& Incoming = InIncoming[Index];
            if (const auto* Existing = ExistingByKey.Find(Incoming.Key))
            {
                **Existing = MoveTemp(Incoming);
                Reconciled.Add(*Existing);
            }
            else
            { Reconciled.Add(MakeShared<FCkDebug_EvidenceItem>(MoveTemp(Incoming))); }
        }

        OutError.Reset();
        OutItems = MoveTemp(Reconciled);
        return true;
    }
}

// ====================================================================================================================

auto SCkDebug_EvidenceList::Construct(const FArguments& InArgs) -> void
{
    _MaxItems = FMath::Max(1, InArgs._MaxItems);
    _EmptyText = InArgs._EmptyText;
    _OnSelectionChanged = InArgs._OnSelectionChanged;

    ChildSlot
    [
        SNew(SOverlay)

        + SOverlay::Slot()
        [
            SAssignNew(_List, SListView<FItemPtr>)
            .ListItemsSource(&_Items)
            .SelectionMode(ESelectionMode::Multi)
            .OnGenerateRow(this, &SCkDebug_EvidenceList::GenerateRow)
            .OnSelectionChanged(this, &SCkDebug_EvidenceList::HandleSelectionChanged)
            .OnContextMenuOpening(this, &SCkDebug_EvidenceList::HandleContextMenuOpening)
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(_EmptyText)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .Visibility(this, &SCkDebug_EvidenceList::GetEmptyVisibility)
        ]
    ];
}

auto SCkDebug_EvidenceList::Set_Items(TArray<FCkDebug_EvidenceItem> InItems) -> bool
{
    auto Reconciled = TArray<FItemPtr>{};
    auto Error = FString{};
    if (!ck::debug_evidence_list::Try_Reconcile(_Items, MoveTemp(InItems), _MaxItems, Reconciled, Error))
    { return false; }

    _Items = MoveTemp(Reconciled);
    if (_List.IsValid()) { _List->RequestListRefresh(); }
    return true;
}

auto SCkDebug_EvidenceList::Clear_Items() -> void
{
    _Items.Reset();
    if (_List.IsValid()) { _List->RequestListRefresh(); }
}

auto SCkDebug_EvidenceList::GenerateRow(
    FItemPtr InItem,
    const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    const auto Tone = InItem.IsValid() ? InItem->Tone : ECk_Tone::Neutral;
    const auto IndentLevel = InItem.IsValid() ? FMath::Max(0, InItem->IndentLevel) : 0;
    const auto& Selection = UCkDebuggerStyleSettings::Get_Selection();

    return SNew(STableRow<FItemPtr>, InOwnerTable)
        .Padding(FMargin{CkStyle::SpaceS, 3.0f})
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Fill)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(3.0f)
                [
                    SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor{CkStyle::GetToneColor(Tone)})
                    .Padding(0.0f)
                ]
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(static_cast<float>(IndentLevel) * CkStyle::SpaceL)
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            ck::debug_axes::Make_Chip(
                                Selection,
                                InItem.IsValid() ? InItem->Source : FText::GetEmpty(),
                                Tone)
                        ]

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(InItem.IsValid() ? InItem->Headline : FText::GetEmpty())
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor{CkStyle::TextStrong()})
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                            .Text(InItem.IsValid() ? InItem->RightLabel : FText::GetEmpty())
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                            .Visibility_Lambda([InItem]()
                            {
                                return InItem.IsValid() && !InItem->RightLabel.IsEmpty()
                                    ? EVisibility::SelfHitTestInvisible
                                    : EVisibility::Collapsed;
                            })
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(InItem.IsValid() ? InItem->Detail : FText::GetEmpty())
                        .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                        .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                        .AutoWrapText(true)
                    ]
                ]
            ]
        ];
}

auto SCkDebug_EvidenceList::HandleSelectionChanged(FItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void
{
    if (InSelectInfo == ESelectInfo::Direct || !InItem.IsValid() || InItem->SelectionId == INDEX_NONE)
    { return; }
    _OnSelectionChanged.ExecuteIfBound(InItem->SelectionId);
}

auto SCkDebug_EvidenceList::HandleContextMenuOpening() -> TSharedPtr<SWidget>
{
    if (!_List.IsValid()) { return nullptr; }

    auto Lines = TArray<FString>{};
    for (const auto& Item : _List->GetSelectedItems())
    {
        if (Item.IsValid() && !Item->CopyText.IsEmpty())
        { Lines.Add(Item->CopyText); }
    }
    if (Lines.IsEmpty()) { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};
    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy Evidence")),
        FText::FromString(TEXT("Copy the selected evidence line(s) to the clipboard")),
        FString::Join(Lines, TEXT("\n")));
    return MenuBuilder.MakeWidget();
}

auto SCkDebug_EvidenceList::GetEmptyVisibility() const -> EVisibility
{
    return _Items.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

// ====================================================================================================================
