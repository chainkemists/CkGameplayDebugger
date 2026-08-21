#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dSceneAdapter.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkDebugScene/CkDebugScene_Materials.h"

namespace ck_crowd_debugger_3d_scene_adapter
{
constexpr auto IsTransparent = true;
constexpr auto PathNetworkSortPriority = 100;

const auto PathChannel = FName{TEXT("CrowdSelectedPath")};
const auto VelocityChannel = FName{TEXT("CrowdVelocity")};
const auto RibbonOutlineChannel = FName{TEXT("CrowdPathNetworkOutlines")};
const auto QueueOriginChannel = FName{TEXT("CrowdQueues.Origins")};
const auto QueueFormationChannel = FName{TEXT("CrowdQueues.Formation")};
const auto QueueMemberLinkChannel = FName{TEXT("CrowdQueues.MemberLinks")};

auto
IsFinite(const FVector& InValue) -> bool
{
    return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z);
}

auto
AddBoxTriangles(const FBox& InBox, TArray<FCk_DebugScene_Triangle>& Out) -> void
{
    if (InBox.IsValid == 0)
    {
        return;
    }
    const auto A = FVector{InBox.Min.X, InBox.Min.Y, InBox.Min.Z};
    const auto B = FVector{InBox.Max.X, InBox.Min.Y, InBox.Min.Z};
    const auto C = FVector{InBox.Max.X, InBox.Max.Y, InBox.Min.Z};
    const auto D = FVector{InBox.Min.X, InBox.Max.Y, InBox.Min.Z};
    const auto E = FVector{InBox.Min.X, InBox.Min.Y, InBox.Max.Z};
    const auto F = FVector{InBox.Max.X, InBox.Min.Y, InBox.Max.Z};
    const auto G = FVector{InBox.Max.X, InBox.Max.Y, InBox.Max.Z};
    const auto H = FVector{InBox.Min.X, InBox.Max.Y, InBox.Max.Z};
    for (const auto& T : {FCk_DebugScene_Triangle{A, C, B},
                          {A, D, C},
                          {E, F, G},
                          {E, G, H},
                          {A, B, F},
                          {A, F, E},
                          {B, C, G},
                          {B, G, F},
                          {C, D, H},
                          {C, H, G},
                          {D, A, E},
                          {D, E, H}})
    {
        Out.Add(T);
    }
}

auto
Get_RibbonSegmentLateral(const FVector& InFrom, const FVector& InTo) -> FVector
{
    const auto Tangent = (InTo - InFrom).GetSafeNormal();
    auto Lateral = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal();
    if (Lateral.IsNearlyZero())
    {
        Lateral = FVector::CrossProduct(FVector::ForwardVector, Tangent).GetSafeNormal();
    }
    return Lateral;
}

auto
Get_RibbonOffset(const TArray<FVector>& InPoints, int32 InPointIndex, float InHalfWidth) -> FVector
{
    const auto HalfWidth = FMath::Max(InHalfWidth, 0.0f);
    if (HalfWidth <= UE_SMALL_NUMBER || NOT InPoints.IsValidIndex(InPointIndex))
    {
        return FVector::ZeroVector;
    }
    const auto Incoming = InPointIndex > 0
                              ? Get_RibbonSegmentLateral(InPoints[InPointIndex - 1], InPoints[InPointIndex])
                              : FVector::ZeroVector;
    const auto Outgoing = InPointIndex + 1 < InPoints.Num()
                              ? Get_RibbonSegmentLateral(InPoints[InPointIndex], InPoints[InPointIndex + 1])
                              : FVector::ZeroVector;
    if (Incoming.IsNearlyZero() && Outgoing.IsNearlyZero())
    {
        return FVector::RightVector * HalfWidth;
    }
    if (Incoming.IsNearlyZero())
    {
        return Outgoing * HalfWidth;
    }
    if (Outgoing.IsNearlyZero())
    {
        return Incoming * HalfWidth;
    }
    auto Miter = (Incoming + Outgoing).GetSafeNormal();
    if (Miter.IsNearlyZero())
    {
        Miter = Outgoing;
    }
    const auto Denominator = FMath::Max(FMath::Abs(FVector::DotProduct(Miter, Outgoing)), 0.5);
    return Miter * FMath::Min(HalfWidth / Denominator, HalfWidth * 2.0f);
}

auto
IsRibbonInputValid(const FCkCrowdDebugger_3dRibbonSnapshot& InRibbon) -> bool
{
    if (InRibbon._Points.Num() < 2 || InRibbon._Points.Num() != InRibbon._HalfWidths.Num())
    {
        return false;
    }
    for (const auto& Point : InRibbon._Points)
    {
        if (NOT IsFinite(Point))
        {
            return false;
        }
    }
    for (const auto HalfWidth : InRibbon._HalfWidths)
    {
        if (NOT FMath::IsFinite(HalfWidth))
        {
            return false;
        }
    }
    return true;
}

auto
AddRibbonTriangles(const FCkCrowdDebugger_3dRibbonSnapshot& InRibbon, TArray<FCk_DebugScene_Triangle>& Out) -> bool
{
    if (NOT IsRibbonInputValid(InRibbon))
    {
        return false;
    }
    auto Left = TArray<FVector>{};
    auto Right = TArray<FVector>{};
    Left.Reserve(InRibbon._Points.Num());
    Right.Reserve(InRibbon._Points.Num());
    for (auto Index = 0; Index < InRibbon._Points.Num(); ++Index)
    {
        const auto Offset = Get_RibbonOffset(InRibbon._Points, Index, InRibbon._HalfWidths[Index]);
        Left.Add(InRibbon._Points[Index] + Offset);
        Right.Add(InRibbon._Points[Index] - Offset);
    }
    for (auto Index = 0; Index + 1 < Left.Num(); ++Index)
    {
        Out.Append({{Left[Index], Left[Index + 1], Right[Index + 1]}, {Left[Index], Right[Index + 1], Right[Index]}});
    }
    return true;
}

