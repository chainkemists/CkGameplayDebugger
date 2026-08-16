#include "CkJoltDebugger/Window/SCkJoltDebugger_OutlinerPanel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger_outliner_panel
{
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
            case ECkJoltDebugger_Population::Constraint:  return TEXT("Joint");
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
            case ECkJoltDebugger_Population::Constraint:  return ECk_Tone::Warn;
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
            case ECkJoltDebugger_Population::Constraint:
                return InSnapshot.IsBodyBWorldAnchor
                    ? ck::Format_UE(TEXT("{} \x00B7 world"), InSnapshot.ConstraintType)
                    : ck::Format_UE(TEXT("{} \x00B7 {} bodies"), InSnapshot.ConstraintType, InSnapshot.NumBodies);
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

            // The health narrowing sits ABOVE the query boxes on purpose: it is a different KIND of filter —
            // one the facility computed, not one the user typed — and stacking it under the text bar would
            // read as a third search field.
            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    // Wrapped rather than tooltipped directly: SCkDebug_Chip does not apply the base
                    // ToolTipText argument, and Slate resolves a tooltip by walking UP the hovered path.
                    SNew(SBox)
                    .ToolTipText(FText::FromString(TEXT(
                        "Narrow to bodies the facility's health scan flagged: NaN transform or velocity, a "
                        "runaway linear speed, an AABB under the world's KillZ, or a shape with no extent. "
                        "The scan walks the ACTIVE bodies each capture, so a broken body that has since "
                        "fallen asleep is not re-flagged.")))
                    [
                        SNew(SCkDebug_Chip)
                        .Text_Lambda([this]() -> FText
                        {
                            const auto NumProblems = Get_NumProblemRows();

                            return NumProblems > 0
                                ? FText::FromString(ck::Format_UE(TEXT("Problems ({})"), NumProblems))
                                : FText::FromString(TEXT("Problems"));
                        })
                        .Kind_Lambda([this]() -> ECkDebug_ChipKind
                        {
                            if (_ProblemsOnly)
                            { return ECkDebug_ChipKind::Unsatisfied; }

                            return Get_NumProblemRows() > 0
                                ? ECkDebug_ChipKind::Effect
                                : ECkDebug_ChipKind::Neutral;
                        })
                        .Highlighted_Lambda([this]() { return _ProblemsOnly; })
                        .OnClicked_Lambda([this]() { Set_ProblemsFilter(NOT _ProblemsOnly); })
                    ]
                ]
            ]

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
                // Ctrl/Shift build a multi-selection (P7-D53): isolating or highlighting a GROUP of bodies is
                // the question this outliner exists to answer, and single-select can only ever ask it once.
                .SelectionMode(ESelectionMode::Multi)
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
    _SelectedIdentities.Reset();
    _Primary.Reset();

    if (_ListView.IsValid())
    {
        const auto Guard = TGuardValue<bool>{_IsApplyingSelection, true};

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
    using namespace ck_jolt_debugger_outliner_panel;

    // A selected row whose entity left the world is no longer a selection — dropping it here keeps the pin,
    // the highlight set and the copy menu from all describing a row that no longer exists.
    _SelectedIdentities.RemoveAll([this](const FRowIdentity& InIdentity)
    {
        return NOT _Bodies.ContainsByPredicate([&InIdentity](const FCkJoltDebugger_BodySnapshot& InBody)
        { return Get_RowIdentity(InBody) == InIdentity; });
    });

    if (_Primary.IsSet() && NOT _SelectedIdentities.Contains(*_Primary))
    {
        // The primary left the world while the rest of the selection did not. The last survivor takes over
        // rather than the whole set losing its primary — a multi-selection with none has nothing to show in
        // the detail panel and nothing to sample on the facility.
        _Primary = _SelectedIdentities.IsEmpty()
            ? TOptional<FRowIdentity>{}
            : TOptional<FRowIdentity>{_SelectedIdentities.Last()};
    }

    // SListView keys selection by pointer identity, so a wholesale rebuild every refresh would eat the
    // user's click before it resolves. Reuse the existing item for a row identity that is still present
    // and update its contents in place; only a change to the visible SET needs a list refresh.
    auto Existing = TMap<FRowIdentity, ItemPtr>{};
    Existing.Reserve(_ItemSource.Num());

    for (const auto& Item : _ItemSource)
    {
        if (Item.IsValid())
        { Existing.Add(Get_RowIdentity(*Item), Item); }
    }

    auto NewItems = TArray<ItemPtr>{};
    NewItems.Reserve(_Bodies.Num());
    auto SetChanged = false;

    for (const auto& Body : _Bodies)
    {
        const auto Identity = Get_RowIdentity(Body);

        // EVERY SELECTED row is listed whatever the filter says. A selection that vanishes because the user
        // narrowed the query afterwards is indistinguishable from no selection at all — and the detail panel
        // beside it would still be showing the primary's facts. It renders dimmed, like a highlight non-match.
        const auto IsPinnedSelection = _SelectedIdentities.Contains(Identity);

        // Both stages, and the pin overrides both: the Problems chip is a narrowing, not a different list.
        const auto SurvivesFilters = Matches_Query(Body, _FilterString) &&
            (NOT _ProblemsOnly || Body.Get_HasProblem());

        if (NOT IsPinnedSelection && NOT SurvivesFilters)
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

    // A row that left the visible set and came back is a NEW pointer, so the view lost the selection with it.
    // The MODEL still knows what is selected, so it is simply re-stamped — Direct, and under the apply guard,
    // so the restore cannot be mistaken for a user selecting anything.
    if (SetChanged)
    { DoPushSelectionToView(); }
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

/*
 * The delegate's item parameter is NOT the row the user just clicked — SListView hands back an arbitrary
 * element of its selection TSet. The primary is therefore derived from the set DELTA: whatever joined the
 * selection is what the user last acted on. The parameter is used only as a tie-break when more than one
 * row joined at once (a Shift-range), where the engine's own idea of the anchor is the better answer.
 */
auto
    SCkJoltDebugger_OutlinerPanel::
    OnSelectionChanged(
        ItemPtr InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    using namespace ck_jolt_debugger_outliner_panel;

    if (_IsApplyingSelection || NOT _ListView.IsValid())
    { return; }

    auto ViewSelected = TSet<FRowIdentity>{};

    for (const auto& Item : _ListView->GetSelectedItems())
    {
        if (Item.IsValid())
        { ViewSelected.Add(Get_RowIdentity(*Item)); }
    }

    // Re-derived in LIST order rather than in the view's set order, so the selection reads the way it looks.
    auto Ordered = TArray<FRowIdentity>{};
    Ordered.Reserve(ViewSelected.Num());

    for (const auto& Item : _ItemSource)
    {
        if (NOT Item.IsValid())
        { continue; }

        const auto Identity = Get_RowIdentity(*Item);

        if (ViewSelected.Contains(Identity))
        { Ordered.Emplace(Identity); }
    }

    auto Added = TArray<FRowIdentity>{};

    for (const auto& Identity : Ordered)
    {
        if (NOT _SelectedIdentities.Contains(Identity))
        { Added.Emplace(Identity); }
    }

    if (Added.Num() > 0)
    {
        const auto Hint = InItem.IsValid() ? TOptional<FRowIdentity>{Get_RowIdentity(*InItem)} : TOptional<FRowIdentity>{};

        _Primary = Hint.IsSet() && Added.Contains(*Hint) ? *Hint : Added.Last();
    }
    else if (NOT _Primary.IsSet() || NOT Ordered.Contains(*_Primary))
    {
        // The primary was deselected (a Ctrl+click that removed it): the last row still standing takes over,
        // because a multi-selection with no primary has nothing to show in the detail panel.
        _Primary = Ordered.IsEmpty() ? TOptional<FRowIdentity>{} : TOptional<FRowIdentity>{Ordered.Last()};
    }

    _SelectedIdentities = MoveTemp(Ordered);

    // Direct is the programmatic apply (external selection, selection restore) — echoing it back to the
    // window would re-broadcast a selection the window itself just pushed in.
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    DoBroadcastSelection();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    OnContextMenuOpening()
    -> TSharedPtr<SWidget>
{
    const auto Selected = Get_SelectedAll();

    if (Selected.IsEmpty())
    { return nullptr; }

    // Multi-select aware: the copy entries join every selected row with a newline rather than describing
    // only the primary (CkDebuggerCommon/CLAUDE.md §"Copy menus").
    auto RowLines = TArray<FString>{};
    auto EntityLines = TArray<FString>{};

    for (const auto& Body : Selected)
    {
        RowLines.Emplace(ck_jolt_debugger_outliner_panel::Make_CopyText(Body));
        EntityLines.Emplace(ck::Format_UE(TEXT("{}"), Body.Handle));
    }

    constexpr auto CloseAfterSelection = true;
    auto MenuBuilder = FMenuBuilder{CloseAfterSelection, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        NSLOCTEXT("CkJoltOutliner", "CopyRow", "Copy Row"),
        NSLOCTEXT("CkJoltOutliner", "CopyRowTip", "Copy the selected rows' text (name, population, state, body key) to the clipboard."),
        FString::Join(RowLines, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        NSLOCTEXT("CkJoltOutliner", "CopyEntity", "Copy Entity"),
        NSLOCTEXT("CkJoltOutliner", "CopyEntityTip", "Copy the selected rows' full entity handles (ID|Version + debug name) to the clipboard."),
        FString::Join(EntityLines, TEXT("\n")));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    DoPushSelectionToView()
    -> void
{
    if (NOT _ListView.IsValid())
    { return; }

    const auto Guard = TGuardValue<bool>{_IsApplyingSelection, true};

    _ListView->ClearSelection();

    for (const auto& Identity : _SelectedIdentities)
    {
        if (const auto Item = TryFind_ItemByIdentity(Identity); Item.IsValid())
        { _ListView->SetItemSelection(Item, true, ESelectInfo::Direct); }
    }
}

auto
    SCkJoltDebugger_OutlinerPanel::
    DoBroadcastSelection()
    -> void
{
    if (NOT _OnRowSelected.IsBound())
    { return; }

    _OnRowSelected.Execute(Get_Selection(), Get_SelectedAll());
}

auto
    SCkJoltDebugger_OutlinerPanel::
    DoSelectItem(
        ItemPtr InItem,
        bool    InIsAdditive)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    if (NOT InItem.IsValid() || NOT _ListView.IsValid())
    { return {}; }

    const auto Identity = ck_jolt_debugger_outliner_panel::Get_RowIdentity(*InItem);

    if (NOT InIsAdditive)
    { _SelectedIdentities.Reset(); }

    _SelectedIdentities.AddUnique(Identity);

    // The row the caller named is what the user last acted on, whether it joined the selection or was
    // already in it — a Ctrl+click on a selected row promotes it rather than doing nothing.
    _Primary = Identity;

    DoPushSelectionToView();
    _ListView->RequestScrollIntoView(InItem);

    return TOptional<FCkJoltDebugger_BodySnapshot>{*InItem};
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_IsRowDimmed(
        const FCkJoltDebugger_BodySnapshot& InBody) const
    -> bool
{
    // Three reasons a listed row can be off-query: it lost the highlight query, or it is a pinned selection
    // the text filter — or the Problems chip — would otherwise have hidden. All read the same: this row is
    // not what you asked for. The chip belongs here for exactly the reason the filter does, or a selected
    // healthy row would sit un-dimmed in a list the user narrowed to broken ones.
    return NOT ck_jolt_debugger_outliner_panel::Matches_Query(InBody, _HighlightString)
        || NOT ck_jolt_debugger_outliner_panel::Matches_Query(InBody, _FilterString)
        || (_ProblemsOnly && NOT InBody.Get_HasProblem());
}

auto
    SCkJoltDebugger_OutlinerPanel::
    TryFind_Item(
        const FCkJoltDebugger_BodySnapshot& InBody) const
    -> ItemPtr
{
    return TryFind_ItemByIdentity(ck_jolt_debugger_outliner_panel::Get_RowIdentity(InBody));
}

auto
    SCkJoltDebugger_OutlinerPanel::
    TryFind_ItemByIdentity(
        const FRowIdentity& InIdentity) const
    -> ItemPtr
{
    for (const auto& Item : _ItemSource)
    {
        if (Item.IsValid() && ck_jolt_debugger_outliner_panel::Get_RowIdentity(*Item) == InIdentity)
        { return Item; }
    }

    return {};
}

auto
    SCkJoltDebugger_OutlinerPanel::
    DoSelectMatching(
        TFunctionRef<bool(const FCkJoltDebugger_BodySnapshot&)> InPredicate,
        bool InIsAdditive)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    const auto* Match = _Bodies.FindByPredicate(InPredicate);

    if (Match == nullptr)
    { return {}; }

    if (const auto Visible = TryFind_Item(*Match); Visible.IsValid())
    { return DoSelectItem(Visible, InIsAdditive); }

    // The match survives in the collected set but the filter hides it. Reveal it: a selection the user cannot
    // see is indistinguishable from no selection at all.
    Set_FilterQuery(FString{});

    return DoSelectItem(TryFind_Item(*Match), InIsAdditive);
}

auto
    SCkJoltDebugger_OutlinerPanel::
    SelectByHandle(
        const FCk_Handle& InHandle)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    return DoSelectMatching([&InHandle](const FCkJoltDebugger_BodySnapshot& InBody)
    { return InBody.Handle == InHandle; }, false);
}

auto
    SCkJoltDebugger_OutlinerPanel::
    SelectByEntity(
        FCk_Entity InEntity)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    return DoSelectMatching([InEntity](const FCkJoltDebugger_BodySnapshot& InBody)
    { return InBody.Handle.Get_Entity() == InEntity; }, false);
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Add_ToSelection(
        const FCk_Handle& InHandle)
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    return DoSelectMatching([&InHandle](const FCkJoltDebugger_BodySnapshot& InBody)
    { return InBody.Handle == InHandle; }, true);
}

auto
    SCkJoltDebugger_OutlinerPanel::
    ClearSelection()
    -> void
{
    _SelectedIdentities.Reset();
    _Primary.Reset();

    if (_ListView.IsValid())
    {
        const auto Guard = TGuardValue<bool>{_IsApplyingSelection, true};
        _ListView->ClearSelection();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_SnapshotsForSelection() const
    -> TArray<FCkJoltDebugger_BodySnapshot>
{
    using namespace ck_jolt_debugger_outliner_panel;

    auto Result = TArray<FCkJoltDebugger_BodySnapshot>{};
    Result.Reserve(_SelectedIdentities.Num());

    const auto Append = [this, &Result](const FRowIdentity& InIdentity)
    {
        const auto* Body = _Bodies.FindByPredicate([&InIdentity](const FCkJoltDebugger_BodySnapshot& InCandidate)
        { return Get_RowIdentity(InCandidate) == InIdentity; });

        if (Body != nullptr)
        { Result.Emplace(*Body); }
    };

    // Primary FIRST: the facility samples and asks for the contacts of the first highlighted key alone.
    if (_Primary.IsSet())
    { Append(*_Primary); }

    for (const auto& Identity : _SelectedIdentities)
    {
        if (_Primary.IsSet() && Identity == *_Primary)
        { continue; }

        Append(Identity);
    }

    return Result;
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_Selection() const
    -> TOptional<FCkJoltDebugger_BodySnapshot>
{
    using namespace ck_jolt_debugger_outliner_panel;

    if (NOT _Primary.IsSet())
    { return {}; }

    const auto* Body = _Bodies.FindByPredicate([this](const FCkJoltDebugger_BodySnapshot& InCandidate)
    { return Get_RowIdentity(InCandidate) == *_Primary; });

    return Body != nullptr ? TOptional<FCkJoltDebugger_BodySnapshot>{*Body} : TOptional<FCkJoltDebugger_BodySnapshot>{};
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_SelectedAll() const
    -> TArray<FCkJoltDebugger_BodySnapshot>
{
    return Get_SnapshotsForSelection();
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_NumSelectedRows() const
    -> int32
{
    return _SelectedIdentities.Num();
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

auto
    SCkJoltDebugger_OutlinerPanel::
    Set_ProblemsFilter(
        bool InIsActive)
    -> void
{
    if (_ProblemsOnly == InIsActive)
    { return; }

    _ProblemsOnly = InIsActive;
    ApplyFilterPipeline();
}

auto
    SCkJoltDebugger_OutlinerPanel::
    Get_NumProblemRows() const
    -> int32
{
    auto Count = 0;

    for (const auto& Body : _Bodies)
    {
        if (Body.Get_HasProblem())
        { ++Count; }
    }

    return Count;
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
