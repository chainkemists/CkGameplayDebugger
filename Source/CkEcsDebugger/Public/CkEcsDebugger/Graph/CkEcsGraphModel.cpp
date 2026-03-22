#include "CkEcsGraphModel.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Utils.h"

#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

// =====================================================================================================================

auto GetEdgeTypeLabel(ECkGraphEdgeType InType) -> FText
{
    switch (InType)
    {
    case ECkGraphEdgeType::LifetimeOwner:   return FText::FromString(TEXT("Lifetime Owner"));
    case ECkGraphEdgeType::ContextOwner:    return FText::FromString(TEXT("Context Owner"));
    case ECkGraphEdgeType::SceneNodeChild:  return FText::FromString(TEXT("Scene Node"));
    default:                                return FText::GetEmpty();
    }
}

// =====================================================================================================================

auto FCkEcsGraphModel::Rebuild(const FCk_Handle& InSelectedEntity) -> bool
{
    if (NOT bForceRebuild
        && InSelectedEntity == LastBuiltEntity
        && ck::IsValid(InSelectedEntity))
    {
        return false;
    }

    Clear();

    if (ck::Is_NOT_Valid(InSelectedEntity))
    {
        LastBuiltEntity = FCk_Handle();
        bForceRebuild = false;
        return true;
    }

    CenterNodeIndex = FindOrAddNode(
        InSelectedEntity,
        MakeEntityLabel(InSelectedEntity),
        FCkDebuggerStyle::Color_Text_Highlight,
        ECkGraphEdgeType::None);
    Nodes[CenterNodeIndex].IsCenterNode = true;

    Gather_LifetimeOwner(InSelectedEntity);
    Gather_ContextOwner(InSelectedEntity);
    Gather_SceneNodeChildren(InSelectedEntity);

    LastBuiltEntity = InSelectedEntity;
    bForceRebuild = false;
    return true;
}

// =====================================================================================================================

auto FCkEcsGraphModel::Gather_LifetimeOwner(const FCk_Handle& InEntity) -> void
{
    if (NOT InEntity.Has<ck::FFragment_LifetimeOwner>())
    { return; }

    const auto& Owner = InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
    if (ck::Is_NOT_Valid(Owner))
    { return; }

    const auto OwnerIndex = FindOrAddNode(
        Owner, MakeEntityLabel(Owner),
        FCkDebuggerStyle::Color_Relationship,
        ECkGraphEdgeType::LifetimeOwner);

    AddEdge(CenterNodeIndex, OwnerIndex, ECkGraphEdgeType::LifetimeOwner, FCkDebuggerStyle::Color_Relationship);
}

auto FCkEcsGraphModel::Gather_ContextOwner(const FCk_Handle& InEntity) -> void
{
    if (NOT UCk_Utils_ContextOwner_UE::Has(InEntity))
    { return; }

    const auto ContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(InEntity);
    if (ck::Is_NOT_Valid(ContextOwner))
    { return; }

    const auto OwnerIndex = FindOrAddNode(
        ContextOwner, MakeEntityLabel(ContextOwner),
        FCkDebuggerStyle::Color_Reference,
        ECkGraphEdgeType::ContextOwner);

    AddEdge(CenterNodeIndex, OwnerIndex, ECkGraphEdgeType::ContextOwner, FCkDebuggerStyle::Color_Reference);
}

auto FCkEcsGraphModel::Gather_SceneNodeChildren(const FCk_Handle& InEntity) -> void
{
    if (NOT UCk_Utils_Transform_UE::Has(InEntity))
    { return; }

    auto TransformHandle = UCk_Utils_Transform_UE::Cast(InEntity);
    UCk_Utils_SceneNode_UE::ForEach_SceneNode(TransformHandle,
        [this](FCk_Handle_SceneNode InSceneNode)
        {
            const auto ChildHandle = FCk_Handle(InSceneNode);
            if (ck::Is_NOT_Valid(ChildHandle))
            { return; }

            const auto ChildIndex = FindOrAddNode(
                ChildHandle, MakeEntityLabel(ChildHandle),
                FCkDebuggerStyle::Color_Transform,
                ECkGraphEdgeType::SceneNodeChild);

            AddEdge(CenterNodeIndex, ChildIndex, ECkGraphEdgeType::SceneNodeChild, FCkDebuggerStyle::Color_Transform);
        });
}

// =====================================================================================================================

auto FCkEcsGraphModel::MakeEntityLabel(const FCk_Handle& InEntity) const -> FText
{
    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InEntity);
    return FText::FromString(DebugName.ToString());
}

auto FCkEcsGraphModel::FindOrAddNode(
    const FCk_Handle& InEntity,
    const FText& InLabel,
    const FLinearColor& InColor,
    ECkGraphEdgeType InEdgeType) -> int32
{
    if (const auto* ExistingIndex = NodeIndexMap.Find(InEntity))
    {
        return *ExistingIndex;
    }

    const auto NewIndex = Nodes.Num();

    auto& Node = Nodes.AddDefaulted_GetRef();
    Node.Entity = InEntity;
    Node.Label = InLabel;
    Node.Color = InColor;
    Node.SourceEdgeType = InEdgeType;

    NodeIndexMap.Add(InEntity, NewIndex);
    return NewIndex;
}

auto FCkEcsGraphModel::AddEdge(
    int32 InSource,
    int32 InTarget,
    ECkGraphEdgeType InType,
    const FLinearColor& InColor) -> void
{
    auto& Edge = Edges.AddDefaulted_GetRef();
    Edge.SourceNodeIndex = InSource;
    Edge.TargetNodeIndex = InTarget;
    Edge.Type = InType;
    Edge.Color = InColor;
    Edge.Label = GetEdgeTypeLabel(InType);
}

auto FCkEcsGraphModel::Clear() -> void
{
    Nodes.Reset();
    Edges.Reset();
    NodeIndexMap.Reset();
    CenterNodeIndex = INDEX_NONE;
}

auto FCkEcsGraphModel::Get_Nodes() const -> const TArray<FCkGraphNode>&
{
    return Nodes;
}

auto FCkEcsGraphModel::Get_Edges() const -> const TArray<FCkGraphEdge>&
{
    return Edges;
}

auto FCkEcsGraphModel::Get_CenterNodeIndex() const -> int32
{
    return CenterNodeIndex;
}

auto FCkEcsGraphModel::IsEmpty() const -> bool
{
    return Nodes.IsEmpty();
}

auto FCkEcsGraphModel::Invalidate() -> void
{
    bForceRebuild = true;
}
