#include "CkJoltBakeInspector/Viewport/SCkJoltBakeInspectorPreview.h"

#include "CkJoltBakeInspector/Viewport/CkJoltBakeInspectorPreviewAdapter.h"

#include "CkDebugScene/CkDebugScene_Target.h"
#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

auto SCkJoltBakeInspectorPreview::Construct(const FArguments&) -> void
{
    _Adapter = MakeShared<FCkJoltBakeInspectorPreviewAdapter>();
    const auto InformationalOverlay = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Left)
        [ SAssignNew(_SourceLabel, STextBlock).ColorAndOpacity(FLinearColor{0.90f, 0.20f, 0.20f}).Text(FText::FromString(TEXT("SOURCE"))) ]
        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
        [ SAssignNew(_NormalizedLabel, STextBlock).ColorAndOpacity(FLinearColor{0.15f, 0.75f, 0.85f}).Text(FText::FromString(TEXT("NORMALIZED JOLT CANDIDATE"))) ]
        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right)
        [ SAssignNew(_CookedLabel, STextBlock).ColorAndOpacity(FLinearColor{0.30f, 0.90f, 0.35f}).Text(FText::FromString(TEXT("CURRENT COOKED: MISSING"))) ];
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
    if (NOT _Adapter.IsValid() || NOT _Viewport.IsValid()) { return; }
    _Adapter->Reconcile(InAudit);
    if (_SourceLabel.IsValid())
    { _SourceLabel->SetText(FText::FromString(InAudit._bSourcePreviewTruncated ? TEXT("SOURCE (TRUNCATED)") : TEXT("SOURCE"))); }
    if (_NormalizedLabel.IsValid())
    { _NormalizedLabel->SetText(FText::FromString(InAudit._bNormalizedPreviewTruncated ? TEXT("NORMALIZED JOLT CANDIDATE (TRUNCATED)") : TEXT("NORMALIZED JOLT CANDIDATE"))); }
    if (_CookedLabel.IsValid())
    {
        _CookedLabel->SetText(FText::FromString(ck::jolt_bake_inspector::Get_CookedPreviewLabel(InAudit)));
    }
    _Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll);
}

auto SCkJoltBakeInspectorPreview::Clear() -> void
{
    if (_Adapter.IsValid()) { _Adapter->Reset(); }
    if (_SourceLabel.IsValid()) { _SourceLabel->SetText(FText::FromString(TEXT("SOURCE"))); }
    if (_NormalizedLabel.IsValid()) { _NormalizedLabel->SetText(FText::FromString(TEXT("NORMALIZED JOLT CANDIDATE"))); }
    if (_CookedLabel.IsValid())
    { _CookedLabel->SetText(FText::FromString(TEXT("CURRENT COOKED: MISSING"))); }
}

auto SCkJoltBakeInspectorPreview::Get_PreviewWorld() const -> UWorld*
{ return _Viewport.IsValid() ? _Viewport->Get_PreviewWorld() : nullptr; }

auto SCkJoltBakeInspectorPreview::Get_RenderedBounds() const -> FBox
{ return _Adapter.IsValid() ? _Adapter->Get_FrameBounds(ECkDebug3dFrameTarget::All) : FBox{ForceInit}; }

auto SCkJoltBakeInspectorPreview::Teardown() -> void
{
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
