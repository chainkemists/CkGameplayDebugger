#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Handle/CkHandle.h"

enum class ECkGraphEdgeType : uint8
{
    None,
    LifetimeOwner,
    ContextOwner,
    SceneNodeChild,
};

/** Returns a human-readable label for the edge type, used for edge labels in the graph. */
auto GetEdgeTypeLabel(ECkGraphEdgeType InType) -> FText;

struct FCkGraphNode
{
    FCk_Handle Entity;
    FText Label;
    FLinearColor Color = FLinearColor::White;
    ECkGraphEdgeType SourceEdgeType = ECkGraphEdgeType::None;
    bool IsCenterNode = false;
};

struct FCkGraphEdge
{
    int32 SourceNodeIndex = INDEX_NONE;
    int32 TargetNodeIndex = INDEX_NONE;
    ECkGraphEdgeType Type = ECkGraphEdgeType::None;
    FLinearColor Color = FLinearColor::White;
    FText Label;
};

class FCkEcsGraphModel
{
public:
    /** Rebuild graph from a selected entity. Returns true if graph changed. */
    auto Rebuild(const FCk_Handle& InSelectedEntity) -> bool;

    auto Get_Nodes() const -> const TArray<FCkGraphNode>&;
    auto Get_Edges() const -> const TArray<FCkGraphEdge>&;
    auto Get_CenterNodeIndex() const -> int32;
    auto IsEmpty() const -> bool;

    /** Force a rebuild on the next call to Rebuild(), even if entity is the same. */
    auto Invalidate() -> void;

private:
    auto Gather_LifetimeOwner(const FCk_Handle& InEntity) -> void;
    auto Gather_ContextOwner(const FCk_Handle& InEntity) -> void;
    auto Gather_SceneNodeChildren(const FCk_Handle& InEntity) -> void;

    auto MakeEntityLabel(const FCk_Handle& InEntity) const -> FText;
    auto FindOrAddNode(const FCk_Handle& InEntity, const FText& InLabel,
                       const FLinearColor& InColor, ECkGraphEdgeType InEdgeType) -> int32;
    auto AddEdge(int32 InSource, int32 InTarget, ECkGraphEdgeType InType,
                 const FLinearColor& InColor) -> void;
    auto Clear() -> void;

    TArray<FCkGraphNode> Nodes;
    TArray<FCkGraphEdge> Edges;
    TMap<FCk_Handle, int32> NodeIndexMap;
    int32 CenterNodeIndex = INDEX_NONE;
    FCk_Handle LastBuiltEntity;
    bool bForceRebuild = true;
};
