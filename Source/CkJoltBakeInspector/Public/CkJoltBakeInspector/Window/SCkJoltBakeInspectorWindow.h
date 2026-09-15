#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkJoltEditor/Cook/CkJoltCook_MeshShapeAudit.h"
#include "CkJolt/CkJolt_Utils.h"
#include "CkJoltBakeInspector/CkJoltBakeInspector_Policy.h"

#include <AssetRegistry/AssetData.h>

class SListViewBase;
template <typename ItemType> class SListView;
class SCkDebug_SearchBar;
class SCkJoltBakeInspectorPreview;
class SCkDebug_PaneHost;
class SCkDebug_StatusPill;
class FCkUiView;
class SBox;
class SCkDebug_WindowChrome;
struct FCkJoltBakeInspectorAuthoredTestAccess;

struct FCkJoltBakeInspectorRow
{
    FAssetData Asset;
    FString PackagePath;
    FString DisplayName;
    FString Classification;
    FString Detail;
    TOptional<ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult> Audit;
    /** Retains the document-authored ordinary row presentation while the virtualized list owns its identity. */
    TSharedPtr<FCkUiView> Presentation;
};

class SCkJoltBakeInspectorWindow final : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;
    using FRowPtr = TSharedPtr<FCkJoltBakeInspectorRow>;

    SLATE_BEGIN_ARGS(SCkJoltBakeInspectorWindow) {}
#if WITH_DEV_AUTOMATION_TESTS
        /** Test-only alternate resource directory for exercising the real fallback/recovery path. */
        SLATE_ARGUMENT(FString, TestResourceDirectory)
#endif
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkJoltBakeInspectorWindow() override;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto Get_WindowId() const -> FName override { return WindowId; }
    auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Jolt Bake Inspector")); }
    /** Idempotent owner release used by module close/pre-exit before preview-world teardown. */
    auto Release_AuthoredPresentation() -> void;
    auto Get_AuthoredView() const -> TSharedPtr<FCkUiView> { return _AuthoredView; }
    auto Get_NativePreview() const -> TSharedPtr<SCkJoltBakeInspectorPreview> { return _Preview; }
    auto Get_NativeList() const -> TSharedPtr<SListView<FRowPtr>> { return _ListView; }
    auto Get_NativeSearch() const -> TSharedPtr<SCkDebug_SearchBar> { return _SearchBar; }
    auto IsUsingNativeFallback() const -> bool { return _UsingNativeFallback; }

private:
    friend struct FCkJoltBakeInspectorAuthoredTestAccess;

    auto RefreshInventory() -> void;
    auto OnRefreshClicked() -> FReply;
    auto RefilterRows() -> void;
    auto OnSearchChanged(const FString& InText) -> void;
    auto OnSelectionChanged(FRowPtr InRow, ESelectInfo::Type InSelection) -> void;
    auto AnalyzeSelectedRow() -> void;
    auto AnalyzeRow(const FRowPtr& InRow) -> void;
    auto StartAnalyzeAll() -> FReply;
    auto CancelAnalyzeAll() -> FReply;
    auto CancelAnalysis() -> void;
    auto SetFilterMode(int32 InMode) -> FReply;
    auto GetIsAnalyzing() const -> bool;
    auto GenerateRow(FRowPtr InRow, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto OpenSelectedAsset() -> FReply;
    auto BrowseSelectedAsset() -> FReply;
    auto BakeSelected() -> FReply;
    auto BakeAll() -> FReply;
    auto CanBakeSelected() const -> bool;
    auto CanBakeAll() const -> bool;
    auto GetSummaryText() const -> FText;
    auto GetMetricText(int32 InMetric) const -> FText;
    auto GetSelectedSourceText() const -> FText;
    auto GetSelectedCookedText() const -> FText;
    auto GetSelectedDiagnosisText() const -> FText;
    auto BuildNativeContent() -> TSharedRef<SWidget>;
    auto BuildAuthoredPresentation() -> void;
    auto BuildAuthoredRow(const FRowPtr& InRow) -> TSharedPtr<SWidget>;
    auto PollAuthoredPresentation(double InCurrentTime) -> void;

    TArray<FRowPtr> _AllRows;
    TArray<FRowPtr> _VisibleRows;
    FRowPtr _SelectedRow;
    FString _FilterText;
    int32 _FilterMode = 0;
    TArray<FRowPtr> _AnalysisQueue;
    ck::jolt_bake_inspector::FCkJoltBakeInspectorAnalysisQueue _AnalysisState;
    TUniquePtr<ck::jolt::FCk_Jolt_ScopedGlobalInit> _AnalysisJoltLease;
    TSharedPtr<SListView<FRowPtr>> _ListView;
    TSharedPtr<SCkDebug_SearchBar> _SearchBar;
    TSharedPtr<SCkJoltBakeInspectorPreview> _Preview;
    TSharedPtr<SCkDebug_PaneHost> _ListPane;
    TSharedPtr<SCkDebug_PaneHost> _PreviewPane;
    TSharedPtr<SBox> _ListPaneMount;
    TSharedPtr<SBox> _PreviewPaneMount;
    /** Disjoint fallback mounts ensure held fallback chrome cannot retain transferred ports. */
    TSharedPtr<SBox> _FallbackSearchHost;
    TSharedPtr<SBox> _FallbackListHost;
    TSharedPtr<SBox> _FallbackPreviewHost;
    TSharedPtr<SCkDebug_StatusPill> _SelectedStatus;
    TSharedPtr<SCkDebug_WindowChrome> _NativeChrome;
    TSharedPtr<FCkUiView> _AuthoredView;
    FString _AuthoredMarkupPath;
    FString _AuthoredStylesheetPath;
    FString _RowMarkupPath;
    FString _RowStylesheetPath;
    FString _AppliedRowMarkup;
    FString _AppliedRowStylesheet;
    TMap<FString, FString> _AppliedRowStyleTokens;
    bool _HasRowSourceState = false;
#if WITH_DEV_AUTOMATION_TESTS
    FString _TestResourceDirectory;
#endif
    double _NextAuthoredPollSeconds = 0.0;
    bool _AuthoredPresentationReleased = false;
    bool _UsingNativeFallback = true;
};
