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

struct FCkJoltBakeInspectorRow
{
    FAssetData Asset;
    FString PackagePath;
    FString DisplayName;
    FString Classification;
    FString Detail;
    TOptional<ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult> Audit;
};

class SCkJoltBakeInspectorWindow final : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkJoltBakeInspectorWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkJoltBakeInspectorWindow() override;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto Get_WindowId() const -> FName override { return WindowId; }
    auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Jolt Bake Inspector")); }

private:
    using FRowPtr = TSharedPtr<FCkJoltBakeInspectorRow>;

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
};