auto
AppendTwoSidedTriangle(
    const FCk_DebugScene_Triangle& InTriangle,
    TArray<FCk_DebugScene_Triangle>& OutTriangles) -> bool
{
    const auto IsFiniteTriangle = IsFinite(InTriangle._A) && IsFinite(InTriangle._B) && IsFinite(InTriangle._C);
    const auto HasArea =
        FVector::CrossProduct(InTriangle._B - InTriangle._A, InTriangle._C - InTriangle._A).SizeSquared() >
        SMALL_NUMBER;
    if (NOT IsFiniteTriangle || NOT HasArea)
    {
        return false;
    }
    OutTriangles.Add(InTriangle);
    OutTriangles.Add({InTriangle._A, InTriangle._C, InTriangle._B});
    return true;
}
} // namespace ck_crowd_debugger_3d_scene_adapter

auto
    FCkCrowdDebugger_3dSceneAdapter::
    MakeItemKey(ECkCrowdDebugger_3dSceneRole InRole, uint64 InIdentity)
    -> uint64
{
    const auto Identity =
        FString::Printf(TEXT("%d:%llu"), static_cast<int32>(InRole), static_cast<unsigned long long>(InIdentity));
    if (const auto* Existing = _InternalItemKeys.Find(Identity))
    {
        return *Existing;
    }

    const auto Key = _NextItemKey++;
    _InternalItemKeys.Add(Identity, Key);
    return Key;
}

