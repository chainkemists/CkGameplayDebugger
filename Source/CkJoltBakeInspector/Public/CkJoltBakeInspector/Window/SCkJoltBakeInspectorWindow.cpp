#include "CkJoltBakeInspector/Window/SCkJoltBakeInspectorWindow.h"
#include "CkJoltBakeInspector/CkJoltBakeInspector_Policy.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkJolt/Settings/CkJolt_ProjectSettings.h"
#include "CkJoltEditor/Cook/CkJoltCook_EditorSubsystem.h"
#include "CkJoltEditor/Cook/CkJoltCook_MeshShapeAudit.h"
#include "CkJoltBakeInspector/Viewport/SCkJoltBakeInspectorPreview.h"

#include <AssetRegistry/AssetRegistryModule.h>
#include <AssetRegistry/IAssetRegistry.h>
#include <Editor.h>
#include <Engine/StaticMesh.h>
#include <Subsystems/AssetEditorSubsystem.h>
#include <UObject/StrongObjectPtr.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SSearchBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Layout/SSplitter.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/SListView.h>

const FName SCkJoltBakeInspectorWindow::WindowId{TEXT("CkJoltBakeInspector")};

namespace ck_jolt_bake_inspector_window
{
    auto MakeButton(const TCHAR* InText, const FOnClicked& InClicked) -> TSharedRef<SWidget>
    {
        return SNew(SButton).Text(FText::FromString(InText)).OnClicked(InClicked);
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction InAction) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction;
        switch (InAction)
        {
            case None: return TEXT("No action");
            case CookMissing: return TEXT("Cook missing shape");
            case RebuildStale: return TEXT("Rebuild stale shape");
            case RebuildInsideOut: return TEXT("Rebuild inside-out shape");
            case FixSource: return TEXT("Fix source collision");
            case DeleteOrphan: return TEXT("Delete orphaned generated shape");
        }
        return TEXT("Unknown action");
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditSourceState InState) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditSourceState;
        switch (InState)
        {
            case Ready: return TEXT("Source ready");
            case MissingBodySetup: return TEXT("Missing BodySetup");
            case NotWorthPreBaking: return TEXT("Not worth pre-baking");
            case MissingTriMesh: return TEXT("Missing tri-mesh");
            case InvalidTriMesh: return TEXT("Invalid tri-mesh");
            case InvalidTriangleIndices: return TEXT("Invalid triangle indices");
            case UnsupportedCookedPath: return TEXT("Unsupported cooked-shape path");
        }
        return TEXT("Unknown source state");
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedState InState) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedState;
        switch (InState)
        {
            case Missing: return TEXT("Missing");
            case Current: return TEXT("Current");
            case StaleCookVersion: return TEXT("Stale cook version");
            case StaleJoltVersion: return TEXT("Stale Jolt version");
            case StaleBodySetup: return TEXT("Stale BodySetup");
            case StaleTraceFlag: return TEXT("Stale collision trace flag");
            case Corrupt: return TEXT("Corrupt");
            case Orphan: return TEXT("Orphan");
        }
        return TEXT("Unknown cooked state");
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditWindingVerdict InVerdict) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditWindingVerdict;
        switch (InVerdict)
        {
            case NotTriMesh: return TEXT("Not tri-mesh");
            case NoVerdict: return TEXT("No winding verdict");
            case Outward: return TEXT("Outward");
            case InsideOut: return TEXT("Inside-out");
        }
        return TEXT("Unknown winding verdict");
    }
}

