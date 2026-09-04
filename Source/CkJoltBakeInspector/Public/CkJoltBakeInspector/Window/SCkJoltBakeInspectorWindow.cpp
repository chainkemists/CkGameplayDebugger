#include "CkJoltBakeInspector/Window/SCkJoltBakeInspectorWindow.h"
#include "CkJoltBakeInspector/CkJoltBakeInspector_Policy.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkJolt/Settings/CkJolt_ProjectSettings.h"
#include "CkJoltEditor/Cook/CkJoltCook_EditorSubsystem.h"
#include "CkJoltEditor/Cook/CkJoltCook_MeshShapeAudit.h"
#include "CkJoltBakeInspector/Viewport/SCkJoltBakeInspectorPreview.h"
#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkEditorTools/Style/CkStyle.h"

#include <AssetRegistry/AssetRegistryModule.h>
#include <AssetRegistry/IAssetRegistry.h>
#include <Editor.h>
#include <Engine/StaticMesh.h>
#include <Subsystems/AssetEditorSubsystem.h>
#include <UObject/StrongObjectPtr.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SSplitter.h>
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

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedPreviewAvailability InAvailability) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedPreviewAvailability;
        switch (InAvailability)
        {
            case MissingCookedAsset: return TEXT("No cooked asset");
            case IncompatibleStale: return TEXT("Stale blob is incompatible with this Jolt version");
            case CorruptBlob: return TEXT("Cooked blob is corrupt");
            case NonTriMesh: return TEXT("Cooked shape is not a tri-mesh");
            case Available: return TEXT("Available");
        }
        return TEXT("Unknown availability");
    }

    auto GetTone(const FCkJoltBakeInspectorRow& InRow) -> ECk_Tone
    {
        if (NOT InRow.Audit.IsSet())
        {
            return InRow.Classification == TEXT("Not analyzed")
                ? ECk_Tone::Neutral
                : ECk_Tone::Err;
        }
        if (InRow.Audit->_bWouldFailBake)
        { return ECk_Tone::Err; }
        if (InRow.Audit->_bWouldUseHeuristic)
        { return ECk_Tone::Warn; }
        return InRow.Audit->_RecommendedAction == ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction::None
            ? ECk_Tone::Ok
            : ECk_Tone::Info;
    }
}