auto
    FCkCrowdDebugger_3dSceneAdapter::
    MakeAppearance(FLinearColor InColor, bool InTransparent, ECk_DebugScene_DepthPriority InDepthPriority,
                   int32 InTranslucencySortPriority) const
    -> FCk_DebugScene_Appearance
{
    return FCk_DebugScene_Appearance{}
        .Set_BaseMaterial(InTransparent ? ck::debug_scene::materials::TryGet_Translucent()
                                        : ck::debug_scene::materials::TryGet_Opaque())
        .Set_RenderClass(InTransparent ? ECk_DebugScene_RenderClass::Transparent : ECk_DebugScene_RenderClass::Opaque)
        .Set_RenderClassId(InTransparent ? 2 : 1)
        .Set_Color(InColor)
        .Set_DepthPriority(InDepthPriority)
        .Set_TranslucencySortPriority(InTranslucencySortPriority);
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    GetOrCreateCapsuleMesh()
    -> TSharedPtr<FCk_DebugScene_Mesh>
{
    if (_CapsuleMesh.IsValid())
    {
        return _CapsuleMesh;
    }
    auto Triangles = TArray<FCk_DebugScene_Triangle>{};
    constexpr auto Sides = 8;
    const auto RingPoint = [](int32 InSide, float InZ, float InRadius) -> FVector
    {
        constexpr auto RingSides = 8;
        const auto Angle = 2.0f * PI * static_cast<float>(InSide) / static_cast<float>(RingSides);
        return FVector{FMath::Cos(Angle) * InRadius, FMath::Sin(Angle) * InRadius, InZ};
    };
    // Four rings approximate a unit capsule: hemispherical caps at z=0 and z=1, with a cylindrical middle.
    for (auto Side = 0; Side < Sides; ++Side)
    {
        const auto Next = (Side + 1) % Sides;
        const auto Bottom = FVector{0.0f, 0.0f, 0.0f};
        const auto Top = FVector{0.0f, 0.0f, 1.0f};
        const auto LowerA = RingPoint(Side, 0.15f, 1.0f);
        const auto LowerB = RingPoint(Next, 0.15f, 1.0f);
        const auto UpperA = RingPoint(Side, 0.85f, 1.0f);
        const auto UpperB = RingPoint(Next, 0.85f, 1.0f);
        Triangles.Append(
            {{Bottom, LowerB, LowerA}, {LowerA, LowerB, UpperB}, {LowerA, UpperB, UpperA}, {UpperA, UpperB, Top}});
    }
    _CapsuleMesh = FCk_DebugScene_Mesh::Create_FromTriangles(MoveTemp(Triangles));
    return _CapsuleMesh;
}

auto
    FCkCrowdDebugger_3dSceneAdapter::
    GetOrCreateBoxMesh()
    -> TSharedPtr<FCk_DebugScene_Mesh>
{
    if (_BoxMesh.IsValid())
    {
        return _BoxMesh;
    }
    auto Triangles = TArray<FCk_DebugScene_Triangle>{};
    ck_crowd_debugger_3d_scene_adapter::AddBoxTriangles(FBox{FVector{-0.5}, FVector{0.5}}, Triangles);
    _BoxMesh = FCk_DebugScene_Mesh::Create_FromTriangles(MoveTemp(Triangles));
    return _BoxMesh;
}

auto
    FCkCrowdDebugger_3dSceneAdapter::
    SubmitLines(const FCkCrowdDebugger_3dSceneSnapshot& InSnapshot,
        FCk_DebugScene_Target& InTarget) const
    -> bool
{
    auto Paths = TArray<FCk_DebugScene_Line>{};
    auto Velocities = TArray<FCk_DebugScene_Vector>{};
    auto RibbonOutlines = TArray<FCk_DebugScene_Line>{};
    auto VoxelLines = TArray<FCk_DebugScene_Line>{};
    auto QueueOriginLines = TArray<FCk_DebugScene_Line>{};
    auto QueueFormationLines = TArray<FCk_DebugScene_Line>{};
    auto QueueMemberLinks = TArray<FCk_DebugScene_Line>{};
    const auto AddBox = [&VoxelLines](const FBox& InBounds, const FLinearColor& InColor, float InThickness)
    {
        if (InBounds.IsValid == 0)
        {
            return;
        }
        const FVector Corners[8] = {
            {InBounds.Min.X, InBounds.Min.Y, InBounds.Min.Z}, {InBounds.Max.X, InBounds.Min.Y, InBounds.Min.Z},
            {InBounds.Max.X, InBounds.Max.Y, InBounds.Min.Z}, {InBounds.Min.X, InBounds.Max.Y, InBounds.Min.Z},
            {InBounds.Min.X, InBounds.Min.Y, InBounds.Max.Z}, {InBounds.Max.X, InBounds.Min.Y, InBounds.Max.Z},
            {InBounds.Max.X, InBounds.Max.Y, InBounds.Max.Z}, {InBounds.Min.X, InBounds.Max.Y, InBounds.Max.Z}};
        constexpr int32 Edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                        {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& Edge : Edges)
        {
            VoxelLines.Add({Corners[Edge[0]], Corners[Edge[1]], InColor, InThickness});
        }
    };
    for (const auto& Agent : InSnapshot._Agents)
    {
        if (NOT Agent._Velocity.IsNearlyZero())
        {
            const auto Length = static_cast<float>(FMath::Min(Agent._Velocity.Size(), 250.0));
            Velocities.Add({Agent._Position, Agent._Velocity.GetSafeNormal(), Length, 10.0f, Agent._StatusColor});
        }
        if (InSnapshot._SelectedIdentity.IsSet() && *InSnapshot._SelectedIdentity == Agent._Identity)
        {
            auto Previous = Agent._Position;
            for (const auto& Point : Agent._PlannedPath)
            {
                Paths.Add({Previous, Point, FLinearColor{1.0f, 0.9f, 0.2f}, 3.0f});
                Previous = Point;
            }
        }
    }
    auto AgentLocations = TMap<uint64, FVector>{};
    for (const auto& Agent : InSnapshot._Agents)
    { AgentLocations.Add(Agent._Identity, Agent._Position); }
    for (const auto& Queue : InSnapshot._Queues)
    {
        for (const auto& Origin : Queue._Origins)
        {
            const auto Color = FLinearColor{0.15f, 0.9f, 1.0f, 0.95f};
            QueueOriginLines.Add({Origin._From, Origin._To, Color, 4.0f});
            QueueOriginLines.Add({Origin._From - FVector{70.0f, 0.0f, 0.0f},
                                  Origin._From + FVector{70.0f, 0.0f, 0.0f}, Color, 4.0f});
            QueueOriginLines.Add({Origin._From - FVector{0.0f, 70.0f, 0.0f},
                                  Origin._From + FVector{0.0f, 70.0f, 0.0f}, Color, 4.0f});
            QueueOriginLines.Add({Origin._From, Origin._From + FVector{0.0f, 0.0f, 140.0f}, Color, 4.0f});
        }
        auto PreviousByOrigin = TMap<int32, FVector>{};
        for (const auto& Member : Queue._Members)
        {
            if (NOT Member._HasReservation)
            { continue; }
            if (const auto* Previous = PreviousByOrigin.Find(Member._OriginIndex))
            { QueueFormationLines.Add({*Previous, Member._ReservationLocation, FLinearColor{0.92f, 0.30f, 1.0f, 0.9f}, 3.0f}); }
            PreviousByOrigin.Add(Member._OriginIndex, Member._ReservationLocation);
            if (const auto* AgentLocation = AgentLocations.Find(Member._AgentIdentity))
            { QueueMemberLinks.Add({*AgentLocation, Member._ReservationLocation, FLinearColor{0.2f, 0.95f, 1.0f, 0.75f}, 1.5f}); }
        }
    }
    for (const auto& Ribbon : InSnapshot._PathNetwork._Ribbons)
    {
        if (NOT ck_crowd_debugger_3d_scene_adapter::IsRibbonInputValid(Ribbon))
        {
            continue;
        }

        auto Left = TArray<FVector>{};
        auto Right = TArray<FVector>{};
        Left.Reserve(Ribbon._Points.Num());
        Right.Reserve(Ribbon._Points.Num());
        for (auto PointIndex = 0; PointIndex < Ribbon._Points.Num(); ++PointIndex)
        {
            const auto Offset = ck_crowd_debugger_3d_scene_adapter::Get_RibbonOffset(Ribbon._Points, PointIndex,
                                                                                     Ribbon._HalfWidths[PointIndex]);
            Left.Add(Ribbon._Points[PointIndex] + Offset);
            Right.Add(Ribbon._Points[PointIndex] - Offset);
        }
        for (auto PointIndex = 0; PointIndex + 1 < Left.Num(); ++PointIndex)
        {
            RibbonOutlines.Add({Left[PointIndex], Left[PointIndex + 1],
                                FLinearColor{0.0f, 0.8f, 0.9f, InSnapshot._PathNetwork._Opacity}, 1.0f});
            RibbonOutlines.Add({Right[PointIndex], Right[PointIndex + 1],
                                FLinearColor{0.0f, 0.8f, 0.9f, InSnapshot._PathNetwork._Opacity}, 1.0f});
        }
    }
    AddBox(InSnapshot._Voxel._AuthoredBounds, InSnapshot._Voxel._AuthoredBoundsColor, 2.0f);
    AddBox(InSnapshot._Voxel._NavigationBounds, InSnapshot._Voxel._NavigationBoundsColor, 1.0f);
    if (InSnapshot._Voxel._CanDrawRetainedGeometry)
    {
        AddBox(InSnapshot._Voxel._PendingBounds, InSnapshot._Voxel._PendingBoundsColor, 2.0f);
        AddBox(InSnapshot._Voxel._ActiveBounds, InSnapshot._Voxel._ActiveBoundsColor, 2.0f);
        for (const auto& Chunk : InSnapshot._Voxel._Chunks)
        {
            AddBox(Chunk, InSnapshot._Voxel._ChunkColor, 1.5f);
        }
        const auto AddSegment =
            [&VoxelLines](const FCkCrowdDebugger_3dSegmentSnapshot& InSegment, const FLinearColor& InColor)
        {
            if (InSegment._HasVia)
            {
                VoxelLines.Add({InSegment._From, InSegment._Via, InColor, 2.0f});
                VoxelLines.Add({InSegment._Via, InSegment._To, InColor, 2.0f});
            }
            else
            {
                VoxelLines.Add({InSegment._From, InSegment._To, InColor, 2.0f});
            }
        };
        for (const auto& Portal : InSnapshot._Voxel._Portals)
        {
            AddSegment(Portal, InSnapshot._Voxel._PortalColor);
        }
        for (const auto& Repair : InSnapshot._Voxel._RepairLinks)
        {
            AddSegment(Repair, InSnapshot._Voxel._RepairColor);
        }
    }
    return InTarget.Set_LineChannel(ck_crowd_debugger_3d_scene_adapter::PathChannel, MoveTemp(Paths)) &&
           InTarget.Set_VectorChannel(ck_crowd_debugger_3d_scene_adapter::VelocityChannel, MoveTemp(Velocities)) &&
           InTarget.Set_LineChannel(ck_crowd_debugger_3d_scene_adapter::RibbonOutlineChannel,
                                    MoveTemp(RibbonOutlines)) &&
           InTarget.Set_LineChannel(FName{TEXT("CkCrowd.Voxel")}, MoveTemp(VoxelLines)) &&
           InTarget.Set_LineChannel(ck_crowd_debugger_3d_scene_adapter::QueueOriginChannel, MoveTemp(QueueOriginLines)) &&
           InTarget.Set_LineChannel(ck_crowd_debugger_3d_scene_adapter::QueueFormationChannel, MoveTemp(QueueFormationLines)) &&
           InTarget.Set_LineChannel(ck_crowd_debugger_3d_scene_adapter::QueueMemberLinkChannel, MoveTemp(QueueMemberLinks));
}

auto
    FCkCrowdDebugger_3dSceneAdapter::
    Reconcile(const FCkCrowdDebugger_3dSceneSnapshot& InSnapshot,
        FCk_DebugScene_Target& InTarget)
    -> bool
{
    const auto PreviousState = *this;
    const auto RestoreAndFail = [this, &PreviousState]() -> bool
    {
        *this = PreviousState;
        return false;
    };
    const auto InputsValid = InSnapshot._WorldEpoch != 0;
    CK_ENSURE_IF_NOT(InputsValid, TEXT("Crowd debug-scene adapter rejected a snapshot without a world epoch"))
    {
    }
    if (NOT InputsValid)
    {
        return RestoreAndFail();
    }
    if (_WorldEpoch != 0 && _WorldEpoch != InSnapshot._WorldEpoch)
    {
        Reset_ForWorldChange(InTarget);
    }
    _WorldEpoch = InSnapshot._WorldEpoch;
    _AgentIndices.Reset();
    _AgentItemKeys.Reset();
    _AgentInstances.Reset();
    _RoleItems.Reset();
    _NonItemRoleCounts.Reset();
    _Appearances.Reset();
    _RoleAppearances.Reset();
    _SelectedIdentity = InSnapshot._SelectedIdentity;
    _RoleAppearances.Add(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon,
                         MakeAppearance(FLinearColor{0.0f, 0.8f, 0.9f, InSnapshot._PathNetwork._Opacity},
                                        ck_crowd_debugger_3d_scene_adapter::IsTransparent));
    InTarget.Begin_Reconcile();
    const auto Capsule = GetOrCreateCapsuleMesh();
    if (NOT Capsule.IsValid())
    {
        InTarget.Abort_Reconcile();
        return RestoreAndFail();
    }
    for (auto Index = 0; Index < InSnapshot._Agents.Num(); ++Index)
    {
        const auto& Agent = InSnapshot._Agents[Index];
        auto PathIsFinite = true;
        for (const auto& Point : Agent._PlannedPath)
        {
            PathIsFinite &= ck_crowd_debugger_3d_scene_adapter::IsFinite(Point);
        }
        const auto ValidAgent = Agent._Identity != 0 && ck_crowd_debugger_3d_scene_adapter::IsFinite(Agent._Position) &&
                                FMath::IsFinite(Agent._Radius) && FMath::IsFinite(Agent._Height) &&
                                Agent._Radius > 0.0f && Agent._Height > 0.0f;
        const auto CompleteAgentIsValid =
            ValidAgent && PathIsFinite && ck_crowd_debugger_3d_scene_adapter::IsFinite(Agent._Velocity) &&
            FMath::IsFinite(Agent._StatusColor.R) && FMath::IsFinite(Agent._StatusColor.G) &&
            FMath::IsFinite(Agent._StatusColor.B) && FMath::IsFinite(Agent._StatusColor.A);
        CK_ENSURE_IF_NOT(CompleteAgentIsValid, TEXT("Crowd debug-scene adapter rejected an invalid agent snapshot"))
        {
        }
        if (NOT CompleteAgentIsValid)
        {
            InTarget.Abort_Reconcile();
            return RestoreAndFail();
        }
        const auto Key = MakeItemKey(ECkCrowdDebugger_3dSceneRole::AgentCapsule, Agent._Identity);
        const auto Appearance = MakeAppearance(Agent._StatusColor);
        const auto Transform =
            FTransform{FQuat::Identity, Agent._Position, FVector{Agent._Radius, Agent._Radius, Agent._Height}};
        auto Instances = TArray<FCk_DebugScene_Instance>{FCk_DebugScene_Instance{}
                                                             .Set_Mesh(Capsule)
                                                             .Set_Transform(Transform)
                                                             .Set_Appearance(Appearance)
                                                             .Set_PickIdentity(Agent._Identity)};
        if (NOT InTarget.Upsert_Item(Key, Instances))
        {
            InTarget.Abort_Reconcile();
            return RestoreAndFail();
        }
        _AgentIndices.Add(Agent._Identity, Index);
        _AgentItemKeys.Add(Agent._Identity, Key);
        _AgentInstances.Add(Agent._Identity, Instances);
        _RoleItems.FindOrAdd(ECkCrowdDebugger_3dSceneRole::AgentCapsule).Add(Key);
        _Appearances.Add(Agent._Identity, Appearance);
        if (InSnapshot._SelectedIdentity.IsSet() && *InSnapshot._SelectedIdentity == Agent._Identity &&
            NOT Agent._PlannedPath.IsEmpty())
        {
            _NonItemRoleCounts.Add(ECkCrowdDebugger_3dSceneRole::SelectedPath, 1);
        }
    }
    auto SubmitStatic = [&](ECkCrowdDebugger_3dSceneRole Role, uint64 Identity, TSharedPtr<FCk_DebugScene_Mesh> Mesh,
                             FLinearColor Color, bool Transparent,
                             const FTransform& Transform = FTransform::Identity,
                             ECk_DebugScene_DepthPriority DepthPriority = ECk_DebugScene_DepthPriority::World,
                             int32 TranslucencySortPriority = 0) -> bool
    {
        if (NOT Mesh.IsValid())
        {
            return false;
        }
        const auto Key = MakeItemKey(Role, Identity);
        const auto Appearance = MakeAppearance(Color, Transparent, DepthPriority, TranslucencySortPriority);
        auto Instances = TArray<FCk_DebugScene_Instance>{FCk_DebugScene_Instance{}
                                                             .Set_Mesh(Mesh)
                                                             .Set_Transform(Transform)
                                                             .Set_Appearance(Appearance)
                                                             .Set_PickIdentity(0)};
        if (NOT InTarget.Upsert_Item(Key, Instances))
        {
            return false;
        }
        _StaticInstances.Add(Key, Instances);
        _RoleItems.FindOrAdd(Role).Add(Key);
        _RoleAppearances.Add(Role, Appearance);
        return true;
    };
    // Queue geometry is source-owned and transactional just like navmesh and voxel snapshots.  Reservation slots
    // use a small oriented box; origin boxes plus formation/member link channels make queue structure legible even
    // when agents overlap during arrival.
    for (const auto Role : {ECkCrowdDebugger_3dSceneRole::QueueOrigin, ECkCrowdDebugger_3dSceneRole::QueueReservation})
    {
        if (const auto* PreviousKeys = PreviousState._RoleItems.Find(Role))
        {
            for (const auto Key : *PreviousKeys)
            { _StaticInstances.Remove(Key); }
        }
    }
    for (const auto& Queue : InSnapshot._Queues)
    {
        const auto QueueValid = Queue._Identity != 0;
        CK_ENSURE_IF_NOT(QueueValid, TEXT("Crowd debug-scene adapter rejected an invalid queue snapshot")) {}
        if (NOT QueueValid)
        { InTarget.Abort_Reconcile(); return RestoreAndFail(); }
        const auto Hue = static_cast<float>(GetTypeHash(Queue._Category)) / static_cast<float>(MAX_uint32);
        const auto Color = FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue * 255.0f), 170, 255);
        for (auto OriginIndex = 0; OriginIndex < Queue._Origins.Num(); ++OriginIndex)
        {
            const auto& Origin = Queue._Origins[OriginIndex];
            if (NOT ck_crowd_debugger_3d_scene_adapter::IsFinite(Origin._From))
            { InTarget.Abort_Reconcile(); return RestoreAndFail(); }
            if (NOT SubmitStatic(ECkCrowdDebugger_3dSceneRole::QueueOrigin,
                     HashCombineFast(GetTypeHash(Queue._Identity), GetTypeHash(OriginIndex)), GetOrCreateBoxMesh(), Color,
                     ck_crowd_debugger_3d_scene_adapter::IsTransparent,
                     FTransform{FQuat::Identity, Origin._From, FVector{70.0f, 70.0f, 140.0f}}))
            { InTarget.Abort_Reconcile(); return RestoreAndFail(); }
        }
        for (const auto& Member : Queue._Members)
        {
            if (NOT Member._HasReservation)
            { continue; }
            const auto ValidReservation = Member._SlotIdentity != 0 &&
                ck_crowd_debugger_3d_scene_adapter::IsFinite(Member._ReservationLocation);
            CK_ENSURE_IF_NOT(ValidReservation, TEXT("Crowd debug-scene adapter rejected an invalid queue reservation")) {}
            if (NOT ValidReservation)
            { InTarget.Abort_Reconcile(); return RestoreAndFail(); }
            const auto Rotation = Member._ReservationForward.IsNearlyZero()
                ? FQuat::Identity : FRotationMatrix::MakeFromX(Member._ReservationForward.GetSafeNormal()).ToQuat();
            if (NOT SubmitStatic(ECkCrowdDebugger_3dSceneRole::QueueReservation, Member._SlotIdentity,
                    GetOrCreateBoxMesh(), FLinearColor{Color.R, Color.G, Color.B, 0.55f},
                    ck_crowd_debugger_3d_scene_adapter::IsTransparent,
                    FTransform{Rotation, Member._ReservationLocation, FVector{60.0f, 32.0f, 12.0f}}))
            { InTarget.Abort_Reconcile(); return RestoreAndFail(); }
        }
    }
    if (InSnapshot._Recast._Revision != _RecastRevision)
    {
        auto Triangles = TArray<FCk_DebugScene_Triangle>{};
        auto LogicalTriangleCount = 0;
        for (auto Index = 0; Index + 2 < InSnapshot._Recast._Triangles.Num(); Index += 3)
        {
            const auto SourceTriangle =
                FCk_DebugScene_Triangle{InSnapshot._Recast._Triangles[Index], InSnapshot._Recast._Triangles[Index + 1],
                                         InSnapshot._Recast._Triangles[Index + 2]};
            if (ck_crowd_debugger_3d_scene_adapter::AppendTwoSidedTriangle(SourceTriangle, Triangles))
            {
                ++LogicalTriangleCount;
            }
        }
        _StaticInstances.Remove(MakeItemKey(ECkCrowdDebugger_3dSceneRole::Recast, 1));
        const auto RenderedTriangleCount = Triangles.Num();
        if (NOT Triangles.IsEmpty() && NOT SubmitStatic(ECkCrowdDebugger_3dSceneRole::Recast, 1,
                                                        FCk_DebugScene_Mesh::Create_FromTriangles(MoveTemp(Triangles)),
                                                        FLinearColor{0.27f, 0.78f, 0.43f, 0.15f},
                                                        ck_crowd_debugger_3d_scene_adapter::IsTransparent,
                                                        FTransform::Identity, ECk_DebugScene_DepthPriority::World))
        {
            InTarget.Abort_Reconcile();
            return RestoreAndFail();
        }
        _RecastRevision = InSnapshot._Recast._Revision;
        _RecastTriangleCount = LogicalTriangleCount;
        _RecastRenderedTriangleCount = RenderedTriangleCount;
    }
    else if (const auto* Instances = _StaticInstances.Find(MakeItemKey(ECkCrowdDebugger_3dSceneRole::Recast, 1)))
    {
        InTarget.Upsert_Item(MakeItemKey(ECkCrowdDebugger_3dSceneRole::Recast, 1), *Instances);
        _RoleItems.FindOrAdd(ECkCrowdDebugger_3dSceneRole::Recast)
            .Add(MakeItemKey(ECkCrowdDebugger_3dSceneRole::Recast, 1));
    }
    if (InSnapshot._PathNetwork._Revision == _RibbonRevision)
    {
        for (auto Index = 0; Index < InSnapshot._PathNetwork._Ribbons.Num(); ++Index)
        {
            const auto Key = MakeItemKey(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon, Index + 1);
            if (const auto* Instances = _StaticInstances.Find(Key))
            {
                InTarget.Upsert_Item(Key, *Instances);
                _RoleItems.FindOrAdd(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon).Add(Key);
            }
        }
    }
    else
    {
        _RibbonTriangleCounts.Reset();
        _RibbonRenderedTriangleCounts.Reset();
        _RibbonOutlinePointCounts.Reset();
        _RibbonTriangleCounts.SetNumZeroed(InSnapshot._PathNetwork._Ribbons.Num());
        _RibbonRenderedTriangleCounts.SetNumZeroed(InSnapshot._PathNetwork._Ribbons.Num());
        _RibbonOutlinePointCounts.SetNumZeroed(InSnapshot._PathNetwork._Ribbons.Num());
        for (auto Index = InSnapshot._PathNetwork._Ribbons.Num(); Index < _RibbonCount; ++Index)
        {
            const auto Identity = static_cast<uint64>(Index + 1);
            _StaticInstances.Remove(MakeItemKey(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon, Identity));
            _InternalItemKeys.Remove(
                FString::Printf(TEXT("%d:%llu"), static_cast<int32>(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon),
                                static_cast<unsigned long long>(Identity)));
        }
        for (auto Index = 0; Index < InSnapshot._PathNetwork._Ribbons.Num(); ++Index)
        {
            auto Triangles = TArray<FCk_DebugScene_Triangle>{};
            const auto& Ribbon = InSnapshot._PathNetwork._Ribbons[Index];
            const auto Key = MakeItemKey(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon, Index + 1);
            _StaticInstances.Remove(Key);
            if (NOT ck_crowd_debugger_3d_scene_adapter::AddRibbonTriangles(Ribbon, Triangles))
            {
                continue;
            }
            const auto SourceTriangles = Triangles;
            Triangles.Reset();
            auto LogicalTriangleCount = 0;
            for (const auto& Triangle : SourceTriangles)
            {
                if (ck_crowd_debugger_3d_scene_adapter::AppendTwoSidedTriangle(Triangle, Triangles))
                {
                    ++LogicalTriangleCount;
                }
            }
            if (LogicalTriangleCount == 0)
            {
                continue;
            }
            const auto RenderedTriangleCount = Triangles.Num();
            _RibbonTriangleCounts[Index] = LogicalTriangleCount;
            _RibbonRenderedTriangleCounts[Index] = RenderedTriangleCount;
            _RibbonOutlinePointCounts[Index] = Ribbon._Points.Num();
            if (NOT SubmitStatic(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon, Index + 1,
                                  FCk_DebugScene_Mesh::Create_FromTriangles(MoveTemp(Triangles)),
                                  FLinearColor{0.0f, 0.8f, 0.9f, InSnapshot._PathNetwork._Opacity},
                                  ck_crowd_debugger_3d_scene_adapter::IsTransparent, FTransform::Identity,
                                  ECk_DebugScene_DepthPriority::Foreground,
                                  ck_crowd_debugger_3d_scene_adapter::PathNetworkSortPriority))
            {
                InTarget.Abort_Reconcile();
                return RestoreAndFail();
            }
        }
        _RibbonRevision = InSnapshot._PathNetwork._Revision;
        _RibbonCount = InSnapshot._PathNetwork._Ribbons.Num();
    }
    const auto VoxelRoles = TArray<ECkCrowdDebugger_3dSceneRole>{
        ECkCrowdDebugger_3dSceneRole::VoxelOccupied, ECkCrowdDebugger_3dSceneRole::VoxelMergedFree,
        ECkCrowdDebugger_3dSceneRole::VoxelRawFree,  ECkCrowdDebugger_3dSceneRole::VoxelChunk,
        ECkCrowdDebugger_3dSceneRole::VoxelPortal,   ECkCrowdDebugger_3dSceneRole::VoxelRepair};
    const auto CanReuseVoxel =
        PreviousState._WorldEpoch == InSnapshot._WorldEpoch && _VoxelRevision == InSnapshot._Voxel._Revision;
    if (CanReuseVoxel)
    {
        for (const auto Role : VoxelRoles)
        {
            const auto* PreviousKeys = PreviousState._RoleItems.Find(Role);
            if (PreviousKeys == nullptr)
            {
                continue;
            }
            for (const auto Key : *PreviousKeys)
            {
                const auto* Instances = _StaticInstances.Find(Key);
                if (Instances == nullptr || NOT InTarget.Upsert_Item(Key, *Instances))
                {
                    InTarget.Abort_Reconcile();
                    return RestoreAndFail();
                }
                _RoleItems.FindOrAdd(Role).Add(Key);
            }
            if (const auto* Appearance = PreviousState._RoleAppearances.Find(Role))
            {
                _RoleAppearances.Add(Role, *Appearance);
            }
        }
    }
    else
    {
        for (const auto Role : VoxelRoles)
        {
            if (const auto* PreviousKeys = PreviousState._RoleItems.Find(Role))
            {
                for (const auto Key : *PreviousKeys)
                {
                    _StaticInstances.Remove(Key);
                }
            }
        }
        _VoxelRevision = InSnapshot._Voxel._Revision;
        const auto SubmitVoxelCells = [&](ECkCrowdDebugger_3dSceneRole InRole, const TArray<FBox>& InCells,
                                          int32 InCap) -> bool
        {
            auto Color = InSnapshot._Voxel._MergedFreeColor;
            if (InRole == ECkCrowdDebugger_3dSceneRole::VoxelOccupied)
            {
                Color = InSnapshot._Voxel._OccupiedColor;
            }
            else if (InRole == ECkCrowdDebugger_3dSceneRole::VoxelRawFree)
            {
                Color = InSnapshot._Voxel._RawFreeColor;
            }
            for (auto Index = 0; Index < FMath::Min(InCells.Num(), InCap); ++Index)
            {
                const auto& Bounds = InCells[Index];
                if (NOT SubmitStatic(InRole, Index + 1, GetOrCreateBoxMesh(), Color,
                                     ck_crowd_debugger_3d_scene_adapter::IsTransparent,
                                     FTransform{FQuat::Identity, Bounds.GetCenter(), Bounds.GetSize()}))
                {
                    return false;
                }
            }
            return true;
        };
        const auto IsLayerVisible = [&InSnapshot](ECkCrowdDebugger_3dVoxelLayer InLayer) -> bool
        {
            const auto* Visible = InSnapshot._Voxel._LayerVisibility.Find(InLayer);
            return Visible == nullptr || *Visible;
        };
        if (InSnapshot._Voxel._CanDrawRetainedGeometry &&
            ((IsLayerVisible(ECkCrowdDebugger_3dVoxelLayer::Occupied) &&
              NOT SubmitVoxelCells(ECkCrowdDebugger_3dSceneRole::VoxelOccupied, InSnapshot._Voxel._Cells._Occupied,
                                   10000)) ||
             (IsLayerVisible(ECkCrowdDebugger_3dVoxelLayer::MergedFree) &&
              NOT SubmitVoxelCells(ECkCrowdDebugger_3dSceneRole::VoxelMergedFree, InSnapshot._Voxel._Cells._MergedFree,
                                   MAX_int32)) ||
             (IsLayerVisible(ECkCrowdDebugger_3dVoxelLayer::RawFree) &&
              NOT SubmitVoxelCells(ECkCrowdDebugger_3dSceneRole::VoxelRawFree, InSnapshot._Voxel._Cells._RawFree,
                                   InSnapshot._Voxel._RawFreeCellCap))))
        {
            InTarget.Abort_Reconcile();
            return RestoreAndFail();
        }
        const auto& VoxelBounds = InSnapshot._Voxel._NavigationBounds.IsValid != 0 ? InSnapshot._Voxel._NavigationBounds
                                                                                   : InSnapshot._Voxel._AuthoredBounds;
        if (VoxelBounds.IsValid != 0 &&
            NOT SubmitStatic(ECkCrowdDebugger_3dSceneRole::VoxelChunk, 0, GetOrCreateBoxMesh(),
                             InSnapshot._Voxel._NavigationBoundsColor,
                             ck_crowd_debugger_3d_scene_adapter::IsTransparent,
                             FTransform{FQuat::Identity, VoxelBounds.GetCenter(), VoxelBounds.GetSize()}))
        {
            InTarget.Abort_Reconcile();
            return RestoreAndFail();
        }
        for (auto Index = 0; InSnapshot._Voxel._CanDrawRetainedGeometry && Index < InSnapshot._Voxel._Chunks.Num();
             ++Index)
        {
            const auto& Chunk = InSnapshot._Voxel._Chunks[Index];
            if (NOT SubmitStatic(ECkCrowdDebugger_3dSceneRole::VoxelChunk, GetTypeHash(Chunk.Min), GetOrCreateBoxMesh(),
                                 InSnapshot._Voxel._ChunkColor,
                                 ck_crowd_debugger_3d_scene_adapter::IsTransparent,
                                 FTransform{FQuat::Identity, Chunk.GetCenter(), Chunk.GetSize()}))
            {
                InTarget.Abort_Reconcile();
                return RestoreAndFail();
            }
        }
        for (auto Index = 0; InSnapshot._Voxel._CanDrawRetainedGeometry && Index < InSnapshot._Voxel._Portals.Num();
             ++Index)
        {
            const auto& Segment = InSnapshot._Voxel._Portals[Index];
            const auto Bounds = FBox{Segment._From, Segment._To}.ExpandBy(2.0f);
            if (NOT SubmitStatic(ECkCrowdDebugger_3dSceneRole::VoxelPortal, Index + 1, GetOrCreateBoxMesh(),
                                 InSnapshot._Voxel._PortalColor,
                                 ck_crowd_debugger_3d_scene_adapter::IsTransparent,
                                 FTransform{FQuat::Identity, Bounds.GetCenter(), Bounds.GetSize()}))
            {
                InTarget.Abort_Reconcile();
                return RestoreAndFail();
            }
        }
        for (auto Index = 0; InSnapshot._Voxel._CanDrawRetainedGeometry && Index < InSnapshot._Voxel._RepairLinks.Num();
             ++Index)
        {
            const auto& Segment = InSnapshot._Voxel._RepairLinks[Index];
            const auto Bounds = FBox{Segment._From, Segment._To}.ExpandBy(2.0f);
            if (NOT SubmitStatic(ECkCrowdDebugger_3dSceneRole::VoxelRepair, Index + 1, GetOrCreateBoxMesh(),
                                 InSnapshot._Voxel._RepairColor,
                                 ck_crowd_debugger_3d_scene_adapter::IsTransparent,
                                 FTransform{FQuat::Identity, Bounds.GetCenter(), Bounds.GetSize()}))
            {
                InTarget.Abort_Reconcile();
                return RestoreAndFail();
            }
        }
    }
    if (NOT SubmitLines(InSnapshot, InTarget))
    {
        InTarget.Abort_Reconcile();
        return RestoreAndFail();
    }
    const auto Committed = InTarget.End_Reconcile();
    if (NOT Committed)
    {
        return RestoreAndFail();
    }

    return true;
}