auto SCkJoltBakeInspectorWindow::Construct(const FArguments&) -> void
{
    Register_WithGate();

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
            [ SAssignNew(_SearchBox, SSearchBox).OnTextChanged(this, &SCkJoltBakeInspectorWindow::OnSearchChanged) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Refresh"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::OnRefreshClicked)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Analyze All"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::StartAnalyzeAll)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Cancel"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::CancelAnalyzeAll)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("All"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::SetFilterMode, 0)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Heuristic"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::SetFilterMode, 1)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Would Fail"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::SetFilterMode, 2)) ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f, 0.0f)
        [ SNew(STextBlock).Text(this, &SCkJoltBakeInspectorWindow::GetSummaryText) ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(6.0f)
        [
            SNew(SSplitter).Orientation(Orient_Horizontal)
            + SSplitter::Slot().Value(0.48f)
            [
                SAssignNew(_ListView, SListView<FRowPtr>)
                .ListItemsSource(&_VisibleRows)
                .OnGenerateRow(this, &SCkJoltBakeInspectorWindow::GenerateRow)
                .OnSelectionChanged(this, &SCkJoltBakeInspectorWindow::OnSelectionChanged)
            ]
            + SSplitter::Slot().Value(0.52f)
            [
                SNew(SBorder).Padding(8.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().FillHeight(0.48f)
                    [ SAssignNew(_Preview, SCkJoltBakeInspectorPreview) ]
                    + SVerticalBox::Slot().FillHeight(0.52f).Padding(0.0f, 8.0f, 0.0f, 0.0f)
                    [ SNew(STextBlock).AutoWrapText(true).Text(this, &SCkJoltBakeInspectorWindow::GetSelectedDetailText) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
                        [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Show in Content Browser"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::BrowseSelectedAsset)) ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
                        [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Open Asset"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::OpenSelectedAsset)) ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("Bake Selected")))
                            .IsEnabled(this, &SCkJoltBakeInspectorWindow::CanBakeSelected)
                            .OnClicked(this, &SCkJoltBakeInspectorWindow::BakeSelected)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("Bake All Repairable/Stale")))
                            .IsEnabled(this, &SCkJoltBakeInspectorWindow::CanBakeAll)
                            .OnClicked(this, &SCkJoltBakeInspectorWindow::BakeAll)
                        ]
                    ]
                ]
            ]
        ]
    ];

    RefreshInventory();
}

SCkJoltBakeInspectorWindow::~SCkJoltBakeInspectorWindow()
{ CancelAnalysis(); }

auto SCkJoltBakeInspectorWindow::Tick(const FGeometry& InGeometry, double InTime, float InDeltaTime) -> void
{
    SCkDebugger_WindowBase::Tick(InGeometry, InTime, InDeltaTime);
    if (NOT GetIsAnalyzing()) { return; }
    const auto NextIndex = _AnalysisState.TryTakeNext();
    if (NOT NextIndex.IsSet() || NOT _AnalysisQueue.IsValidIndex(*NextIndex)) { CancelAnalysis(); return; }
    AnalyzeRow(_AnalysisQueue[*NextIndex]);
    // All-rows mode keeps the stable item source and reads row text through attributes. Filtered modes alone need a
    // list-source change when a just-audited row enters or leaves the result.
    if (_FilterMode != 0 || NOT _FilterText.IsEmpty()) { RefilterRows(); }
    if (NOT GetIsAnalyzing()) { CancelAnalysis(); }
}

