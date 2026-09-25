#include "CkProceduralAnimationDebugger/Viewport/CkProceduralAnimationDebugger_Preview.h"
#include "CkDebugScene/CkDebugScene_Target.h"
#include "CkDebugScene/CkDebugScene_Shapes.h"
#include "CkDebugScene/CkDebugScene_Materials.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkCore/Format/CkFormat.h"

namespace ck_procedural_debug_preview
{
    const FName Lines{TEXT("ProceduralAnimation.Probes")};
    const FName Labels{TEXT("ProceduralAnimation.Legs")};

    auto MakeInstance(const FTransform& InPose, const FVector& InFullSize, FLinearColor InColor, uint64 InId)
        -> FCk_DebugScene_Instance
    {
        auto Pose = InPose;
        Pose.SetScale3D(InFullSize);
        auto Appearance = FCk_DebugScene_Appearance{};
        Appearance.Set_BaseMaterial(ck::debug_scene::materials::TryGet_Opaque())
            .Set_RenderClass(ECk_DebugScene_RenderClass::Opaque).Set_Color(InColor);
        return FCk_DebugScene_Instance{}.Set_Mesh(ck::debug_scene::shapes::Get_Box())
            .Set_Transform(Pose).Set_Appearance(Appearance).Set_PickIdentity(InId);
    }
}

auto
    FCkProceduralAnimationDebugger_Preview::
    Initialize(
        UWorld* InPreviewWorld)
    -> void

{
    auto Config = FCk_DebugScene_TargetConfig{};
    Config.Set_World(InPreviewWorld).Set_MaxItems(512).Set_MaxInstances(1024);
    _Target = MakeShared<FCk_DebugScene_Target>(Config);
}

auto
    FCkProceduralAnimationDebugger_Preview::
    Show(
        const FCk_ProceduralAnimation_DebugSnapshot& InSample,
        int32 InSelectedLeg)
    -> void

{
    if (NOT _Target.IsValid())
    { return; }
    _Sample = InSample;
    _SelectedLeg = InSelectedLeg;
    _SelectedCenter.Reset();
    const auto Origin = InSample.Get_BodyTransform().GetLocation();
    auto Lines = TArray<FCk_DebugScene_Line>{};
    auto Labels = TArray<FCk_DebugScene_Label>{};
    _Target->Begin_Reconcile();
    auto Body = InSample.Get_BodyTransform();
    Body.SetLocation(FVector::ZeroVector);
    const auto BodyInstances = TArray<FCk_DebugScene_Instance>{ck_procedural_debug_preview::MakeInstance(
        Body, FVector{60.0, 40.0, 20.0}, CkStyle::TextDim(), 0)};
    if (NOT _Target->Upsert_Item(1, BodyInstances))
    { _Target->Abort_Reconcile(); return; }
    Lines.Add({FVector::ZeroVector, InSample.Get_SupportNormal() * 100.0, CkStyle::Accent(), 3.0f});
    for (auto Index = 0; Index < InSample.Get_Legs().Num(); ++Index)
    {
        const auto& Leg = InSample.Get_Legs()[Index];
        const auto Id = static_cast<uint64>(Index + 1);
        const auto Color = Index == InSelectedLeg ? CkStyle::Accent() : ck::debug_axes::Get_CategoricalColor(Index);
        const auto ContactColor = Leg.Get_ContactTrusted() ? CkStyle::Ok() : CkStyle::Warn();
        auto Instances = TArray<FCk_DebugScene_Instance>{};
        const auto Hip = Leg.Get_HipWorld() - Origin;
        const auto Foot = Leg.Get_FootPosition() - Origin;
        Instances.Add(ck_procedural_debug_preview::MakeInstance(FTransform{Leg.Get_FootRotation(), Foot}, FVector{10.0}, Color, Id));
        Instances.Add(ck_procedural_debug_preview::MakeInstance(FTransform{Leg.Get_IdealTarget() - Origin}, FVector{5.0}, ContactColor, Id));
        // These are diagnostic solids around the observed rig poses, not replicas of the user's meshes.
        if (InSample.Get_RigMatchesGaitSequence() && NOT InSample.Get_RigPosePending()
            && Leg.Get_UpperAvailable() && Leg.Get_LowerAvailable())
        {
            auto Upper = Leg.Get_UpperTransform();
            auto Lower = Leg.Get_LowerTransform();
            const auto Joint = Upper.GetLocation() * 2.0 - Leg.Get_HipWorld();
            const auto UpperLength = FVector::Distance(Leg.Get_HipWorld(), Joint);
            const auto LowerLength = FVector::Distance(Joint, Lower.GetLocation()) * 2.0;
            Upper.AddToTranslation(-Origin);
            Lower.AddToTranslation(-Origin);
            Instances.Add(ck_procedural_debug_preview::MakeInstance(Upper, FVector{UpperLength, 5.0, 5.0}, Color, Id));
            Instances.Add(ck_procedural_debug_preview::MakeInstance(Lower, FVector{LowerLength, 4.0, 4.0}, Color, Id));
        }
        else
        { Lines.Add({Hip, Foot, Color, 2.0f}); }
        if (NOT _Target->Upsert_Item(100 + Id, Instances))
        { _Target->Abort_Reconcile(); return; }
        Lines.Add({Leg.Get_ProbeStart() - Origin, Leg.Get_ProbeEnd() - Origin, CkStyle::TextMute(), 1.0f});
        if (Leg.Get_ProbeHit())
        {
            const auto Hit = Leg.Get_ProbeHitPosition() - Origin;
            Lines.Add({Hit, Hit + Leg.Get_ProbeHitNormal() * 25.0, ContactColor, 3.0f});
        }
        Lines.Add({Foot, Leg.Get_IdealTarget() - Origin, Color, 1.0f});
        if (_ShowLabels)
        { Labels.Add({Foot + FVector{0.0, 0.0, 12.0}, Leg.Get_Id().ToString(), Color, 1.0f}); }
        if (Index == InSelectedLeg)
        { _SelectedCenter = Foot; }
    }
    if (NOT _Target->End_Reconcile())
    { return; }
    _Target->Set_LineChannel(ck_procedural_debug_preview::Lines, MoveTemp(Lines));
    _Target->Set_LabelChannel(ck_procedural_debug_preview::Labels, MoveTemp(Labels));
}

