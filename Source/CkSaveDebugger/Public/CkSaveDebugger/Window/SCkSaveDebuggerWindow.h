#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkSaveDebugger/Model/CkSaveDebugger_Model.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CoreMinimal.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkDebug_SelectableLabel;
class STextBlock;
class SVerticalBox;
class SWidgetSwitcher;

// --------------------------------------------------------------------------------------------------------------------

/** Offline inspector for a CK `.sav` snapshot file.
 *
 *  There is no world, no PIE session and no ECS registry behind this window: it opens a file off disk, hands the
 *  bytes to `ck::snapshot::Inspect_*`, and renders the resulting document. Nothing it does can instantiate a saved
 *  entity, mutate a registry, or fabricate a live handle — every entity reference on screen is a raw saved id, which
 *  is exactly why `SCkDebug_EntityRef` (whose click navigates to a LIVE entity) is banned here.
 *
 *  Consequence for refresh: this debugger never polls. Every rebuild is driven by an explicit event — open, reload,
 *  filter, selection, decode — so the base's gated style-revision watch is the only per-tick work in the window. */
class SCkSaveDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkSaveDebuggerWindow) {}
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Save Debugger")); }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    // ---- Chrome construction ----
    auto DoCreate_MenuActions() -> TSharedRef<SWidget>;
    auto DoCreate_Toolbar() -> TSharedRef<SWidget>;
    auto DoCreate_Body() -> TSharedRef<SWidget>;
    auto DoCreate_Status() -> TSharedRef<SWidget>;
    auto DoCreate_ProvenanceChips() -> TSharedRef<SWidget>;
    auto DoCreate_BlobColumn() -> TSharedRef<SWidget>;
    auto DoCreate_DiffColumn() -> TSharedRef<SWidget>;

    // ---- Commands ----
    auto DoOnOpenClicked() -> FReply;
    auto DoOnReloadClicked() -> FReply;
    auto DoOnExportJsonClicked() -> FReply;
    auto DoOnCopyReportClicked() -> FReply;
    auto DoOnCompareAgainstClicked() -> FReply;
    auto DoOnCloseDiffClicked() -> FReply;
    auto DoOnCopyDiffReportClicked() -> FReply;
    auto DoOnTryDecodeClicked() -> FReply;
    auto DoOnTryLoadTypeClicked() -> FReply;
    auto DoOnCopyTypePathClicked() -> FReply;
    auto DoOnCopyMetadataClicked() -> FReply;
    auto DoOnExportBlobClicked() -> FReply;

    auto DoOpen_Path(const FString& InAbsolutePath) -> void;
    auto DoDecode_SelectedPayload(ECk_SnapshotInspection_DecodePolicy InPolicy) -> void;

    // ---- Rebuild entry points (never called from Tick) ----
    auto DoRebuild_All() -> void;
    auto DoRebuild_Summary() -> void;
    auto DoRebuild_Tree() -> void;
    auto DoRefresh_Filters() -> void;
    auto DoRebuild_EntityDetail() -> void;
    auto DoRebuild_BlobDetail() -> void;
    auto DoRebuild_Diagnostics() -> void;
    auto DoRebuild_Diff() -> void;
    auto DoRebuild_DiffDetail() -> void;

    auto DoSelect_Entity(uint32 InSavedId) -> void;
    auto DoSelect_Payload(int32 InPayloadIndex) -> void;

    // ---- Views ----
    auto DoGenerate_EntityRow(
        TSharedPtr<FCkSaveDebugger_TreeNode> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

    auto DoGet_EntityChildren(
        TSharedPtr<FCkSaveDebugger_TreeNode> InItem,
        TArray<TSharedPtr<FCkSaveDebugger_TreeNode>>& OutChildren) -> void;

    auto DoOnEntitySelectionChanged(
        TSharedPtr<FCkSaveDebugger_TreeNode> InItem,
        ESelectInfo::Type InSelectInfo) -> void;

    auto DoOnEntityContextMenu() -> TSharedPtr<SWidget>;

    auto DoGenerate_PayloadRow(
        TSharedPtr<FCkSaveDebugger_PayloadRow> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

    auto DoOnPayloadSelectionChanged(
        TSharedPtr<FCkSaveDebugger_PayloadRow> InItem,
        ESelectInfo::Type InSelectInfo) -> void;

    auto DoOnPayloadContextMenu() -> TSharedPtr<SWidget>;

    auto DoGenerate_DiagnosticRow(
        TSharedPtr<FCkSaveDebugger_DiagnosticRow> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

    auto DoOnDiagnosticSelectionChanged(
        TSharedPtr<FCkSaveDebugger_DiagnosticRow> InItem,
        ESelectInfo::Type InSelectInfo) -> void;

    auto DoOnDiagnosticContextMenu() -> TSharedPtr<SWidget>;

    auto DoGenerate_DiffGroupRow(
        TSharedPtr<FCkSaveDebugger_DiffGroupRow> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

    auto DoOnDiffGroupSelectionChanged(
        TSharedPtr<FCkSaveDebugger_DiffGroupRow> InItem,
        ESelectInfo::Type InSelectInfo) -> void;

    auto DoOnDiffGroupContextMenu() -> TSharedPtr<SWidget>;

    auto DoGenerate_ValueRow(
        TSharedPtr<FCk_SnapshotInspection_ValueNode> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

    auto DoGet_ValueChildren(
        TSharedPtr<FCk_SnapshotInspection_ValueNode> InItem,
        TArray<TSharedPtr<FCk_SnapshotInspection_ValueNode>>& OutChildren) -> void;

    // ---- Helpers ----
    auto DoSet_Status(const FString& InText, ECk_Tone InTone) -> void;

    auto DoBuild_SelectedBlobMetadata() const -> FString;

    /** The blob the right-hand panel currently addresses: the selected payload's bytes, or the selected entity's
     *  ActorSaveField bytes when no payload is selected and that blob is the only thing there is to show. */
    auto DoGet_SelectedBlobBytes() const -> TArray<uint8>;

private:
    FCkSaveDebugger_Model _Model;

    FString _CurrentPath;

    TSharedPtr<STreeView<TSharedPtr<FCkSaveDebugger_TreeNode>>> _EntityTree;
    TArray<TSharedPtr<FCkSaveDebugger_TreeNode>> _VisibleRoots;

    TSharedPtr<SListView<TSharedPtr<FCkSaveDebugger_PayloadRow>>> _PayloadList;
    TArray<TSharedPtr<FCkSaveDebugger_PayloadRow>> _PayloadRows;
    TMap<int32, TSharedPtr<FCkSaveDebugger_PayloadRow>> _PayloadRowsByIndex;

    TSharedPtr<SListView<TSharedPtr<FCkSaveDebugger_DiagnosticRow>>> _DiagnosticList;
    TArray<TSharedPtr<FCkSaveDebugger_DiagnosticRow>> _DiagnosticRows;
    TMap<int32, TSharedPtr<FCkSaveDebugger_DiagnosticRow>> _DiagnosticRowsByIndex;

    TSharedPtr<STreeView<TSharedPtr<FCk_SnapshotInspection_ValueNode>>> _ValueTree;
    TArray<TSharedPtr<FCk_SnapshotInspection_ValueNode>> _ValueRoots;

    // A diff session is a different task than blob inspection, so it TAKES the right-hand column rather than
    // squeezing a fourth one in — the ownership tree and the payload list stay put beside it.
    TSharedPtr<SWidgetSwitcher> _RightColumnSwitcher;

    TSharedPtr<SListView<TSharedPtr<FCkSaveDebugger_DiffGroupRow>>> _DiffGroupList;
    TArray<TSharedPtr<FCkSaveDebugger_DiffGroupRow>> _DiffGroupRows;
    TMap<FString, TSharedPtr<FCkSaveDebugger_DiffGroupRow>> _DiffGroupRowsByPath;

    TSharedPtr<SVerticalBox> _SummaryBox;
    TSharedPtr<SVerticalBox> _EntityDetailBox;
    TSharedPtr<SVerticalBox> _BlobDetailBox;
    TSharedPtr<SVerticalBox> _DiffHeaderBox;
    TSharedPtr<SVerticalBox> _DiffDetailBox;

    // Which group's per-type table the detail panel under the list is showing. Identity paths, not saved ids —
    // ids are not stable across captures, which is the whole reason the diff groups by identity.
    FString _DiffSelectedIdentityPath;

    TSharedPtr<SCkDebug_SelectableLabel> _PathLabel;
    TSharedPtr<STextBlock> _StatusText;

    ECk_Tone _StatusTone = ECk_Tone::Neutral;

    // Toolbar name-verbosity state (SCkDebug_NameDepthCycler): how many trailing segments of a type path the
    // payload list and the recipe rows show. 0 = full path. The ceiling is recomputed per document, never per paint.
    int32 _NameDepth = 1;
    int32 _MaxNameDepth = 1;

    // True while a programmatic selection is being applied, so the view's echo does not re-enter the rebuild path.
    bool _SuppressSelectionEcho = false;
};

// --------------------------------------------------------------------------------------------------------------------
