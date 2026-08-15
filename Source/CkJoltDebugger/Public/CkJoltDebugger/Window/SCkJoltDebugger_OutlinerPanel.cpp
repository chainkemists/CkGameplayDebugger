#include "CkJoltDebugger/Window/SCkJoltDebugger_OutlinerPanel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger_outliner_panel
{
    /*
     * What makes two rows the same row across refreshes. An entity can back more than one population at once
     * (a JoltBody entity that also owns a Probe), so the handle alone would collapse those rows into one and
     * churn the list every pass.
     */
    struct FRowIdentity
    {
        FCk_Handle _Handle;
        ECkJoltDebugger_Population _Population = ECkJoltDebugger_Population::JoltBody;

        auto operator==(const FRowIdentity& InOther) const -> bool
        {
            return _Handle == InOther._Handle && _Population == InOther._Population;
        }

        friend auto GetTypeHash(const FRowIdentity& InIdentity) -> uint32
        {
            return HashCombine(GetTypeHash(InIdentity._Handle), GetTypeHash(static_cast<uint8>(InIdentity._Population)));
        }
    };

    auto Get_RowIdentity(
        const FCkJoltDebugger_BodySnapshot& InSnapshot) -> FRowIdentity
    {
        return FRowIdentity{InSnapshot.Handle, InSnapshot.Population};
    }

    auto Font_RowName() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); }

    auto Font_RowDetail() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); }

    auto Get_RowPadding() -> FMargin
    { return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceS, 1.0f}); }

    auto Get_PopulationLabel(
        ECkJoltDebugger_Population InPopulation) -> FString
    {
        switch (InPopulation)
        {
            case ECkJoltDebugger_Population::JoltBody:    return TEXT("Body");
            case ECkJoltDebugger_Population::BakedStatic: return TEXT("Baked");
            case ECkJoltDebugger_Population::Sensor:      return TEXT("Sensor");
            case ECkJoltDebugger_Population::Character:   return TEXT("Char");
            default:                                      return TEXT("Unknown");
        }
    }

    auto Get_PopulationTone(
        ECkJoltDebugger_Population InPopulation) -> ECk_Tone
    {
        switch (InPopulation)
        {
            case ECkJoltDebugger_Population::JoltBody:    return ECk_Tone::Accent;
            case ECkJoltDebugger_Population::BakedStatic: return ECk_Tone::Neutral;
            case ECkJoltDebugger_Population::Sensor:      return ECk_Tone::Info;
            case ECkJoltDebugger_Population::Character:   return ECk_Tone::Ok;
            default:                                      return ECk_Tone::Neutral;
        }
    }

    auto Get_MotionLabel(
        ECk_MotionType InMotionType) -> FString
    {
        switch (InMotionType)
        {
            case ECk_MotionType::Kinematic: return TEXT("Kinematic");
            case ECk_MotionType::Dynamic:   return TEXT("Dynamic");
            default:                        return TEXT("Static");
        }
    }

    auto Get_SleepLabel(
        ECk_Jolt_SleepState InSleepState) -> FString
    {
        return InSleepState == ECk_Jolt_SleepState::Asleep ? TEXT("Asleep") : TEXT("Awake");
    }

    auto Get_RowDetailText(
        const FCkJoltDebugger_BodySnapshot& InSnapshot) -> FString
    {
        switch (InSnapshot.Population)
        {
            case ECkJoltDebugger_Population::JoltBody:
                return ck::Format_UE(TEXT("{} \x00B7 {}"),
                    Get_MotionLabel(InSnapshot.MotionType),
                    Get_SleepLabel(InSnapshot.SleepState));
            case ECkJoltDebugger_Population::BakedStatic:
                return ck::Format_UE(TEXT("{} bodies"), InSnapshot.NumBodies);
            case ECkJoltDebugger_Population::Sensor:
                return TEXT("sensor");
            case ECkJoltDebugger_Population::Character:
                return TEXT("capsule");
            default:
                return {};
        }
    }

    auto Get_BodyKeyText(
        const FCkJoltDebugger_BodySnapshot& InSnapshot) -> FString
    {
        return InSnapshot.BodyKey.IsSet() ? ck::Format_UE(TEXT("{}"), *InSnapshot.BodyKey) : FString{TEXT("--")};
    }

    // Case-insensitive substring match over exactly what the row renders, plus the body key — the one
    // identifier a user reads off the facility's own logs. An empty needle matches everything.
    auto Matches_Query(
        const FCkJoltDebugger_BodySnapshot& InSnapshot,
        const FString& InNeedle) -> bool
    {
        if (InNeedle.IsEmpty())
        { return true; }

        if (InSnapshot.DisplayName.Contains(InNeedle, ESearchCase::IgnoreCase))
        { return true; }

        if (Get_PopulationLabel(InSnapshot.Population).Contains(InNeedle, ESearchCase::IgnoreCase))
        { return true; }

        if (Get_RowDetailText(InSnapshot).Contains(InNeedle, ESearchCase::IgnoreCase))
        { return true; }

        return Get_BodyKeyText(InSnapshot).Contains(InNeedle, ESearchCase::IgnoreCase);
    }

    auto Make_CopyText(
        const FCkJoltDebugger_BodySnapshot& InSnapshot) -> FString
    {
        return ck::Format_UE(TEXT("{} [{}] {} (BodyKey {})"),
            InSnapshot.DisplayName,
            Get_PopulationLabel(InSnapshot.Population),
            Get_RowDetailText(InSnapshot),
            Get_BodyKeyText(InSnapshot));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _OnRowSelected = InArgs._OnRowSelected;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(FSlateColor{CkStyle::Bg1()})
        .Padding(FMargin{0.0f})
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SAssignNew(_SearchBar, SCkDebug_DualSearchBar)
                .FilterHintText(FText::FromString(TEXT("Filter bodies\x2026")))
                .HighlightHintText(FText::FromString(TEXT("Highlight\x2026")))
                .OnFilterTextChanged_Lambda([this](const FString& InText)
                {
                    if (_FilterString == InText)
                    { return; }

                    _FilterString = InText;
                    ApplyFilterPipeline();
                })
                .OnHighlightTextChanged_Lambda([this](const FString& InText)
                {
                    if (_HighlightString == InText)
                    { return; }

                    _HighlightString = InText;
                    ApplyFilterPipeline();
                })
            ]

            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SAssignNew(_ListView, SListView<ItemPtr>)
                .ListItemsSource(&_ItemSource)
                .OnGenerateRow(this, &SCkJoltDebugger_OutlinerPanel::OnGenerateRow)
                .OnSelectionChanged(this, &SCkJoltDebugger_OutlinerPanel::OnSelectionChanged)
                .OnContextMenuOpening(this, &SCkJoltDebugger_OutlinerPanel::OnContextMenuOpening)
                .SelectionMode(ESelectionMode::Single)
            ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    Refresh(
        const TArray<FCkJoltDebugger_BodySnapshot>& InBodies)
    -> void
{
    _Bodies = InBodies;
    ApplyFilterPipeline();
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Clear()
    -> void
{
    _Bodies.Reset();
    _ItemSource.Reset();

    if (_ListView.IsValid())
    {
        _ListView->ClearSelection();
        _ListView->RequestListRefresh();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    ApplyFilterPipeline()
    -> void
{
    // SListView keys selection by pointer identity, so a wholesale rebuild every refresh would eat the
    // user's click before it resolves. Reuse the existing item for a row identity that is still present
    // and update its contents in place; only a change to the visible SET needs a list refresh.
    auto SelectedItem = ItemPtr{};

    if (_ListView.IsValid())
    {
        const auto Selection = _ListView->GetSelectedItems();

        if (Selection.Num() == 1)
        { SelectedItem = Selection[0]; }
    }

    auto SelectedIdentity = TOptional<ck_jolt_debugger_outliner_panel::FRowIdentity>{};

    if (SelectedItem.IsValid())
    { SelectedIdentity = ck_jolt_debugger_outliner_panel::Get_RowIdentity(*SelectedItem); }

    auto Existing = TMap<ck_jolt_debugger_outliner_panel::FRowIdentity, ItemPtr>{};
    Existing.Reserve(_ItemSource.Num());

    for (const auto& Item : _ItemSource)
    {
        if (Item.IsValid())
        { Existing.Add(ck_jolt_debugger_outliner_panel::Get_RowIdentity(*Item), Item); }
    }

    auto NewItems = TArray<ItemPtr>{};
    NewItems.Reserve(_Bodies.Num());
    auto SetChanged = false;

    for (const auto& Body : _Bodies)
    {
        const auto Identity = ck_jolt_debugger_outliner_panel::Get_RowIdentity(Body);

        // The SELECTED row is listed whatever the filter says. A selection that vanishes because the user
        // narrowed the query afterwards is indistinguishable from no selection at all — and the detail panel
        // beside it would still be showing the row's facts. It renders dimmed, the way a highlight non-match does.
        const auto IsPinnedSelection = SelectedIdentity.IsSet() && *SelectedIdentity == Identity;

        if (NOT IsPinnedSelection && NOT ck_jolt_debugger_outliner_panel::Matches_Query(Body, _FilterString))
        { continue; }

        auto Item = ItemPtr{};

        if (auto* Found = Existing.Find(Identity))
        {
            Item = *Found;
            *Item = Body;
            Existing.Remove(Identity);
        }
        else
        {
            Item = MakeShared<FCkJoltDebugger_BodySnapshot>(Body);
            SetChanged = true;
        }

        NewItems.Emplace(MoveTemp(Item));
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    // Population first, then name. Stable, so rows the collector emitted in registry order for one population
    // keep that order instead of shuffling between refreshes for no reason the user can see.
    NewItems.StableSort([](const ItemPtr& InLeft, const ItemPtr& InRight) -> bool
    {
        if (NOT InLeft.IsValid() || NOT InRight.IsValid())
        { return InLeft.IsValid(); }

        if (InLeft->Population != InRight->Population)
        { return static_cast<uint8>(InLeft->Population) < static_cast<uint8>(InRight->Population); }

        return InLeft->DisplayName.Compare(InRight->DisplayName, ESearchCase::IgnoreCase) < 0;
    });

    // The set can be identical while the ORDER is not — a renamed entity moves within its population. The view
    // renders from its own cached row order, so an unrefreshed reorder shows rows in the wrong places.
    auto OrderChanged = NewItems.Num() != _ItemSource.Num();

    for (auto Index = 0; NOT OrderChanged && Index < NewItems.Num(); ++Index)
    {
        if (NewItems[Index] != _ItemSource[Index])
        { OrderChanged = true; }
    }

    _ItemSource = MoveTemp(NewItems);

    if (NOT _ListView.IsValid() || (NOT SetChanged && NOT OrderChanged))
    { return; }

    _ListView->RequestListRefresh();

    // A row that left the visible set and came back is a NEW pointer, so the view lost the selection with
    // it. Re-stamp it Direct — and only when it is genuinely missing, or the compare-free path would fire
    // a spurious selection change every refresh.
    const auto SelectionSurvived = SelectedItem.IsValid() && _ItemSource.Contains(SelectedItem);

    if (NOT SelectedItem.IsValid() || SelectionSurvived)
    { return; }

    if (const auto Restored = TryFind_Item(*SelectedItem); Restored.IsValid())
    { _ListView->SetSelection(Restored, ESelectInfo::Direct); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    OnGenerateRow(
        ItemPtr InItem,
        const TSharedRef<STableViewBase>& InTable)
    -> TSharedRef<ITableRow>
{
    if (NOT InItem.IsValid())
    {
        return SNew(STableRow<ItemPtr>, InTable)
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("(invalid)")))
        ];
    }

    const auto WeakItem  = TWeakPtr<FCkJoltDebugger_BodySnapshot>{InItem};
    const auto WeakPanel = TWeakPtr<SCkJoltDebugger_OutlinerPanel>{SharedThis(this)};

    return SNew(STableRow<ItemPtr>, InTable)
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_jolt_debugger_outliner_panel::Get_RowPadding))
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_StatusPill)
                .ShowDot(false)
                .Text_Lambda([WeakItem]() -> FText
                {
                    const auto Pinned = WeakItem.Pin();
                    return Pinned.IsValid()
                        ? FText::FromString(ck_jolt_debugger_outliner_panel::Get_PopulationLabel(Pinned->Population))
                        : FText::GetEmpty();
                })
                .Tone_Lambda([WeakItem]() -> ECk_Tone
                {
                    const auto Pinned = WeakItem.Pin();
                    return Pinned.IsValid()
                        ? ck_jolt_debugger_outliner_panel::Get_PopulationTone(Pinned->Population)
                        : ECk_Tone::Neutral;
                })
            ]

            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font_Static(&ck_jolt_debugger_outliner_panel::Font_RowName)
                .Text_Lambda([WeakItem]() -> FText
                {
                    const auto Pinned = WeakItem.Pin();
                    return Pinned.IsValid() ? FText::FromString(Pinned->DisplayName) : FText::GetEmpty();
                })
                .ColorAndOpacity_Lambda([WeakPanel, WeakItem]() -> FSlateColor
                {
                    const auto Panel = WeakPanel.Pin();
                    const auto Item  = WeakItem.Pin();

                    if (NOT Panel.IsValid() || NOT Item.IsValid())
                    { return FSlateColor::UseForeground(); }

                    return Panel->Get_IsRowDimmed(*Item)
                        ? FSlateColor{CkStyle::TextMute()}
                        : FSlateColor::UseForeground();
                })
            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Font_Static(&ck_jolt_debugger_outliner_panel::Font_RowDetail)
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                .Text_Lambda([WeakItem]() -> FText
                {
                    const auto Pinned = WeakItem.Pin();
                    return Pinned.IsValid()
                        ? FText::FromString(ck_jolt_debugger_outliner_panel::Get_RowDetailText(*Pinned))
                        : FText::GetEmpty();
                })
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    OnSelectionChanged(
        ItemPtr InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    // Direct is the programmatic apply (external selection, selection restore) — echoing it back to the
    // window would re-broadcast a selection the window itself just pushed in.
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT _OnRowSelected.IsBound())
    { return; }

    _OnRowSelected.Execute(InItem.IsValid()
        ? TOptional<FCkJoltDebugger_BodySnapshot>{*InItem}
        : TOptional<FCkJoltDebugger_BodySnapshot>{});
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    OnContextMenuOpening()
    -> TSharedPtr<SWidget>
{
    if (NOT _ListView.IsValid())
    { return nullptr; }

    const auto Selected = _ListView->GetSelectedItems();

    if (Selected.IsEmpty() || NOT Selected[0].IsValid())
    { return nullptr; }

    const auto& Item = *Selected[0];

    constexpr auto CloseAfterSelection = true;
    auto MenuBuilder = FMenuBuilder{CloseAfterSelection, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        NSLOCTEXT("CkJoltOutliner", "CopyRow", "Copy Row"),
        NSLOCTEXT("CkJoltOutliner", "CopyRowTip", "Copy the row text (name, population, state, body key) to the clipboard."),
        ck_jolt_debugger_outliner_panel::Make_CopyText(Item));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        NSLOCTEXT("CkJoltOutliner", "CopyEntity", "Copy Entity"),
        NSLOCTEXT("CkJoltOutliner", "CopyEntityTip", "Copy the full entity handle (ID|Version + debug name) to the clipboard."),
        ck::Format_UE(TEXT("{}"), Item.Handle));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    DoSelectItem(
        ItemPtr InItem)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    if (NOT InItem.IsValid() || NOT _ListView.IsValid())
    { return {}; }

    _ListView->SetSelection(InItem, ESelectInfo::Direct);
    _ListView->RequestScrollIntoView(InItem);

    return TOptional<FCkJoltDebugger_BodySnapshot>{*InItem};
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_IsRowDimmed(
        const FCkJoltDebugger_BodySnapshot& InBody) const
    -> bool
{
    // Two reasons a listed row can be off-query: it lost the highlight query, or it is the pinned selection
    // the filter would otherwise have hidden. Both read the same — this row is not what you asked for.
    return NOT ck_jolt_debugger_outliner_panel::Matches_Query(InBody, _HighlightString)
        || NOT ck_jolt_debugger_outliner_panel::Matches_Query(InBody, _FilterString);
}

