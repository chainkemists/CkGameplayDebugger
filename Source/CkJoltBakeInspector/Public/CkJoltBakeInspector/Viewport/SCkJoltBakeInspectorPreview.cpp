#include "CkJoltBakeInspector/Viewport/SCkJoltBakeInspectorPreview.h"

#include "CkJoltBakeInspector/Viewport/CkJoltBakeInspectorPreviewAdapter.h"

#include "CkDebugScene/CkDebugScene_Target.h"
#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_jolt_bake_inspector_preview
{
    constexpr double LegendPollIntervalSeconds{0.5};
    const FText SourceLabel{FText::FromString(TEXT("SOURCE"))};
    const FText NormalizedLabel{FText::FromString(TEXT("NORMALIZED JOLT CANDIDATE"))};
    const FText MissingCookedLabel{FText::FromString(TEXT("CURRENT COOKED: MISSING"))};

    auto LegendStyleTokens() -> FCkUiView::FTokens
    {
        auto Tokens = FCkUiView::FTokens{};
        // These semantic colors identify source, normalized candidate and cooked preview geometry, not UI severity.
        Tokens.Add(TEXT("--jolt-bake-legend-source"), TEXT("#") + FLinearColor{0.90f, 0.20f, 0.20f}.ToFColorSRGB().ToHex());
        Tokens.Add(TEXT("--jolt-bake-legend-normalized"), TEXT("#") + FLinearColor{0.15f, 0.75f, 0.85f}.ToFColorSRGB().ToHex());
        Tokens.Add(TEXT("--jolt-bake-legend-cooked"), TEXT("#") + FLinearColor{0.30f, 0.90f, 0.35f}.ToFColorSRGB().ToHex());
        Tokens.Add(TEXT("--jolt-bake-legend-font-size"), FString::FromInt(CkStyle::FontSizeBody()));
        return Tokens;
    }
}

auto SCkJoltBakeInspectorPreview::Construct(const FArguments&) -> void
{
    _Adapter = MakeShared<FCkJoltBakeInspectorPreviewAdapter>();
    _SourceLegendText = ck_jolt_bake_inspector_preview::SourceLabel;
    _NormalizedLegendText = ck_jolt_bake_inspector_preview::NormalizedLabel;
    _CookedLegendText = ck_jolt_bake_inspector_preview::MissingCookedLabel;
    const TSharedRef<SWidget> InformationalOverlay = Build_AuthoredLegend();
    _Viewport = SNew(SCkDebug_3dPreviewViewport)
        .Descriptor(FCkDebug3dPreviewDescriptor{})
        .Adapter(_Adapter)
        .SafeAreaOverlay(InformationalOverlay);

    auto Config = FCk_DebugScene_TargetConfig{};
    Config.Set_World(_Viewport->Get_PreviewWorld()).Set_MaxItems(3).Set_MaxInstances(3);
    _Target = MakeShared<FCk_DebugScene_Target>(Config);
    _Adapter->Set_Target(_Target);

    ChildSlot[_Viewport.ToSharedRef()];
}

SCkJoltBakeInspectorPreview::~SCkJoltBakeInspectorPreview()
{ Teardown(); }

auto SCkJoltBakeInspectorPreview::Show_Audit(const ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult& InAudit) -> void
{
    if (_Released || NOT _Adapter.IsValid() || NOT _Viewport.IsValid()) { return; }
    _Adapter->Reconcile(InAudit);
    _SourceLegendText = FText::FromString(InAudit._bSourcePreviewTruncated ? TEXT("SOURCE (TRUNCATED)") : TEXT("SOURCE"));
    _NormalizedLegendText = FText::FromString(InAudit._bNormalizedPreviewTruncated ? TEXT("NORMALIZED JOLT CANDIDATE (TRUNCATED)") : TEXT("NORMALIZED JOLT CANDIDATE"));
    _CookedLegendText = FText::FromString(ck::jolt_bake_inspector::Get_CookedPreviewLabel(InAudit));
    Refresh_NativeLegendFallback();
    _Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll);
}

auto SCkJoltBakeInspectorPreview::Clear() -> void
{
    if (_Released) { return; }
    if (_Adapter.IsValid()) { _Adapter->Reset(); }
    _SourceLegendText = ck_jolt_bake_inspector_preview::SourceLabel;
    _NormalizedLegendText = ck_jolt_bake_inspector_preview::NormalizedLabel;
    _CookedLegendText = ck_jolt_bake_inspector_preview::MissingCookedLabel;
    Refresh_NativeLegendFallback();
}

auto SCkJoltBakeInspectorPreview::Get_PreviewWorld() const -> UWorld*
{ return _Viewport.IsValid() ? _Viewport->Get_PreviewWorld() : nullptr; }

auto SCkJoltBakeInspectorPreview::Get_RenderedBounds() const -> FBox
{ return _Adapter.IsValid() ? _Adapter->Get_FrameBounds(ECkDebug3dFrameTarget::All) : FBox{ForceInit}; }