auto SCkJoltBakeInspectorWindow::RefreshInventory() -> void
{
    CancelAnalysis();
    const auto SelectedPath = _SelectedRow.IsValid() ? _SelectedRow->Asset.GetSoftObjectPath() : FSoftObjectPath{};
    auto ExistingRows = TMap<FSoftObjectPath, FRowPtr>{};
    for (const auto& Existing : _AllRows)
    { ExistingRows.Add(Existing->Asset.GetSoftObjectPath(), Existing); }
    _AllRows.Reset();
    _SelectedRow.Reset();

    auto& Registry = FAssetRegistryModule::GetRegistry();
    auto Filter = FARFilter{};
    Filter.ClassPaths.Emplace(UStaticMesh::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;
    for (const auto& Root : UCk_Utils_Jolt_ProjectSettings::Get_BakedMeshShapeRoots())
    { Filter.PackagePaths.Emplace(*Root); }

    auto Assets = TArray<FAssetData>{};
    Registry.GetAssets(Filter, Assets);
    Assets.Sort([](const FAssetData& A, const FAssetData& B)
    { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });

    for (const auto& Asset : Assets)
    {
        const auto AssetPath = Asset.GetSoftObjectPath();
        const auto Row = ExistingRows.FindRef(AssetPath).IsValid()
            ? ExistingRows.FindRef(AssetPath)
            : MakeShared<FCkJoltBakeInspectorRow>();
        Row->Asset = Asset;
        Row->PackagePath = Asset.PackageName.ToString();
        Row->DisplayName = Asset.AssetName.ToString();
        // The audit adapter fills these values only after a user selects a row. Inventory remains registry-only.
        Row->Classification = TEXT("Not analyzed");
        Row->Detail = TEXT("Select this mesh to run the read-only Jolt bake audit.");
        Row->Audit.Reset();
        _AllRows.Add(Row);
        if (Asset.GetSoftObjectPath() == SelectedPath)
        { _SelectedRow = Row; }
    }

    RefilterRows();
    if (_SelectedRow.IsValid() && _ListView.IsValid())
    {
        _ListView->SetSelection(_SelectedRow, ESelectInfo::Direct);
        AnalyzeSelectedRow();
        if (_Preview.IsValid() && _SelectedRow->Audit.IsSet()) { _Preview->Show_Audit(_SelectedRow->Audit.GetValue()); }
    }
}

auto SCkJoltBakeInspectorWindow::OnRefreshClicked() -> FReply
{
    RefreshInventory();
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::RefilterRows() -> void
{
    _VisibleRows.Reset();
    for (const auto& Row : _AllRows)
    {
        const auto MatchesText = _FilterText.IsEmpty() || Row->DisplayName.Contains(_FilterText, ESearchCase::IgnoreCase) ||
            Row->PackagePath.Contains(_FilterText, ESearchCase::IgnoreCase) ||
            Row->Classification.Contains(_FilterText, ESearchCase::IgnoreCase);
        const auto MatchesMode = _FilterMode == 0 || (_FilterMode == 1 && Row->Audit.IsSet() && Row->Audit->_bWouldUseHeuristic)
            || (_FilterMode == 2 && Row->Audit.IsSet() && Row->Audit->_bWouldFailBake);
        if (MatchesText && MatchesMode)
        { _VisibleRows.Add(Row); }
    }
    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }
}

auto SCkJoltBakeInspectorWindow::OnSearchChanged(const FText& InText) -> void
{
    _FilterText = InText.ToString();
    RefilterRows();
}

auto SCkJoltBakeInspectorWindow::OnSelectionChanged(FRowPtr InRow, ESelectInfo::Type InSelection) -> void
{
    if (InSelection == ESelectInfo::Direct) { return; }
    _SelectedRow = MoveTemp(InRow);
    AnalyzeSelectedRow();
    if (_Preview.IsValid())
    {
        if (_SelectedRow.IsValid() && _SelectedRow->Audit.IsSet()) { _Preview->Show_Audit(_SelectedRow->Audit.GetValue()); }
        else { _Preview->Clear(); }
    }
}

auto SCkJoltBakeInspectorWindow::AnalyzeSelectedRow() -> void
{
    if (NOT _SelectedRow.IsValid() || _SelectedRow->Audit.IsSet())
    { return; }
    AnalyzeRow(_SelectedRow);
}

auto SCkJoltBakeInspectorWindow::AnalyzeRow(const FRowPtr& InRow) -> void
{
    if (NOT InRow.IsValid() || InRow->Audit.IsSet()) { return; }
    const TStrongObjectPtr<UStaticMesh> Mesh{Cast<UStaticMesh>(InRow->Asset.GetAsset())};
    if (NOT Mesh.IsValid())
    {
        InRow->Classification = TEXT("Could not load source mesh");
        InRow->Detail = TEXT("The Asset Registry entry no longer resolves to a UStaticMesh.");
        return;
    }

    // Shared CkJoltEditor inspection is explicitly mutation-free and owns the same classification semantics as the
    // cooker. This tab deliberately does not calculate a parallel freshness or winding verdict.
    InRow->Audit = ck::jolt::cook::Analyze_MeshShape(*Mesh);
    const auto& Audit = InRow->Audit.GetValue();
    InRow->Classification = ck_jolt_bake_inspector_window::ToText(Audit._RecommendedAction);
    InRow->Detail = ck::Format_UE(
        TEXT("Source: {}\nCooked: {}\nSource winding: {} ({})\nNormalized winding: {} ({})\nCooked winding: {} ({})\n"
             "Cooked preview: {}\nRepairs: {} individual, {} aggregate no-verdict\nHeuristic: {}\nWould fail bake: {}\n{}"),
        ck_jolt_bake_inspector_window::ToText(Audit._SourceState),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedState),
        ck_jolt_bake_inspector_window::ToText(Audit._SourceWinding), Audit._SourceWindingRatio,
        ck_jolt_bake_inspector_window::ToText(Audit._NormalizedSourceWinding), Audit._NormalizedSourceWindingRatio,
        ck_jolt_bake_inspector_window::ToText(Audit._CookedWinding), Audit._CookedWindingRatio,
        Audit._bCookedPreviewUnavailable ? TEXT("unavailable")
        : Audit._bCookedPreviewTruncated ? TEXT("truncated") : TEXT("available"),
        Audit._IndividualHeuristicRepairCount, Audit._AggregateHeuristicRepairCount,
        Audit._bWouldUseHeuristic ? TEXT("yes") : TEXT("no"),
        Audit._bWouldFailBake ? TEXT("yes") : TEXT("no"),
        Audit._Failure);
}

