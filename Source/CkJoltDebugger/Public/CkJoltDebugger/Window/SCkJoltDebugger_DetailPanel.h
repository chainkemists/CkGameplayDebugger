#pragma once

#include "CkJoltDebugger/Data/CkJoltDebugger_Types.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

/*
 * By CONST REFERENCE, not by value: every row's value attribute pulls the selection on every paint, and a
 * by-value delegate copied the whole snapshot — handle, two strings and a vector — once per row per frame.
 * The referent is the window's own selection member, which outlives every row it feeds.
 */
DECLARE_DELEGATE_RetVal(const TOptional<FCkJoltDebugger_BodySnapshot>&, FOnCkJoltDebugger_GetSelection);

/*
 * The facility half of the same read, on the same by-reference terms: the sample rows pull it every paint,
 * and it carries a contacts array that must never be copied per row per frame.
 */
DECLARE_DELEGATE_RetVal(const FCkJoltDebugger_SelectionFacts&, FOnCkJoltDebugger_GetSelectionFacts);

/** A contacts-list row click. The key names the OTHER body — the one the selection is touching. */
DECLARE_DELEGATE_OneParam(FOnCkJoltDebugger_ContactSelected, uint64);

// --------------------------------------------------------------------------------------------------------------------

/*
 * One row of the contacts list. A plain value type with the body key as its identity, so the list can keep
 * TSharedPtr identity across refreshes the way the outliner does (CkDebuggerCommon/CLAUDE.md §"List / tree
 * rows") — a contact set that churns its pointers every capture would eat the user's click.
 */
struct FCkJoltDebugger_ContactRow
{
    uint64 OtherBodyKey     = 0;
    int32  NumContactPoints = 0;
    float  PenetrationDepth = 0.0f;
};

// --------------------------------------------------------------------------------------------------------------------

/*
 * The selected body's facts, as key/value rows. Built ONCE — every value is a TAttribute lambda pulling the
 * window's current selection (and its facility sample) through the delegates, so a live selection change or a
 * live velocity lands without a Slate rebuild. Rows that do not apply to the selected population read "--"
 * rather than disappearing, so the panel's height never jumps as the selection moves between populations.
 *
 * The one exception to "never disappears" is the CHARACTER group, which is collapsed outright for a rigid
 * body: its six rows would otherwise be six permanent "--"s on the population the window shows most.
 */
class SCkJoltDebugger_DetailPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkJoltDebugger_DetailPanel) {}
        SLATE_EVENT(FOnCkJoltDebugger_GetSelection, GetSelection)
        SLATE_EVENT(FOnCkJoltDebugger_GetSelectionFacts, GetSelectionFacts)
        SLATE_EVENT(FOnCkJoltDebugger_ContactSelected, OnContactSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** What the named row renders right now — the row's own bound value, evaluated. Empty for an unknown key. */
    auto Get_RowValueText(const FString& InKey) const -> FText;

    /** The live value color the named row paints. Unknown and unset rows are intentionally muted. */
    auto Get_RowValueColor(const FString& InKey) const -> FLinearColor;

    /** The live semantic tone of a status-pill row. Neutral for an unknown or non-status row. */
    auto Get_RowStatusTone(const FString& InKey) const -> ECk_Tone;

    /** Whether the character group is currently rendering. False for every rigid-body selection. */
    auto Get_IsCharacterGroupVisible() const -> bool;

    /** Live semantic surfaces for the identity summary; useful to focused automation without walking Slate. */
    auto Get_PopulationStatusText() const -> FText;
    auto Get_PopulationStatusTone() const -> ECk_Tone;
    auto Get_SimulationStatusText() const -> FText;
    auto Get_SimulationStatusTone() const -> ECk_Tone;

    /*
     * Reconcile the contacts list against the facts the window just refreshed. PUSHED rather than bound: an
     * SListView renders from its own item source, and pointer identity has to survive a refresh or the row
     * the user is clicking dies under the click.
     */
    auto Refresh_Contacts() -> void;

    auto Get_NumContactRows() const -> int32;

private:
    using ContactItemPtr = TSharedPtr<FCkJoltDebugger_ContactRow>;

    auto Get_Selection() const -> const TOptional<FCkJoltDebugger_BodySnapshot>&;
    auto Get_SelectionFacts() const -> const FCkJoltDebugger_SelectionFacts&;

    auto MakeRow(
        const FString& InKey,
        TAttribute<FText> InValue,
        TAttribute<FLinearColor> InSetValueColor = {}) -> TSharedRef<SWidget>;

    /** A row whose value comes from the facility's rigid-body sample, degrading to "--" while it is unset. */
    auto MakeSampleRow(
        const FString& InKey,
        TFunction<FText(const FCk_Jolt_DebugDraw_BodySample&)> InRead,
        TAttribute<FLinearColor> InSetValueColor = {}) -> TSharedRef<SWidget>;

    /** A sample-backed state rendered as a status pill while retaining the same keyed value read surface. */
    auto MakeSampleStatusRow(
        const FString& InKey,
        TFunction<FText(const FCk_Jolt_DebugDraw_BodySample&)> InText,
        TFunction<ECk_Tone(const FCk_Jolt_DebugDraw_BodySample&)> InTone) -> TSharedRef<SWidget>;

    /** The character twin of MakeSampleRow. */
    auto MakeCharacterRow(
        const FString& InKey,
        TFunction<FText(const FCk_Jolt_DebugDraw_CharacterSample&)> InRead,
        TAttribute<FLinearColor> InSetValueColor = {}) -> TSharedRef<SWidget>;

    /** The character counterpart of MakeSampleStatusRow. */
    auto MakeCharacterStatusRow(
        const FString& InKey,
        TFunction<FText(const FCk_Jolt_DebugDraw_CharacterSample&)> InText,
        TFunction<ECk_Tone(const FCk_Jolt_DebugDraw_CharacterSample&)> InTone) -> TSharedRef<SWidget>;

    auto MakeSection(const FString& InLabel) -> TSharedRef<SWidget>;

    auto MakeEntityRow() -> TSharedRef<SWidget>;
    auto MakeIdentitySummary() -> TSharedRef<SWidget>;

    auto MakeContactsList() -> TSharedRef<SWidget>;

    auto OnGenerateContactRow(ContactItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;
    auto OnContactSelectionChanged(ContactItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;

    FOnCkJoltDebugger_GetSelection      _GetSelection;
    FOnCkJoltDebugger_GetSelectionFacts _GetSelectionFacts;
    FOnCkJoltDebugger_ContactSelected   _OnContactSelected;

    // The bound value of every key/value row, by key: one lookup surface for the row values, so what a caller
    // reads back is the same attribute the widget paints rather than a re-derivation of it.
    TMap<FString, TAttribute<FText>> _RowValues;
    TMap<FString, TAttribute<FLinearColor>> _RowValueColors;
    TMap<FString, TAttribute<ECk_Tone>> _RowStatusTones;

    TSharedPtr<SListView<ContactItemPtr>> _ContactList;
    TArray<ContactItemPtr>                _ContactItems;
};

// --------------------------------------------------------------------------------------------------------------------