auto SCkJoltBakeInspectorPreview::Teardown() -> void
{
    if (_Released) { return; }
    _Released = true;

    // Views own weak data bindings into this widget; drop them before either preview-world owner.
    _LegendView.Reset();
    _LegendHost.Reset();
    _SourceFallbackLabel.Reset();
    _NormalizedFallbackLabel.Reset();
    _CookedFallbackLabel.Reset();
    // FCk_DebugScene_Target owns registered preview-world components. Release it before Common destroys that world.
    if (_Adapter.IsValid())
    {
        _Adapter->Reset();
        _Adapter->Set_Target({});
    }
    _Target.Reset();
    if (_Viewport.IsValid()) { _Viewport->Teardown(); }
    _Viewport.Reset();
    _Adapter.Reset();
}

auto SCkJoltBakeInspectorPreview::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (_Released || NOT _LegendView.IsValid() || InCurrentTime < _NextLegendPollSeconds) { return; }
    _NextLegendPollSeconds = InCurrentTime + ck_jolt_bake_inspector_preview::LegendPollIntervalSeconds;
    _LegendView->PollFiles(ck_jolt_bake_inspector_preview::LegendStyleTokens());
    if (_LegendView->GetLastResult().Succeeded && _LegendHost.IsValid())
    { _LegendHost->SetContent(_LegendView->GetRegion(TEXT("legend"))); }
}

auto SCkJoltBakeInspectorPreview::Build_AuthoredLegend() -> TSharedRef<SWidget>
{
    _LegendHost = SNew(SBox);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid()) { return Build_NativeLegendFallback(); }

    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkJoltBakeInspectorPreview> WeakPreview{SharedThis(this)};
    Data.Text.Add(TEXT("jolt-bake-legend-source"), TAttribute<FText>::CreateLambda([WeakPreview]()
    {
        const TSharedPtr<SCkJoltBakeInspectorPreview> Preview = WeakPreview.Pin();
        return Preview.IsValid() && NOT Preview->_Released ? Preview->_SourceLegendText : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("jolt-bake-legend-normalized"), TAttribute<FText>::CreateLambda([WeakPreview]()
    {
        const TSharedPtr<SCkJoltBakeInspectorPreview> Preview = WeakPreview.Pin();
        return Preview.IsValid() && NOT Preview->_Released ? Preview->_NormalizedLegendText : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("jolt-bake-legend-cooked"), TAttribute<FText>::CreateLambda([WeakPreview]()
    {
        const TSharedPtr<SCkJoltBakeInspectorPreview> Preview = WeakPreview.Pin();
        return Preview.IsValid() && NOT Preview->_Released ? Preview->_CookedLegendText : FText::GetEmpty();
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPreview]()
    {
        const TSharedPtr<SCkJoltBakeInspectorPreview> Preview = WeakPreview.Pin();
        return Preview.IsValid() && NOT Preview->_Released;
    });

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data));
    const TSharedRef<SWidget> Legend = Candidate->GetRegion(TEXT("legend"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("JoltBakeInspectorLegend.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("JoltBakeInspectorLegend.ui.css")));
    _LegendView = Candidate;
    Candidate->PollFiles(ck_jolt_bake_inspector_preview::LegendStyleTokens());
    if (Candidate->GetLastResult().Succeeded) { _LegendHost->SetContent(Legend); }
    else { _LegendHost->SetContent(Build_NativeLegendFallback()); }
    return _LegendHost.ToSharedRef();
}

auto SCkJoltBakeInspectorPreview::Build_NativeLegendFallback() -> TSharedRef<SWidget>
{
    // Left source, centered normalized, and right cooked labels identify the three preview geometries.
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Left)
        [ SAssignNew(_SourceFallbackLabel, STextBlock).ColorAndOpacity(FLinearColor{0.90f, 0.20f, 0.20f}).Text(_SourceLegendText) ]
        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
        [ SAssignNew(_NormalizedFallbackLabel, STextBlock).ColorAndOpacity(FLinearColor{0.15f, 0.75f, 0.85f}).Text(_NormalizedLegendText) ]
        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right)
        [ SAssignNew(_CookedFallbackLabel, STextBlock).ColorAndOpacity(FLinearColor{0.30f, 0.90f, 0.35f}).Text(_CookedLegendText) ];
}

auto SCkJoltBakeInspectorPreview::Refresh_NativeLegendFallback() -> void
{
    if (_SourceFallbackLabel.IsValid()) { _SourceFallbackLabel->SetText(_SourceLegendText); }
    if (_NormalizedFallbackLabel.IsValid()) { _NormalizedFallbackLabel->SetText(_NormalizedLegendText); }
    if (_CookedFallbackLabel.IsValid()) { _CookedFallbackLabel->SetText(_CookedLegendText); }
}