auto
    SCkJoltDebugger_OutlinerPanel::
    TryFind_Item(
        const FCkJoltDebugger_BodySnapshot& InBody) const
    -> ItemPtr
{
    const auto Identity = ck_jolt_debugger_outliner_panel::Get_RowIdentity(InBody);

    for (const auto& Item : _ItemSource)
    {
        if (Item.IsValid() && ck_jolt_debugger_outliner_panel::Get_RowIdentity(*Item) == Identity)
        { return Item; }
    }

    return {};
}

auto
    SCkJoltDebugger_OutlinerPanel::
    DoSelectMatching(
        TFunctionRef<bool(const FCkJoltDebugger_BodySnapshot&)> InPredicate)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    const auto* Match = _Bodies.FindByPredicate(InPredicate);

    if (Match == nullptr)
    { return {}; }

    if (const auto Visible = TryFind_Item(*Match); Visible.IsValid())
    { return DoSelectItem(Visible); }

    // The match survives in the collected set but the filter hides it. Reveal it: a selection the user cannot
    // see is indistinguishable from no selection at all.
    Set_FilterQuery(FString{});

    return DoSelectItem(TryFind_Item(*Match));
}

auto
    SCkJoltDebugger_OutlinerPanel::
    SelectByHandle(
        const FCk_Handle& InHandle)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    return DoSelectMatching([&InHandle](const FCkJoltDebugger_BodySnapshot& InBody)
    { return InBody.Handle == InHandle; });
}

