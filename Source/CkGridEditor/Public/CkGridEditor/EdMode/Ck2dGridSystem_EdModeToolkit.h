#pragma once

#include "CoreMinimal.h"

#include "Toolkits/BaseToolkit.h"

#include "Types/SlateEnums.h"

#include <GameplayTagContainer.h>

// --------------------------------------------------------------------------------------------------------------------

enum class ECk_GridPaint_Tool : uint8;
enum class ECk_GridPaint_TagScope : uint8;
enum class ECk_Tone : uint8;

class AActor;
class SVerticalBox;
class ITableRow;
class STableViewBase;
template <typename ItemType> class SListView;

// --------------------------------------------------------------------------------------------------------------------

// One row in the Select-tab Blockers list.
struct FCk_GridBlockerListItem
{
    int32        Index = INDEX_NONE;
    FGameplayTag Name;
    FIntPoint    RangeMin = FIntPoint::ZeroValue;
    FIntPoint    RangeMax = FIntPoint::ZeroValue;
};

// One row in the Select-tab Tags list (doubles as the clickable legend).
struct FCk_GridTagListItem
{
    FGameplayTag Tag;
    int32        CellCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

// Toolkit for the Grid Paint editor mode: a 4-tool selector that sets the active tool on the owning
// UCk_2dGridSystem_EdMode, plus the per-tool sections it reveals.
class FCk_2dGridSystem_EdModeToolkit : public FModeToolkit
{
public:
    FCk_2dGridSystem_EdModeToolkit();