auto SCkJoltBakeInspectorWindow::Construct(const FArguments&) -> void
{
    Register_WithGate();

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkJoltBakeInspector"))
        .StatusText(this, &SCkJoltBakeInspectorWindow::GetSummaryText)
        .ToolbarContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [ SAssignNew(_SearchBar, SCkDebug_SearchBar).HintText(FText::FromString(TEXT("Filter baked meshes"))).OnSearchTextChanged(this, &SCkJoltBakeInspectorWindow::OnSearchChanged) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Refresh"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::OnRefreshClicked)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Analyze All"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::StartAnalyzeAll)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Cancel"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::CancelAnalyzeAll)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("All"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::SetFilterMode, 0)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Heuristic"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::SetFilterMode, 1)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Would Fail"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::SetFilterMode, 2)) ]
        ]
        .Content()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ SNew(SCkDebug_Card).StripeColor(CkStyle::Info()).BodyPadding(FMargin{CkStyle::SpaceS})
                    [ SNew(SCkDebug_StatPair).Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop).Value_Lambda([this] { return GetMetricText(0); }).Label(FText::FromString(TEXT("Inventory"))) ] ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceXS, 0.0f, CkStyle::SpaceS, 0.0f)
                [ SNew(SCkDebug_Card).StripeColor(CkStyle::Accent()).BodyPadding(FMargin{CkStyle::SpaceS})
                    [ SNew(SCkDebug_StatPair).Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop).Value_Lambda([this] { return GetMetricText(1); }).Label(FText::FromString(TEXT("Analyzed"))) ] ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceXS, 0.0f, CkStyle::SpaceS, 0.0f)
                [ SNew(SCkDebug_Card).StripeColor(CkStyle::Warn()).BodyPadding(FMargin{CkStyle::SpaceS})
                    [ SNew(SCkDebug_StatPair).Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop).Value_Lambda([this] { return GetMetricText(2); }).Label(FText::FromString(TEXT("Heuristic"))) ] ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f)
                [ SNew(SCkDebug_Card).StripeColor(CkStyle::Err()).BodyPadding(FMargin{CkStyle::SpaceS})
                    [ SNew(SCkDebug_StatPair).Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop).Value_Lambda([this] { return GetMetricText(3); }).Label(FText::FromString(TEXT("Would fail"))) ] ]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(CkStyle::SpaceM)
            [
                SNew(SSplitter).Orientation(Orient_Horizontal)
                + SSplitter::Slot().Value(0.42f)
                [
                    SNew(SCkDebug_PaneHost)
                    [
                        SAssignNew(_ListView, SListView<FRowPtr>)
                        .ListItemsSource(&_VisibleRows)
                        .OnGenerateRow(this, &SCkJoltBakeInspectorWindow::GenerateRow)
                        .OnSelectionChanged(this, &SCkJoltBakeInspectorWindow::OnSelectionChanged)
                    ]
                ]
                + SSplitter::Slot().Value(0.58f)
                [
                    SNew(SCkDebug_PaneHost)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().FillHeight(0.48f)
                        [ SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer)[SAssignNew(_Preview, SCkJoltBakeInspectorPreview)] ]
                        + SVerticalBox::Slot().FillHeight(0.52f).Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                        [
                            SNew(SScrollBox)
                            + SScrollBox::Slot()
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight()
                                [ SNew(SCkDebug_InspectorPanel).Title(FText::FromString(TEXT("Bake status"))).Body()[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[SNew(SCkDebug_StatusPill).Text_Lambda([this] { return _SelectedRow.IsValid() ? FText::FromString(_SelectedRow->Classification) : FText::FromString(TEXT("Select mesh")); }).Tone_Lambda([this] { return _SelectedRow.IsValid() ? ck_jolt_bake_inspector_window::GetTone(*_SelectedRow) : ECk_Tone::Neutral; })] + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)[SNew(STextBlock).AutoWrapText(true).Text(this, &SCkJoltBakeInspectorWindow::GetSelectedDiagnosisText)]] ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
                                [ SNew(SCkDebug_InspectorPanel).Title(FText::FromString(TEXT("Source topology"))).Body()[SNew(STextBlock).AutoWrapText(true).Text(this, &SCkJoltBakeInspectorWindow::GetSelectedSourceText)] ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
                                [ SNew(SCkDebug_InspectorPanel).Title(FText::FromString(TEXT("Cooked shape"))).Body()[SNew(STextBlock).AutoWrapText(true).Text(this, &SCkJoltBakeInspectorWindow::GetSelectedCookedText)] ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)[ck_jolt_bake_inspector_window::MakeButton(TEXT("Show in Content Browser"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::BrowseSelectedAsset))]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)[ck_jolt_bake_inspector_window::MakeButton(TEXT("Open Asset"), FOnClicked::CreateSP(this, &SCkJoltBakeInspectorWindow::OpenSelectedAsset))]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)[SNew(SButton).Text(FText::FromString(TEXT("Bake Selected"))).IsEnabled(this, &SCkJoltBakeInspectorWindow::CanBakeSelected).OnClicked(this, &SCkJoltBakeInspectorWindow::BakeSelected)]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)[SNew(SButton).Text(FText::FromString(TEXT("Bake Repairable"))).IsEnabled(this, &SCkJoltBakeInspectorWindow::CanBakeAll).OnClicked(this, &SCkJoltBakeInspectorWindow::BakeAll)]
                                ]
                            ]
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

auto SCkJoltBakeInspectorWindow::OnSearchChanged(const FString& InText) -> void
{
    _FilterText = InText;
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
        TEXT("Source: {}\nCooked: {}\nCooked preview: {}\nRepairs: {} individual, {} aggregate no-verdict\n"
             "Heuristic: {}\nWould fail bake: {}\n{}"),
        ck_jolt_bake_inspector_window::ToText(Audit._SourceState),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedState),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedPreviewAvailability),
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
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [ SNew(STextBlock).Text_Lambda([InRow] { return FText::FromString(InRow->DisplayName); }).Font(CkStyle::BoldFont(CkStyle::FontSizeSmall())) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ SNew(SCkDebug_StatusPill).ShowDot(false).Text_Lambda([InRow] { return FText::FromString(InRow->Classification); }).Tone_Lambda([InRow] { return ck_jolt_bake_inspector_window::GetTone(*InRow); }) ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [ SNew(STextBlock).Text_Lambda([InRow] { return FText::FromString(InRow->PackagePath); }).ColorAndOpacity(CkStyle::TextMute()) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ SNew(SBox).Visibility_Lambda([InRow] { return InRow->Audit.IsSet() && InRow->Audit->_bWouldUseHeuristic ? EVisibility::Visible : EVisibility::Collapsed; })
                [ SNew(SCkDebug_Chip).Text(FText::FromString(TEXT("heuristic"))).Kind(ECkDebug_ChipKind::Neutral).ShowDot(false) ] ]
        ]
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
        if (NOT Row->Audit.IsSet())
        { continue; }
        ++NumAnalyzed;
        NumWouldFail += Row->Audit->_bWouldFailBake ? 1 : 0;
        NumHeuristic += Row->Audit->_bWouldUseHeuristic ? 1 : 0;
    }
    return FText::FromString(ck::Format_UE(TEXT("{} baked-root meshes; {} shown; {} analyzed; {} would fail; {} repair heuristics"),
        _AllRows.Num(), _VisibleRows.Num(), NumAnalyzed, NumWouldFail, NumHeuristic)
         + (GetIsAnalyzing() ? ck::Format_UE(TEXT("; analyzing {}/{}"), _AnalysisState.Get_Processed(), _AnalysisState.Get_Total()) : FString{}));
}

