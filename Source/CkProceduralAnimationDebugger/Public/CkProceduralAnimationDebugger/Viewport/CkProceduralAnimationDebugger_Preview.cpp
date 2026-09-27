#include "CkProceduralAnimationDebugger/Viewport/CkProceduralAnimationDebugger_Preview.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"

#include "CkDebugScene/CkDebugScene_Materials.h"
#include "CkDebugScene/CkDebugScene_Shapes.h"
#include "CkDebugScene/CkDebugScene_Target.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_debug_preview
{
    const FName Lines{TEXT("ProceduralAnimation.Probes")};
    const FName Labels{TEXT("ProceduralAnimation.Legs")};
    constexpr auto BodyItemKey = uint64{1};
    constexpr auto LegItemKeyBase = uint64{100};
    constexpr auto RootSegmentThickness = 5.0;
    constexpr auto TipSegmentThickness = 3.5;
    const auto BodySize = FVector{60.0, 40.0, 20.0};
    const auto FootOutputSize = FVector{10.0};
    const auto FootPartSize = FVector{14.0, 12.0, 6.0};
    const auto GoalSize = FVector{5.0};
    constexpr auto CandidateCrossHalfSize = 2.0;
    constexpr auto ChosenRingRadius = 10.0;
    constexpr auto ChosenRingSegments = 16;
    constexpr auto PendingContactLength = 80.0;
    constexpr auto ObstructionLength = 60.0;
    constexpr auto FeetPlaneHalfSize = 30.0;

    // The plane the gait fitted through the supporting feet, as a square outline around its point: Ok while fitted, Warn
    // while held.
    auto
        Add_FeetPlaneLines(
            const FCk_ProceduralAnimation_DebugGait& InGait,
            const FVector& InOrigin,
            TArray<FCk_DebugScene_Line>& OutLines)
        -> void
    {
        if (InGait.Get_FeetPlane() == ck::EProceduralGaitFeetPlane::None)
        { return; }

        const auto Color = InGait.Get_FeetPlane() == ck::EProceduralGaitFeetPlane::Fitted ? CkStyle::Ok() : CkStyle::Warn();
        const auto Centre = InGait.Get_FeetPlanePoint() - InOrigin;
        auto Tangent = FVector::ZeroVector;
        auto Bitangent = FVector::ZeroVector;
        InGait.Get_FeetPlaneNormal().GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector).FindBestAxisVectors(Tangent, Bitangent);
        const auto Corners = TArray<FVector, TInlineAllocator<4>>{
            Centre + (Tangent + Bitangent) * FeetPlaneHalfSize, Centre + (Tangent - Bitangent) * FeetPlaneHalfSize,
            Centre - (Tangent + Bitangent) * FeetPlaneHalfSize, Centre - (Tangent - Bitangent) * FeetPlaneHalfSize};
        for (auto Corner = 0; Corner < Corners.Num(); ++Corner)
        { OutLines.Add({Corners[Corner], Corners[(Corner + 1) % Corners.Num()], Color, 2.0f}); }
    }

    auto
        Get_VerdictColor(
            ck::EProceduralFootholdVerdict InVerdict)
        -> FLinearColor
    {
        switch (InVerdict)
        {
            case ck::EProceduralFootholdVerdict::Usable: return CkStyle::Ok();
            case ck::EProceduralFootholdVerdict::Miss: return CkStyle::TextMute();
            case ck::EProceduralFootholdVerdict::Unreachable: return CkStyle::Warn();
            case ck::EProceduralFootholdVerdict::TooSteep: return CkStyle::Info();
            case ck::EProceduralFootholdVerdict::Occluded: return CkStyle::Err();
            case ck::EProceduralFootholdVerdict::Inboard: return CkStyle::Accent();
            case ck::EProceduralFootholdVerdict::UnderBody: return CkStyle::AccentDim();
        }
        return CkStyle::TextMute();
    }

    // Each candidate a solve validated as a cross tinted by its verdict, a ring around the one that became the target and
    // the line from the hip to it.
    auto
        Add_FootholdLines(
            const FCk_ProceduralAnimation_DebugLeg& InLeg,
            const FVector& InOrigin,
            TArray<FCk_DebugScene_Line>& OutLines)
        -> void
    {
        const auto& Footholds = InLeg.Get_Footholds();
        for (const auto& Foothold : Footholds)
        {
            const auto Centre = Foothold.Get_Position() - InOrigin;
            const auto Color = Get_VerdictColor(Foothold.Get_Verdict());
            for (const auto& Axis : {FVector::ForwardVector, FVector::RightVector, FVector::UpVector})
            { OutLines.Add({Centre - Axis * CandidateCrossHalfSize, Centre + Axis * CandidateCrossHalfSize, Color, 2.0f}); }
        }

        if (NOT Footholds.IsValidIndex(InLeg.Get_ChosenFoothold()))
        { return; }

        const auto& Chosen = Footholds[InLeg.Get_ChosenFoothold()];
        const auto Centre = Chosen.Get_Position() - InOrigin;
        auto Tangent = FVector::ZeroVector;
        auto Bitangent = FVector::ZeroVector;
        Chosen.Get_Normal().GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector).FindBestAxisVectors(Tangent, Bitangent);
        for (auto Segment = 0; Segment < ChosenRingSegments; ++Segment)
        {
            const auto From = UE_DOUBLE_TWO_PI * Segment / ChosenRingSegments;
            const auto To = UE_DOUBLE_TWO_PI * (Segment + 1) / ChosenRingSegments;
            OutLines.Add({Centre + (Tangent * FMath::Cos(From) + Bitangent * FMath::Sin(From)) * ChosenRingRadius,
                Centre + (Tangent * FMath::Cos(To) + Bitangent * FMath::Sin(To)) * ChosenRingRadius, CkStyle::Accent(), 2.0f});
        }
        OutLines.Add({InLeg.Get_Targeting().Get_HipWorld() - InOrigin, Centre, CkStyle::Accent(), 1.5f});
    }

    auto
        MakeInstance(
            const FTransform& InPose,
            const FVector& InFullSize,
            FLinearColor InColor,
            uint64 InId)
        -> FCk_DebugScene_Instance
    {
        auto Pose = InPose;
        Pose.SetScale3D(InFullSize);
        auto Appearance = FCk_DebugScene_Appearance{};
        Appearance.Set_BaseMaterial(ck::debug_scene::materials::TryGet_Opaque())
            .Set_RenderClass(ECk_DebugScene_RenderClass::Opaque)
            .Set_Color(InColor);
        return FCk_DebugScene_Instance{}.Set_Mesh(ck::debug_scene::shapes::Get_Box())
            .Set_Transform(Pose)
            .Set_Appearance(Appearance)
            .Set_PickIdentity(InId);
    }

    // Each segment pose sits at the midpoint of its joints, so walking from the hip recovers every joint.
    auto
        Add_ChainInstances(
            const FCk_ProceduralAnimation_DebugLeg& InLeg,
            const FVector& InOrigin,
            FLinearColor InColor,
            uint64 InId,
            TArray<FCk_DebugScene_Instance>& OutInstances)
        -> void
    {
        const auto& Segments = InLeg.Get_Rig().Get_Segments();
        auto Joint = InLeg.Get_Targeting().Get_HipWorld();
        for (auto Index = 0; Index < Segments.Num(); ++Index)
        {
            auto Pose = Segments[Index].Get_Transform();
            const auto NextJoint = Joint + (Pose.GetLocation() - Joint) * 2.0;
            const auto Alpha = Segments.Num() > 1 ? static_cast<double>(Index) / static_cast<double>(Segments.Num() - 1) : 0.0;
            const auto Thickness = FMath::Lerp(RootSegmentThickness, TipSegmentThickness, Alpha);
            const auto Length = FVector::Distance(Joint, NextJoint);
            Pose.AddToTranslation(-InOrigin);
            OutInstances.Add(MakeInstance(Pose, FVector{Length, Thickness, Thickness}, InColor, InId));
            Joint = NextJoint;
        }

        if (InLeg.Get_Rig().Get_Foot().Get_Available())
        {
            auto FootPose = InLeg.Get_Rig().Get_Foot().Get_Transform();
            FootPose.AddToTranslation(-InOrigin);
            OutInstances.Add(MakeInstance(FootPose, FootPartSize, InColor, InId));
        }
    }

    auto
        Get_IsChainDrawable(
            const FCk_ProceduralAnimation_DebugSnapshot& InSample,
            const FCk_ProceduralAnimation_DebugLeg& InLeg)
        -> bool
    {
        return InSample.Get_Freshness().Get_RigMatchesGaitSequence()
            && NOT InSample.Get_Freshness().Get_RigPosePending()
            && InLeg.Get_Rig().Get_Composed()
            && NOT InLeg.Get_Rig().Get_Segments().IsEmpty()
            && NOT InLeg.Get_Rig().Get_Segments().ContainsByPredicate([](const FCk_ProceduralAnimation_DebugPart& InPart)
            {
                return NOT InPart.Get_Available();
            });
    }
}