auto
    FCkCrowdDebugger_3dSceneAdapter::
    Reset_ForWorldChange(FCk_DebugScene_Target& InTarget)
    -> void
{
    InTarget.HideAll();
    _WorldEpoch = 0;
    _RecastRevision = MAX_uint64;
    _RibbonRevision = MAX_uint64;
    _VoxelRevision = MAX_uint64;
    _RibbonCount = 0;
    _SelectedIdentity.Reset();
    _AgentIndices.Reset();
    _AgentItemKeys.Reset();
    _AgentInstances.Reset();
    _StaticInstances.Reset();
    _InternalItemKeys.Reset();
    _NextItemKey = 1;
    _RoleItems.Reset();
    _NonItemRoleCounts.Reset();
    _Appearances.Reset();
    _RoleAppearances.Reset();
    _RecastTriangleCount = 0;
    _RecastRenderedTriangleCount = 0;
    _RibbonTriangleCounts.Reset();
    _RibbonRenderedTriangleCounts.Reset();
    _RibbonOutlinePointCounts.Reset();
}

auto
    FCkCrowdDebugger_3dSceneAdapter::
    Resolve_Pick(const FCk_DebugScene_Pick& InPick) const
        -> TOptional<FCkCrowdDebugger_3dPickResolution>
        {
        const auto* Index = _AgentIndices.Find(InPick.Get_PickIdentity());
        return Index != nullptr ? TOptional<FCkCrowdDebugger_3dPickResolution>{{InPick.Get_PickIdentity(), *Index}}
        : TOptional<FCkCrowdDebugger_3dPickResolution>{};
        }
        auto
        FCkCrowdDebugger_3dSceneAdapter::Get_CurrentAgentIndex(uint64 InIdentity) const
    -> TOptional<int32>
{
    const auto* Value = _AgentIndices.Find(InIdentity);
    return Value != nullptr ? TOptional<int32>{*Value} : TOptional<int32>{};
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    TrySelect_Identity(uint64 InIdentity, FCk_DebugScene_Target& InTarget)
    -> bool
{
    const auto* Key = _AgentItemKeys.Find(InIdentity);
    if (Key == nullptr || NOT InTarget.Get_ItemBounds(*Key).IsSet())
    {
        return false;
    }
    _SelectedIdentity = InIdentity;
    return true;
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_SelectionBounds(const FCk_DebugScene_Target& InTarget) const
    -> TOptional<FBox>
{
    const auto* Key = _SelectedIdentity.IsSet() ? _AgentItemKeys.Find(*_SelectedIdentity) : nullptr;
    return Key != nullptr ? InTarget.Get_ItemBounds(*Key) : TOptional<FBox>{};
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_ItemCount(ECkCrowdDebugger_3dSceneRole InRole) const
    -> int32
{
    const auto* Items = _RoleItems.Find(InRole);
    const auto* NonItems = _NonItemRoleCounts.Find(InRole);
    return (Items != nullptr ? Items->Num() : 0) + (NonItems != nullptr ? *NonItems : 0);
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Has_Role(ECkCrowdDebugger_3dSceneRole InRole) const
    -> bool
{
    return Get_ItemCount(InRole) > 0;
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_TargetItemId(ECkCrowdDebugger_3dSceneRole InRole, int32 InIndex) const
        -> TOptional<uint64>
        {
        const auto* Items = _RoleItems.Find(InRole);
        return Items != nullptr && Items->IsValidIndex(InIndex) ? TOptional<uint64>{(*Items)[InIndex]}
        : TOptional<uint64>{};
        }
        auto
        FCkCrowdDebugger_3dSceneAdapter::Get_SubmittedInstances(uint64 InIdentity) const
        -> const TArray<FCk_DebugScene_Instance>&
        {
        static const auto Empty = TArray<FCk_DebugScene_Instance>{};
        const auto* Found = _AgentInstances.Find(InIdentity);
        return Found != nullptr ? *Found : Empty;
        }
        auto
        FCkCrowdDebugger_3dSceneAdapter::Get_Appearance(uint64 InIdentity) const
    -> FCk_DebugScene_Appearance
{
    const auto* Found = _Appearances.Find(InIdentity);
    return Found != nullptr ? *Found : FCk_DebugScene_Appearance{};
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_RoleAppearance(ECkCrowdDebugger_3dSceneRole InRole) const
        -> FCk_DebugScene_Appearance
        {
        const auto* Found = _RoleAppearances.Find(InRole);
        return Found != nullptr ? *Found : FCk_DebugScene_Appearance{};
        }
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_RibbonTriangleCount(int32 InIndex) const
    -> int32
{
    return _RibbonTriangleCounts.IsValidIndex(InIndex) ? _RibbonTriangleCounts[InIndex] : 0;
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_RibbonRenderedTriangleCount(int32 InIndex) const
    -> int32
{
    return _RibbonRenderedTriangleCounts.IsValidIndex(InIndex) ? _RibbonRenderedTriangleCounts[InIndex] : 0;
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_RibbonOutlinePointCount(int32 InIndex) const
    -> int32
{
    return _RibbonOutlinePointCounts.IsValidIndex(InIndex) ? _RibbonOutlinePointCounts[InIndex] : 0;
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_RecastTriangleCount() const
    -> int32
{
    return _RecastTriangleCount;
}
auto
    FCkCrowdDebugger_3dSceneAdapter::
    Get_RecastRenderedTriangleCount() const
    -> int32
{
    return _RecastRenderedTriangleCount;
}