auto SCkJoltBakeInspectorWindow::GetMetricText(int32 InMetric) const -> FText
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

    switch (InMetric)
    {
        case 0: return FText::AsNumber(_AllRows.Num());
        case 1: return FText::AsNumber(NumAnalyzed);
        case 2: return FText::AsNumber(NumHeuristic);
        case 3: return FText::AsNumber(NumWouldFail);
    }
    return FText::GetEmpty();
}

auto SCkJoltBakeInspectorWindow::GetSelectedSourceText() const -> FText
{
    if (NOT _SelectedRow.IsValid())
    { return FText::FromString(TEXT("Select a mesh to inspect its source collision.")); }
    if (NOT _SelectedRow->Audit.IsSet())
    { return FText::FromString(TEXT("Source audit is pending for this mesh.")); }

    const auto& Audit = _SelectedRow->Audit.GetValue();
    return FText::FromString(ck::Format_UE(
        TEXT("Package: {}\nState: {}\nTriangles: {}\nSource winding: {} ({})\nNormalized winding: {} ({})\n"
             "Repairs: {} individual, {} aggregate no-verdict"),
        _SelectedRow->PackagePath,
        ck_jolt_bake_inspector_window::ToText(Audit._SourceState),
        Audit._SourceTriangleCount,
        ck_jolt_bake_inspector_window::ToText(Audit._SourceWinding), Audit._SourceWindingRatio,
        ck_jolt_bake_inspector_window::ToText(Audit._NormalizedSourceWinding), Audit._NormalizedSourceWindingRatio,
        Audit._IndividualHeuristicRepairCount, Audit._AggregateHeuristicRepairCount));
}

auto SCkJoltBakeInspectorWindow::GetSelectedCookedText() const -> FText
{
    if (NOT _SelectedRow.IsValid())
    { return FText::FromString(TEXT("Select a mesh to inspect its generated Jolt shape.")); }
    if (NOT _SelectedRow->Audit.IsSet())
    { return FText::FromString(TEXT("Cooked-shape audit is pending for this mesh.")); }

    const auto& Audit = _SelectedRow->Audit.GetValue();
    return FText::FromString(ck::Format_UE(
        TEXT("Asset: {}\nState: {}\nPreview: {}{}\nCooked winding: {} ({})"),
        Audit._CookedShapeObjectPath,
        ck_jolt_bake_inspector_window::ToText(Audit._CookedState),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedPreviewAvailability),
        Audit._bCookedPreviewTruncated ? TEXT(" (triangle cap reached)") : TEXT(""),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedWinding), Audit._CookedWindingRatio));
}

auto SCkJoltBakeInspectorWindow::GetSelectedDiagnosisText() const -> FText
{
    if (NOT _SelectedRow.IsValid())
    { return FText::FromString(TEXT("Select a mesh. The preview and read-only audit appear here.")); }
    if (NOT _SelectedRow->Audit.IsSet())
    { return FText::FromString(_SelectedRow->Detail); }

    const auto& Audit = _SelectedRow->Audit.GetValue();
    const auto FailureSuffix = Audit._Failure.IsEmpty()
        ? FString{}
        : ck::Format_UE(TEXT("\n{}"), Audit._Failure);
    return FText::FromString(ck::Format_UE(TEXT("Recommended action: {}\nHeuristic repair: {}\nWould fail bake: {}{}"),
        _SelectedRow->Classification,
        Audit._bWouldUseHeuristic ? TEXT("yes") : TEXT("no"),
        Audit._bWouldFailBake ? TEXT("yes") : TEXT("no"),
        FailureSuffix));
}
