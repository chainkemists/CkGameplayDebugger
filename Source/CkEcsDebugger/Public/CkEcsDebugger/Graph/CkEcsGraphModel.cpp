#include "CkEcsGraphModel.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

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

    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InSelectedEntity);
    const auto Label = FText::FromString(FString::Printf(TEXT("%s [%s]"),
        *DebugName.ToString(),
        *InSelectedEntity.ToString()));

    CenterNodeIndex = FindOrAddNode(
        InSelectedEntity,
        Label,
        FCkDebuggerStyle::Color_Text_Highlight,
        ECkGraphEdgeType::None);
    Nodes[CenterNodeIndex].IsCenterNode = true;

    Gather_LifetimeOwner(InSelectedEntity);

    LastBuiltEntity = InSelectedEntity;
    bForceRebuild = false;
    return true;
}

auto FCkEcsGraphModel::Gather_LifetimeOwner(const FCk_Handle& InEntity) -> void
{
    if (NOT InEntity.Has<ck::FFragment_LifetimeOwner>())
    { return; }

    const auto& Owner = InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
    if (ck::Is_NOT_Valid(Owner))
    { return; }

    const auto OwnerDebugName = UCk_Utils_Handle_UE::Get_DebugName(Owner);
    const auto OwnerLabel = FText::FromString(FString::Printf(TEXT("%s [%s]"),
        *OwnerDebugName.ToString(),
        *Owner.ToString()));

    const auto OwnerIndex = FindOrAddNode(
        Owner,
        OwnerLabel,
        FCkDebuggerStyle::Color_Relationship,
        ECkGraphEdgeType::LifetimeOwner);

    AddEdge(CenterNodeIndex, OwnerIndex, ECkGraphEdgeType::LifetimeOwner, FCkDebuggerStyle::Color_Relationship);
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