auto SCkJoltBakeInspectorWindow::StartAnalyzeAll() -> FReply
{
    CancelAnalysis();
    for (const auto& Row : _AllRows) { if (NOT Row->Audit.IsSet()) { _AnalysisQueue.Add(Row); } }
    _AnalysisState.Start(_AnalysisQueue.Num());
    if (GetIsAnalyzing()) { _AnalysisJoltLease = MakeUnique<ck::jolt::FCk_Jolt_ScopedGlobalInit>(); }
    return FReply::Handled();
}
auto SCkJoltBakeInspectorWindow::CancelAnalyzeAll() -> FReply { CancelAnalysis(); return FReply::Handled(); }
auto SCkJoltBakeInspectorWindow::CancelAnalysis() -> void { _AnalysisQueue.Reset(); _AnalysisState.Cancel(); _AnalysisJoltLease.Reset(); }
auto SCkJoltBakeInspectorWindow::SetFilterMode(int32 InMode) -> FReply { _FilterMode = InMode; RefilterRows(); return FReply::Handled(); }
auto SCkJoltBakeInspectorWindow::GetIsAnalyzing() const -> bool { return _AnalysisState.IsActive(); }

auto SCkJoltBakeInspectorWindow::GenerateRow(FRowPtr InRow, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    return SNew(STableRow<FRowPtr>, InOwnerTable)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([InRow] { return FText::FromString(InRow->DisplayName); })]
        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([InRow]
            { return FText::FromString(ck::Format_UE(TEXT("{} — {}"), InRow->Classification, InRow->PackagePath)); })]
    ];
}