auto
    FCkProceduralAnimationDebugger_Preview::
    Reset()
    -> void

{
    _Sample.Reset();
    _SelectedCenter.Reset();
    if (_Target.IsValid())
    {
        _Target->Begin_Reconcile();
        _Target->End_Reconcile();
        _Target->Clear_LineChannel(ck_procedural_debug_preview::Lines);
        _Target->Clear_LabelChannel(ck_procedural_debug_preview::Labels);
    }
}

auto
    FCkProceduralAnimationDebugger_Preview::
    Get_FrameBounds(
        ECkDebug3dFrameTarget InTarget) const
    -> FBox

{
    if (InTarget == ECkDebug3dFrameTarget::Selection && _SelectedCenter.IsSet())
    { return FBox::BuildAABB(_SelectedCenter.GetValue(), FVector{40.0}); }
    return _Target.IsValid() ? _Target->Get_ContentBounds() : FBox{ForceInit};
}
auto
    FCkProceduralAnimationDebugger_Preview::
    Get_SelectionCenter() const
    -> TOptional<FVector>

{ return _SelectedCenter.IsSet() ? _SelectedCenter : TOptional<FVector>{FVector::ZeroVector}; }
auto
    FCkProceduralAnimationDebugger_Preview::
    Get_Capabilities() const
    -> ECkDebug3dViewportCapability

{ return ECkDebug3dViewportCapability::Labels | ECkDebug3dViewportCapability::FrameSelection; }
auto
    FCkProceduralAnimationDebugger_Preview::
    TryHit(
        const FCkDebug3dCursorRay& InRay)
    -> TOptional<FCkDebug3dInteractionHit>

{
    if (NOT _Target.IsValid())
    { return {}; }
    const auto Hit = _Target->TryPick(InRay._Origin, InRay._Direction);
    if (NOT Hit.IsSet() || Hit->Get_PickIdentity() == 0)
    { return {}; }
    return FCkDebug3dInteractionHit{Hit->Get_PickIdentity(), Hit->Get_HitPoint(), Hit->Get_Distance()};
}
auto
    FCkProceduralAnimationDebugger_Preview::
    Select(
        uint64 InIdentity,
        bool)
    -> void

{
    if (_Sample.IsSet() && InIdentity > 0 && InIdentity <= static_cast<uint64>(_Sample->Get_Legs().Num()))
    { _OnLegSelected.Broadcast(static_cast<int32>(InIdentity - 1)); }
}
auto
    FCkProceduralAnimationDebugger_Preview::
    On_Pick(
        const FCkDebug3dCursorRay& InRay)
    -> void

{
    if (const auto Hit = TryHit(InRay); Hit.IsSet())
    { Select(Hit->_Identity, InRay._IsAdditiveSelection); }
}
auto
    FCkProceduralAnimationDebugger_Preview::
    Set_ShowLabels(
        bool InShow)
    -> void

{
    _ShowLabels = InShow;
    if (_Sample.IsSet())
    {
        const auto Sample = _Sample.GetValue();
        Show(Sample, _SelectedLeg);
    }
}
auto
    FCkProceduralAnimationDebugger_Preview::
    On_ViewportTeardown()
    -> void

{
    Reset();
    _Target.Reset();
    _OnLegSelected.Clear();
}