    // FModeToolkit interface
    virtual void Init(const TSharedPtr<IToolkitHost>& InToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
    virtual void GetToolPaletteNames(TArray<FName>& OutPaletteNames) const override;

    // IToolkit interface
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual TSharedPtr<SWidget> GetInlineContent() const override;

private:
    auto Set_ActiveTool(ECk_GridPaint_Tool InTool) -> void;

    // Defaults to Shape if the owning mode is gone.
    auto Get_ActiveTool() const -> ECk_GridPaint_Tool;

    // The always-visible roster of every grid spawner in the editor world, so the targeted grid can be
    // swapped without hunting through the outliner. Selecting the actor IS targeting the grid — the
    // EdMode resolves its target from the actor selection.
    auto Build_GridsInLevelSection() -> TSharedRef<SWidget>;

    // Live-bound; also drives the roster rebuild on a signature change, the way the multi-cell summary
    // drives its own tag-union list.
    auto Get_GridsInLevelSummaryText() const -> FText;
    auto Rebuild_GridsInLevelList() -> void;
    auto Compute_GridsInLevelSignature() const -> FString;

    auto On_SelectGridSpawner(TWeakObjectPtr<AActor> InSpawner) -> FReply;
    auto On_FrameGridSpawner(TWeakObjectPtr<AActor> InSpawner) -> FReply;

    // Debug-visibility only: the unselected grids draw their authored overlay, never the interaction ones.
    auto Get_ShowAllGrids() const -> bool;
    auto On_ShowAllGridsChanged(bool InShowAll) -> void;

    // The always-visible Grid section: Dimensions and Cell Size numeric entry.
    auto Build_GridSection() -> TSharedRef<SWidget>;

    // Unset when no grid spawner is selected.
    auto Get_GridDimensionX() const -> TOptional<int32>;
    auto Get_GridDimensionY() const -> TOptional<int32>;
    auto Get_GridCellSizeX() const -> TOptional<double>;
    auto Get_GridCellSizeY() const -> TOptional<double>;

    // Each reads the other axis off the Spec, then pushes the combined value.
    auto On_GridDimensionXCommitted(int32 InValue, ETextCommit::Type InCommitType) -> void;
    auto On_GridDimensionYCommitted(int32 InValue, ETextCommit::Type InCommitType) -> void;
    auto On_GridCellSizeXCommitted(double InValue, ETextCommit::Type InCommitType) -> void;
    auto On_GridCellSizeYCommitted(double InValue, ETextCommit::Type InCommitType) -> void;

    // Tag combo + scope toggle + grid-default Apply/Remove; visible only while the Tags tool is active.
    auto Build_TagsSection() -> TSharedRef<SWidget>;
    auto Get_TagsSectionVisibility() const -> EVisibility;

    // Every tag control in this toolkit is an SGameplayTagCombo: it edits exactly ONE tag, is live-bound to
    // its backing value through a Get_* attribute, and hands back that one tag. SGameplayTagPicker is not
    // used because its container semantics are hierarchical — it renders ancestors as checked and re-adds
    // the parent on an uncheck, which silently promoted A.B.C to A.B.
    auto Get_ActivePaintTag() const -> FGameplayTag;
    auto On_PaintTagChanged(FGameplayTag InTag) -> void;

    // Defaults to PerCellBulk if the owning mode is gone.
    auto Get_ActiveTagScope() const -> ECk_GridPaint_TagScope;
    auto On_ActiveTagScopeChanged(ECk_GridPaint_TagScope InScope) -> void;

    // Enabled only in GridDefault scope.
    auto Get_GridDefaultButtonsEnabled() const -> bool;
    auto On_ApplyGridDefaultTag() -> FReply;
    auto On_RemoveGridDefaultTag() -> FReply;

    // Read-only per-tag color legend, rebuilt from the Spec whenever its signature changes.
    auto Rebuild_TagLegend() -> void;
    auto Compute_TagLegendSignature() const -> FString;

    // New-blocker + selected-blocker tag combos; visible only while the Blocker tool is active.
    auto Build_BlockerSection() -> TSharedRef<SWidget>;
    auto Get_BlockerSectionVisibility() const -> EVisibility;

    // Becomes the EdMode's _ActiveBlockerTag, stamped onto the next drag-rect blocker.
    auto Get_ActiveBlockerTag() const -> FGameplayTag;
    auto On_NewBlockerTagChanged(FGameplayTag InTag) -> void;

    // Written to the selected blocker via Set_SelectedBlockerName (transacted + rebuild). Shared by the
    // Blocker section's editor and the Select-tool Details blocker editor — both edit the same blocker.
    auto Get_SelectedBlockerTag() const -> FGameplayTag;
    auto On_SelectedBlockerTagChanged(FGameplayTag InTag) -> void;

    auto Get_SelectedBlockerEditorVisibility() const -> EVisibility;

    auto Get_SelectedBlockerText() const -> FText;

    // The Select-tool Details panel: three MUTUALLY EXCLUSIVE editors — a single-CELL editor, a MULTI-CELL
    // bulk editor shown once the marquee selected more than one cell, and a BLOCKER editor shown when the
    // pick landed on a blocker — switched by the sub-block visibilities below.
    auto Build_DetailsSection() -> TSharedRef<SWidget>;
    auto Get_DetailsSectionVisibility() const -> EVisibility;
    auto Get_DetailsCellEditorVisibility() const -> EVisibility;
    auto Get_DetailsMultiCellEditorVisibility() const -> EVisibility;
    auto Get_DetailsBlockerEditorVisibility() const -> EVisibility;

    // Live-bound: each reads the EdMode's selected-cell info off the Spec every repaint.
    auto Get_DetailsCoordinateText() const -> FText;
    auto Get_DetailsStateText() const -> FText;
    auto Get_DetailsGridDefaultTagsText() const -> FText;

    // The State readout's pill tone, keyed off the same ECellState the text is: Err for a disabled
    // cell, Warn for a blocked one, Ok for an enabled one. Collapsed while nothing is picked, because
    // the state text is empty there and an empty pill still paints its fill.
    auto Get_DetailsStateTone() const -> ECk_Tone;
    auto Get_DetailsStateVisibility() const -> EVisibility;

    auto Get_SelectedCellDisabled() const -> bool;
    auto On_SelectedCellDisabledChanged(bool InIsDisabled) -> void;

    // One row per tag with a Remove button. Re-driven from the live-bound coordinate getter when the
    // signature changes, so the list tracks live edits (picker add, row remove, external Spec edits).
    auto Rebuild_PerCellTagList() -> void;
    auto Compute_PerCellTagListSignature() const -> FString;

    // Writes the picked tag straight through to the selected cell; the combo itself holds nothing.
    auto On_AddCellTagChanged(FGameplayTag InTag) -> void;

    // Bound per row to the row's tag.
    auto On_RemoveCellTag(FGameplayTag InTag) -> FReply;

    // The multi-cell bulk editor: a count summary, the union of the selection's per-cell tags with the
    // number of selected cells carrying each, and the bulk add/remove/disable actions.
    auto Build_MultiCellEditor() -> TSharedRef<SWidget>;

    // Also drives the tag-union list rebuild, the same way Get_DetailsCoordinateText drives the
    // single-cell one — a collapsed editor's attributes are never evaluated, so each owns its driver.
    auto Get_MultiCellSummaryText() const -> FText;
    auto Rebuild_MultiCellTagList() -> void;
    auto Compute_MultiCellTagListSignature() const -> FString;

    // Bound per row to the row's tag.
    auto On_RemoveTagFromSelection(FGameplayTag InTag) -> FReply;

    // Unlike the single-cell add combo, this one KEEPS its tag — the Add button is the commit.
    auto Get_BulkAddTag() const -> FGameplayTag;
    auto On_BulkAddTagChanged(FGameplayTag InTag) -> void;
    auto Get_BulkAddButtonEnabled() const -> bool;
    auto On_AddTagToSelection() -> FReply;

    auto On_DisableSelection() -> FReply;
    auto On_EnableSelection() -> FReply;

    auto Get_DetailsBlockerText() const -> FText;
    auto On_DeleteSelectedBlocker() -> FReply;

    // Blockers + Tags list items, rebuilt from the Spec whenever their signature changes.
    auto Rebuild_SelectLists() -> void;
    auto Compute_SelectListsSignature() const -> FString;

    // The section headers' trailing count badges: SCkDebug_SectionHeader's own CountText is a
    // construct-time argument, and both lists change under the user.
    auto Get_BlockerCountText() const -> FText;
    auto Get_TagCountText() const -> FText;

    auto OnGenerate_BlockerRow(TSharedPtr<FCk_GridBlockerListItem> InItem, const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>;
    auto OnGenerate_TagRow(TSharedPtr<FCk_GridTagListItem> InItem, const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>;

    // Push the chosen blocker/tag onto the EdMode, which highlights it in the viewport.
    auto On_BlockerRowSelected(TSharedPtr<FCk_GridBlockerListItem> InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto On_TagRowSelected(TSharedPtr<FCk_GridTagListItem> InItem, ESelectInfo::Type InSelectInfo) -> void;

    // The always-last section: the viewport gestures the ACTIVE tool answers to. All four per-tool blocks
    // are built once and switched by the visibilities below, the way the tool sections above are.
    auto Build_ControlsSection() -> TSharedRef<SWidget>;

    auto Get_ShapeHintsVisibility() const -> EVisibility;
    auto Get_TagsHintsVisibility() const -> EVisibility;
    auto Get_BlockerHintsVisibility() const -> EVisibility;
    auto Get_SelectHintsVisibility() const -> EVisibility;

private:
    TSharedPtr<SWidget>     InlineContent;
    TWeakObjectPtr<UEdMode> OwningMode;

    // One clickable row per grid spawner in the editor world.
    TSharedPtr<SVerticalBox> GridsInLevelContainer;
    FString SeededGridsInLevelSignature;

    // Tags section: one read-only swatch+name row per distinct per-cell tag.
    TSharedPtr<SVerticalBox> TagLegendContainer;
    FString SeededTagLegendSignature;

    // One removable row per tag on the selected cell.
    TSharedPtr<SVerticalBox> PerCellTagListContainer;

    // Selected cell + its tags, as the rows were last built; the live-bound coordinate getter re-drives
    // Rebuild_PerCellTagList on a change.
    FString SeededPerCellTagSignature;

    // One removable row per tag in the multi-cell selection's tag union.
    TSharedPtr<SVerticalBox> MultiCellTagListContainer;
    FString SeededMultiCellTagSignature;

    // Held by the multi-cell add combo until the Add button commits it.
    FGameplayTag BulkAddTag;

    TArray<TSharedPtr<FCk_GridBlockerListItem>> BlockerListItems;
    TArray<TSharedPtr<FCk_GridTagListItem>>     TagListItems;
    TSharedPtr<SListView<TSharedPtr<FCk_GridBlockerListItem>>> BlockerListView;
    TSharedPtr<SListView<TSharedPtr<FCk_GridTagListItem>>>     TagListView;
    FString SeededSelectListsSignature;
};

// --------------------------------------------------------------------------------------------------------------------