auto
    SCkJoltDebugger_OutlinerPanel::
    SelectByEntity(
        FCk_Entity InEntity)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    return DoSelectMatching([InEntity](const FCkJoltDebugger_BodySnapshot& InBody)
    { return InBody.Handle.Get_Entity() == InEntity; });
}

auto
    SCkJoltDebugger_OutlinerPanel::
    ClearSelection()
    -> void
{
    if (_ListView.IsValid())
    { _ListView->ClearSelection(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_Selection() const
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    if (NOT _ListView.IsValid())
    { return {}; }

    const auto Selected = _ListView->GetSelectedItems();

    if (Selected.Num() != 1 || NOT Selected[0].IsValid())
    { return {}; }

    return TOptional<FCkJoltDebugger_BodySnapshot>{*Selected[0]};
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_NumVisibleRows() const
    -> int32
{
    return _ItemSource.Num();
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Set_FilterQuery(
        const FString& InQuery)
    -> void
{
    if (_SearchBar.IsValid())
    { _SearchBar->Set_FilterText(InQuery); }

    if (_FilterString == InQuery)
    { return; }

    _FilterString = InQuery;
    ApplyFilterPipeline();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    Rebuild_ForStyleChange()
    -> void
{
    if (_ListView.IsValid())
    { _ListView->RebuildList(); }
}

// --------------------------------------------------------------------------------------------------------------------
