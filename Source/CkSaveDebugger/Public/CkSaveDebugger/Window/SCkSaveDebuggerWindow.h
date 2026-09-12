#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkSaveDebugger/Model/CkSaveDebugger_Model.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CoreMinimal.h"
#include "Slate/DeferredCleanupSlateBrush.h" // sidecar thumbnail — outlives the document it was decoded from
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkDebug_SelectableLabel;
class STextBlock;
class SVerticalBox;
class SWidgetSwitcher;
class FCkUiView;
class FCkUiTreeCollection;
class SCkUiTree;
struct FCkUiLoadResult;
struct FSlateBrush;

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
class CKSAVEDEBUGGER_API SCkSaveDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkSaveDebuggerWindow) {}
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

    // The editor-viewport visualizer outlives no window: the EdMode and the published row snapshot are torn down
    // with the Slate that owns them.
    virtual ~SCkSaveDebuggerWindow() override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Save Debugger")); }

    /** Narrow authored-navigation seams for production-path widget coverage. */
    auto TryReload_EntityNavigationLayout(const FString& InMarkup, const FString& InStylesheet) -> FCkUiLoadResult;
    auto Get_AuthoredEntityTree() const -> TSharedPtr<SCkUiTree>;
    auto Get_EntityNavigationLayoutRevision() const -> int64;
    auto Get_EntityNavigationLayoutError() const -> FText;
    auto Get_EntityUiKey(uint32 InSavedId) const -> TOptional<FString>;
    auto Get_EntityFilterStringForTest() const -> const FString&;
    auto Get_EntityHighlightStringForTest() const -> const FString&;
    auto Open_SaveFileForTest(const FString& InAbsolutePath) -> bool;
    const FString& Get_LastEntityContextActionKey() const { return _LastEntityContextActionKey; }
    int32 Get_EntityContextActionCount() const { return _EntityContextActionCount; }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    // ---- Chrome construction ----
    auto DoCreate_MenuActions() -> TSharedRef<SWidget>;
    auto DoCreate_FileControls() -> TSharedRef<SWidget>;
    auto DoCreate_VisualizationControls() -> TSharedRef<SWidget>;
    auto DoCreate_ExportControls() -> TSharedRef<SWidget>;
    auto DoCreate_FileStatus() -> TSharedRef<SWidget>;
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
    auto DoOnVisualizeClicked() -> FReply;
    auto DoOnFrameSelectedClicked() -> FReply;
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
    // Built by DoRebuild_Summary and seated beside File & Header — the sidecar rides the same document.
    auto DoCreate_SlotMetaPanel() -> TSharedRef<SWidget>;
    auto DoCreate_DiagnosticSeverityPills() -> TSharedRef<SWidget>;
    auto DoOnThumbnailClicked() -> FReply;
    auto Get_SeverityShown(ECk_SnapshotInspection_Severity InSeverity) const -> bool;
    auto Set_SeverityShown(ECk_SnapshotInspection_Severity InSeverity, bool InShown) -> void;
    auto DoRebuild_Tree() -> void;
    auto DoRefresh_Filters() -> void;
    auto DoRebuild_EntityDetail() -> void;
    auto DoRebuild_BlobDetail() -> void;
    auto DoRebuild_Diagnostics() -> void;
    auto DoRebuild_Diff() -> void;
    auto DoRebuild_DiffDetail() -> void;

    // ---- Authored entity navigation ----
    auto DoInitialize_EntityNavigation() -> void;
    auto DoReload_EntityNavigationLayout() -> void;
    auto DoTick_EntityNavigationLayout(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType;
    auto DoPublish_EntityNavigation() -> void;
    auto DoOnAuthoredEntitySelectionChanged(TOptional<FString> InKey, ESelectInfo::Type InSelectInfo) -> void;
    auto DoOnCopyEntityIdentity(const FString& InKey) -> void;
    auto DoOnCopyEntitySavedId(const FString& InKey) -> void;
    auto TryGet_EntityNodeForUiKey(const FString& InKey) const -> TSharedPtr<FCkSaveDebugger_TreeNode>;

    auto DoSelect_Entity(uint32 InSavedId) -> void;
    auto DoSelect_Payload(int32 InPayloadIndex) -> void;

    // ---- Views ----
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

    // ---- Editor-viewport visualizer (bodies are no-ops outside WITH_EDITOR) ----

    /** Projects the current document into visualization rows and publishes them to the EdMode. Returns false when
     *  the save carries nothing placeable — the caller decides whether that stops the visualizer or blocks a start. */
    auto DoVisualize_Publish() -> bool;
    auto DoVisualize_Stop() -> void;
    auto DoVisualize_SyncSelection() -> void;

    /** Rebuilds the model's SaveKey -> "ActorName (Class)" map from the open editor level: an EngineOwned row's key
     *  is a deterministic hash of (level package + actor name), so the world can be asked what the file cannot say. */
    auto DoRefresh_ActorAnnotations() -> void;

    auto DoOnTreeKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply;
    auto DoOnEditorSelectionChanged(UObject* InSelectionObject) -> void;
    auto DoOnMapOpened(const FString& InFilename, bool InAsTemplate) -> void;

    // ---- Helpers ----
    auto DoSet_Status(const FString& InText, ECk_Tone InTone) -> void;

    auto DoBuild_SelectedBlobMetadata() const -> FString;

    /** The blob the right-hand panel currently addresses: the selected payload's bytes, or the selected entity's
     *  ActorSaveField bytes when no payload is selected and that blob is the only thing there is to show. */
    auto DoGet_SelectedBlobBytes() const -> TArray<uint8>;

private:
    FCkSaveDebugger_Model _Model;

    FString _CurrentPath;

    TSharedPtr<FCkUiView> _EntityNavigationView;
    TSharedPtr<FCkUiTreeCollection> _EntityNavigationModel;
    TMap<FString, TWeakPtr<FCkSaveDebugger_TreeNode>> _EntityNodesByUiKey;
    /** Own copied style brushes: authored Image fields may outlive the style pointer used to publish a row. */
    TMap<const FSlateBrush*, TSharedPtr<const FSlateBrush>> _EntityNavigationBrushes;
    FString _EntityNavigationPublicationError;
    bool _EntityNavigationInitialExpansionApplied = false;
    FString _LastEntityContextActionKey;
    int32 _EntityContextActionCount = 0;

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

    // The sidecar's thumbnail, decoded on open. The texture is transient and unrooted, so it is held strongly
    // here — and the brush defers its own cleanup, because Slate may still be drawing it after the next open
    // replaces both.
    TStrongObjectPtr<UTexture2D>              _SlotMetaThumbnail;
    TSharedPtr<FDeferredCleanupSlateBrush>    _SlotMetaThumbnailBrush;

    // One bit per ECk_SnapshotInspection_Severity. All on by default — a file's diagnostics are the reason to
    // open it, so nothing is hidden until the reader says so.
    uint8 _DiagnosticSeverityMask = 0xFFu;
    bool  _DiagnosticsExpanded = true;

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

    // What the last retained-visuals rebuild produced ("N previews, M ghost meshes, ..."), for the status line.
    FString _VisualizeSummary;

    // True while a programmatic selection is being applied, so the view's echo does not re-enter the rebuild path.
    bool _SuppressSelectionEcho = false;
};

// --------------------------------------------------------------------------------------------------------------------