auto SCkJoltBakeInspectorWindow::BrowseSelectedAsset() -> FReply
{
    if (NOT _SelectedRow.IsValid() || GEditor == nullptr)
    { return FReply::Handled(); }
    const TStrongObjectPtr<UObject> Asset{_SelectedRow->Asset.GetAsset()};
    if (Asset.IsValid())
    { GEditor->SyncBrowserToObjects(TArray<UObject*>{Asset.Get()}); }
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::OpenSelectedAsset() -> FReply
{
    // Refresh is deliberately an explicit command only. It never runs from Tick, filtering, or attributes.
    if (NOT _SelectedRow.IsValid())
    { return FReply::Handled(); }
    if (GEditor != nullptr)
    {
        const TStrongObjectPtr<UObject> Asset{_SelectedRow->Asset.GetAsset()};
        if (Asset.IsValid())
        {
            const TWeakObjectPtr<UAssetEditorSubsystem> Editors{GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()};
            if (Editors.IsValid())
            { Editors->OpenEditorForAsset(Asset.Get()); }
        }
    }
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::BakeSelected() -> FReply
{
    if (NOT _SelectedRow.IsValid() || GEditor == nullptr)
    { return FReply::Handled(); }

    const TWeakObjectPtr<UCk_JoltCook_EditorSubsystem_UE> Cooker{
        GEditor->GetEditorSubsystem<UCk_JoltCook_EditorSubsystem_UE>()};
    if (Cooker.IsValid())
    {
        // Explicit mutation only. The subsystem owns slicing, save policy, and completion; this window does not
        // reimplement any cook/write path and deliberately does not poll to infer completion.
        Cooker->Request_CookMeshShapes_ForAssets(TArray<FAssetData>{_SelectedRow->Asset});
    }
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::BakeAll() -> FReply
{
    if (GEditor == nullptr)
    { return FReply::Handled(); }

    const TWeakObjectPtr<UCk_JoltCook_EditorSubsystem_UE> Cooker{
        GEditor->GetEditorSubsystem<UCk_JoltCook_EditorSubsystem_UE>()};
    auto Repairable = TArray<FAssetData>{};
    for (const auto& Row : _AllRows)
    {
        if (Row->Audit.IsSet() && ck::jolt_bake_inspector::Get_IsRepairableBakeAction(
            Row->Audit->_RecommendedAction, Row->Audit->_bWouldFailBake))
        { Repairable.Add(Row->Asset); }
    }
    if (Cooker.IsValid() && NOT Repairable.IsEmpty())
    { Cooker->Request_CookMeshShapes_ForAssets(Repairable); }
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::CanBakeSelected() const -> bool
{
    if (NOT _SelectedRow.IsValid() || NOT _SelectedRow->Audit.IsSet() || _SelectedRow->Audit->_bWouldFailBake || GEditor == nullptr)
    { return false; }

    return ck::jolt_bake_inspector::Get_IsRepairableBakeAction(
        _SelectedRow->Audit->_RecommendedAction, _SelectedRow->Audit->_bWouldFailBake);
}

auto SCkJoltBakeInspectorWindow::CanBakeAll() const -> bool
{
    return GEditor != nullptr && _AllRows.ContainsByPredicate([](const FRowPtr& Row)
    {
        return Row->Audit.IsSet() && ck::jolt_bake_inspector::Get_IsRepairableBakeAction(
            Row->Audit->_RecommendedAction, Row->Audit->_bWouldFailBake);
    });
}

auto SCkJoltBakeInspectorWindow::GetSummaryText() const -> FText
{
    auto NumAnalyzed = 0;
    auto NumWouldFail = 0;
    auto NumHeuristic = 0;
    for (const auto& Row : _AllRows)
    {
        if (NOT Row->Audit.IsSet()) { continue; }
        ++NumAnalyzed;
        NumWouldFail += Row->Audit->_bWouldFailBake ? 1 : 0;
        NumHeuristic += Row->Audit->_bWouldUseHeuristic ? 1 : 0;
    }
    return FText::FromString(ck::Format_UE(TEXT("{} baked-root meshes; {} shown; {} analyzed; {} would fail; {} repair heuristics"),
        _AllRows.Num(), _VisibleRows.Num(), NumAnalyzed, NumWouldFail, NumHeuristic)
        + (GetIsAnalyzing() ? ck::Format_UE(TEXT("; analyzing {}/{}"), _AnalysisState.Get_Processed(), _AnalysisState.Get_Total()) : FString{}));
}

auto SCkJoltBakeInspectorWindow::GetSelectedDetailText() const -> FText
{
    if (NOT _SelectedRow.IsValid())
    { return FText::FromString(TEXT("Select a mesh. Preview and the shared read-only bake audit land in the selected-row panel.")); }
    const auto CookedPath = _SelectedRow->Audit.IsSet() ? _SelectedRow->Audit->_CookedShapeObjectPath : TEXT("pending audit");
    return FText::FromString(ck::Format_UE(TEXT("Source package: {}\nCooked shape: {}\n\n{}\n\n"
        "Preview: source collision is red on the left; the normalized Jolt candidate is cyan in the center; "
        "the current cooked Jolt shape is green on the right. Preview triangles are capped by the shared audit and "
        "marked when truncated or unavailable."),
        _SelectedRow->PackagePath, CookedPath, _SelectedRow->Detail));
}