// --------------------------------------------------------------------------------------------------------------------

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

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Preview::
    Show(
        const FCk_ProceduralAnimation_DebugSnapshot& InSample,
        const FString& InSelectedLegId)
    -> void
{
    if (NOT _Target.IsValid())
    { return; }

    _Sample = InSample;
    _SelectedLegId = InSelectedLegId;
    _SelectedCenter.Reset();

    const auto Origin = InSample.Get_Gait().Get_BodyTransform().GetLocation();
    auto Lines = TArray<FCk_DebugScene_Line>{};
    auto Labels = TArray<FCk_DebugScene_Label>{};
    _Target->Begin_Reconcile();

    auto Body = InSample.Get_Gait().Get_BodyTransform();
    Body.SetLocation(FVector::ZeroVector);
    const auto BodyInstances = TArray<FCk_DebugScene_Instance>{
        ck_procedural_debug_preview::MakeInstance(Body, ck_procedural_debug_preview::BodySize, CkStyle::TextDim(), 0)};
    const auto BodyRetained = _Target->Upsert_Item(ck_procedural_debug_preview::BodyItemKey, BodyInstances);
    CK_ENSURE_IF_NOT(BodyRetained,
        TEXT("Procedural animation preview could not retain the body of [{}]; the preview is cleared."), InSample.Get_EntityId())
    {
        _Target->Abort_Reconcile();
        Reset();
        return;
    }

    Lines.Add({FVector::ZeroVector, InSample.Get_Gait().Get_SupportNormal() * 100.0, CkStyle::Accent(), 3.0f});
    ck_procedural_debug_preview::Add_FeetPlaneLines(InSample.Get_Gait(), Origin, Lines);
    if (InSample.Get_Motion().Get_CandidateSeen() > FCk_Time{})
    {
        Lines.Add({FVector::ZeroVector, InSample.Get_Motion().Get_CandidateNormal() * ck_procedural_debug_preview::PendingContactLength,
            CkStyle::Warn(), 2.0f});
    }
    if (InSample.Get_Motion().Get_Obstruction() == ECk_SurfaceMotion_Obstruction::Wall)
    {
        Lines.Add({FVector::ZeroVector, InSample.Get_Motion().Get_ObstructionNormal() * ck_procedural_debug_preview::ObstructionLength,
            CkStyle::Err(), 2.0f});
    }
    for (auto Index = 0; Index < InSample.Get_Legs().Num(); ++Index)
    {
        const auto& Leg = InSample.Get_Legs()[Index];
        if (Leg.Get_LegEntityId().IsEmpty())
        { continue; }

        const auto Id = static_cast<uint64>(Index + 1);
        const auto IsSelected = Leg.Get_LegEntityId() == InSelectedLegId;
        const auto Color = IsSelected ? CkStyle::Accent()
            : Leg.Get_Enabled() ? ck::debug_axes::Get_CategoricalColor(Index)
            : CkStyle::TextMute();
        const auto ContactColor = Leg.Get_Foot().Get_ContactTrusted() ? CkStyle::Ok() : CkStyle::Warn();
        const auto Hip = Leg.Get_Targeting().Get_HipWorld() - Origin;
        const auto Foot = Leg.Get_Foot().Get_Position() - Origin;

        auto Instances = TArray<FCk_DebugScene_Instance>{};
        Instances.Add(ck_procedural_debug_preview::MakeInstance(FTransform{Leg.Get_Foot().Get_Rotation(), Foot},
            ck_procedural_debug_preview::FootOutputSize, Color, Id));
        Instances.Add(ck_procedural_debug_preview::MakeInstance(FTransform{Leg.Get_Targeting().Get_IdealTarget() - Origin},
            ck_procedural_debug_preview::GoalSize, ContactColor, Id));

        // The snapshot counts the crossing links without naming them, so a crossing chain is tinted whole.
        const auto ChainColor = Leg.Get_Rig().Get_ChainState() == ECk_ProceduralRig_ChainState::Crossing ? CkStyle::Err() : Color;
        if (ck_procedural_debug_preview::Get_IsChainDrawable(InSample, Leg))
        { ck_procedural_debug_preview::Add_ChainInstances(Leg, Origin, ChainColor, Id, Instances); }
        else
        { Lines.Add({Hip, Foot, Color, 2.0f}); }

        const auto LegRetained = _Target->Upsert_Item(ck_procedural_debug_preview::LegItemKeyBase + Id, Instances);
        CK_ENSURE_IF_NOT(LegRetained,
            TEXT("Procedural animation preview could not retain leg [{}] of [{}]; the preview is cleared."),
            Leg.Get_Id(), InSample.Get_EntityId())
        {
            _Target->Abort_Reconcile();
            Reset();
            return;
        }

        Lines.Add({Leg.Get_Probe().Get_Start() - Origin, Leg.Get_Probe().Get_End() - Origin, CkStyle::TextMute(), 1.0f});
        if (Leg.Get_Probe().Get_Hit())
        {
            const auto Hit = Leg.Get_Probe().Get_HitPosition() - Origin;
            Lines.Add({Hit, Hit + Leg.Get_Probe().Get_HitNormal() * 25.0, ContactColor, 3.0f});
        }
        const auto& LandingProbe = Leg.Get_LandingProbe();
        if (LandingProbe.Get_AttemptCount() > 0)
        {
            // A probe that found no ground under the landing point sends the swing back onto its validated target.
            const auto LandingColor = Leg.Get_LandingGround() == ck::EProceduralGaitLandingGround::None ? CkStyle::Warn() : CkStyle::Info();
            Lines.Add({LandingProbe.Get_Start() - Origin, LandingProbe.Get_End() - Origin, LandingColor, 1.0f});
            if (LandingProbe.Get_Hit())
            {
                const auto Hit = LandingProbe.Get_HitPosition() - Origin;
                Lines.Add({Hit, Hit + LandingProbe.Get_HitNormal() * 25.0, LandingColor, 2.0f});
            }
        }
        Lines.Add({Foot, Leg.Get_Targeting().Get_IdealTarget() - Origin, Color, 1.0f});
        if (IsSelected)
        { ck_procedural_debug_preview::Add_FootholdLines(Leg, Origin, Lines); }

        if (_ShowLabels)
        { Labels.Add({Foot + FVector{0.0, 0.0, 12.0}, Leg.Get_Id().ToString(), Color, 1.0f}); }

        if (IsSelected)
        { _SelectedCenter = Foot; }
    }

    const auto Reconciled = _Target->End_Reconcile();
    CK_ENSURE_IF_NOT(Reconciled,
        TEXT("Procedural animation preview could not reconcile the geometry of [{}]; the preview is cleared."),
        InSample.Get_EntityId())
    {
        Reset();
        return;
    }

    _Target->Set_LineChannel(ck_procedural_debug_preview::Lines, MoveTemp(Lines));
    _Target->Set_LabelChannel(ck_procedural_debug_preview::Labels, MoveTemp(Labels));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Preview::
    Reset()
    -> void
{
    _Sample.Reset();
    _SelectedCenter.Reset();
    if (NOT _Target.IsValid())
    { return; }

    _Target->Begin_Reconcile();
    _Target->End_Reconcile();
    _Target->Clear_LineChannel(ck_procedural_debug_preview::Lines);
    _Target->Clear_LabelChannel(ck_procedural_debug_preview::Labels);
}

// --------------------------------------------------------------------------------------------------------------------

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

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Preview::
    Get_SelectionCenter() const
    -> TOptional<FVector>
{
    return _SelectedCenter.IsSet() ? _SelectedCenter : TOptional<FVector>{FVector::ZeroVector};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Preview::
    Get_Capabilities() const
    -> ECkDebug3dViewportCapability
{
    return ECkDebug3dViewportCapability::Labels | ECkDebug3dViewportCapability::FrameSelection;
}

// --------------------------------------------------------------------------------------------------------------------

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

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Preview::
    Select(
        uint64 InIdentity,
        bool)
    -> void
{
    if (NOT _Sample.IsSet() || InIdentity == 0 || InIdentity > static_cast<uint64>(_Sample->Get_Legs().Num()))
    { return; }

    const auto& LegEntityId = _Sample->Get_Legs()[static_cast<int32>(InIdentity - 1)].Get_LegEntityId();
    if (LegEntityId.IsEmpty())
    { return; }

    _OnLegSelected.Broadcast(LegEntityId);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Preview::
    On_Pick(
        const FCkDebug3dCursorRay& InRay)
    -> void
{
    if (const auto Hit = TryHit(InRay); Hit.IsSet())
    { Select(Hit->_Identity, InRay._IsAdditiveSelection); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Preview::
    Set_ShowLabels(
        bool InShow)
    -> void
{
    _ShowLabels = InShow;
    if (NOT _Sample.IsSet())
    { return; }

    const auto Sample = _Sample.GetValue();
    Show(Sample, _SelectedLegId);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Preview::
    On_ViewportTeardown()
    -> void
{
    Reset();
    _Target.Reset();
    _OnLegSelected.Clear();
}

// --------------------------------------------------------------------------------------------------------------------
