#include "CkEcsDebugger/Graph/CkEcsRuntimeGraphModel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEditorTools/Style/CkStyle.h"

namespace ck_ecs_runtime_graph_model
{
    struct FRelatedEntity
    {
        FCk_Handle Entity;
        ECkEcsRuntimeGraphRelationship Relationship = ECkEcsRuntimeGraphRelationship::None;
    };

    auto MakeLabel(const FCk_Handle& InEntity) -> FString
    {
        return UCk_Utils_Handle_UE::Get_DebugName(InEntity).ToString();
    }
} // namespace ck_ecs_runtime_graph_model

auto ck::ecs_runtime_graph::EstimateNodeWidth(const FString& InLabel) -> int32
{
    return FMath::Max(FCkEcsRuntimeGraphLayoutParams::MinNodeWidth,
                      static_cast<int32>(InLabel.Len() *
                                         FCkEcsRuntimeGraphLayoutParams::CharWidthEstimate) +
                          FCkEcsRuntimeGraphLayoutParams::NodePaddingX);
}

auto ck::ecs_runtime_graph::BuildLayout(const FCkEcsRuntimeGraphLayoutInput& InInput)
    -> FCkEcsRuntimeGraphLayoutResult
{
    auto Result = FCkEcsRuntimeGraphLayoutResult{};
    const auto CenterWidth = EstimateNodeWidth(InInput.CenterLabel);

    auto OwnerTotalWidth = 0;
    for (const auto& Label : InInput.OwnerLabels)
    {
        Result.OwnerWidths.Add(EstimateNodeWidth(Label));
        OwnerTotalWidth += Result.OwnerWidths.Last();
    }
    if (NOT Result.OwnerWidths.IsEmpty())
    {
        OwnerTotalWidth += (Result.OwnerWidths.Num() - 1) * FCkEcsRuntimeGraphLayoutParams::NodeGap;
    }

    auto MaxDependentRowWidth = 0;
    auto CurrentDependentRowWidth = 0;
    auto CurrentDependentRowCount = 0;
    for (const auto& Label : InInput.DependentLabels)
    {
        const auto Width = EstimateNodeWidth(Label);
        Result.DependentWidths.Add(Width);

        if (CurrentDependentRowCount > 0)
        {
            CurrentDependentRowWidth += FCkEcsRuntimeGraphLayoutParams::NodeGap;
        }
        CurrentDependentRowWidth += Width;
        ++CurrentDependentRowCount;

        if (CurrentDependentRowCount == FCkEcsRuntimeGraphLayoutParams::MaxDependentsPerRow)
        {
            MaxDependentRowWidth = FMath::Max(MaxDependentRowWidth, CurrentDependentRowWidth);
            CurrentDependentRowWidth = 0;
            CurrentDependentRowCount = 0;
        }
    }
    MaxDependentRowWidth = FMath::Max(MaxDependentRowWidth, CurrentDependentRowWidth);

    const auto WidestRow = FMath::Max3(OwnerTotalWidth, CenterWidth, MaxDependentRowWidth);
    Result.DynamicSpacingY = FMath::Max(FCkEcsRuntimeGraphLayoutParams::SpacingY, WidestRow / 6);
    Result.CenterPosition = {0, Result.DynamicSpacingY};

    auto CurrentOwnerX = -OwnerTotalWidth / 2;
    for (const auto Width : Result.OwnerWidths)
    {
        Result.OwnerPositions.Add({CurrentOwnerX + Width / 2, 0});
        CurrentOwnerX += Width + FCkEcsRuntimeGraphLayoutParams::NodeGap;
    }

    const auto DependentStartY = Result.CenterPosition.Y + Result.DynamicSpacingY;
    for (auto FirstIndex = 0; FirstIndex < Result.DependentWidths.Num();
         FirstIndex += FCkEcsRuntimeGraphLayoutParams::MaxDependentsPerRow)
    {
        const auto LastIndex = FMath::Min(FirstIndex +
                                              FCkEcsRuntimeGraphLayoutParams::MaxDependentsPerRow,
                                          Result.DependentWidths.Num());
        auto RowWidth = 0;
        for (auto Index = FirstIndex; Index < LastIndex; ++Index)
        {
            RowWidth += Result.DependentWidths[Index];
        }
        if (LastIndex - FirstIndex > 1)
        {
            RowWidth += (LastIndex - FirstIndex - 1) * FCkEcsRuntimeGraphLayoutParams::NodeGap;
        }

        auto CurrentX = -RowWidth / 2;
        const auto RowIndex = FirstIndex / FCkEcsRuntimeGraphLayoutParams::MaxDependentsPerRow;
        for (auto Index = FirstIndex; Index < LastIndex; ++Index)
        {
            const auto Width = Result.DependentWidths[Index];
            Result.DependentPositions.Add(
                {CurrentX + Width / 2,
                 DependentStartY + RowIndex * FCkEcsRuntimeGraphLayoutParams::SpacingY});
            CurrentX += Width + FCkEcsRuntimeGraphLayoutParams::NodeGap;
        }
    }

    return Result;
}

auto ck::ecs_runtime_graph::GetRelationshipColor(
    const ECkEcsRuntimeGraphRelationship InRelationship) -> FLinearColor
{
    switch (InRelationship)
    {
        case ECkEcsRuntimeGraphRelationship::LifetimeOwner:
            return CkStyle::Relationship();
        case ECkEcsRuntimeGraphRelationship::ContextOwner:
            return CkStyle::Reference();
        case ECkEcsRuntimeGraphRelationship::LifetimeDependent:
            return CkStyle::Transform();
        default:
            return FLinearColor::Transparent;
    }
}

auto FCkEcsRuntimeGraphModel::MakeStableId(const FCk_Handle& InEntity) -> FString
{
    return ck::IsValid(InEntity)
               ? ck::Format_UE(TEXT("Entity:{}"), InEntity.Get_Entity().ToString())
               : TEXT("Entity:Invalid");
}

auto FCkEcsRuntimeGraphModel::ComputeTopologyHash(const FCk_Handle& InCenter,
                                                  const FCk_Handle& InLifetimeOwner,
                                                  const FCk_Handle& InContextOwner,
                                                  TConstArrayView<FCk_Handle> InDependents)
    -> uint32
{
    auto SortedDependents = TArray<FCk_Handle>{};
    SortedDependents.Append(InDependents.GetData(), InDependents.Num());
    SortedDependents.Sort();

    auto Hash = GetTypeHash(InCenter);
    Hash = HashCombine(Hash, GetTypeHash(InLifetimeOwner));
    Hash = HashCombine(Hash, GetTypeHash(InContextOwner));
    Hash = HashCombine(Hash, GetTypeHash(SortedDependents.Num()));
    for (const auto& Dependent : SortedDependents)
    {
        Hash = HashCombine(Hash, GetTypeHash(Dependent));
    }
    return Hash;
}

auto FCkEcsRuntimeGraphModel::Clear() -> bool
{
    const auto bWasPopulated = NOT _Nodes.IsEmpty() || NOT _Edges.IsEmpty() ||
                               _LastTopologyHash != 0;
    _Nodes.Reset();
    _Edges.Reset();
    _LastBuiltEntity = {};
    _LastTopologyHash = 0;
    return bWasPopulated;
}

auto FCkEcsRuntimeGraphModel::RebuildFromEntity(const FCk_Handle& InEntity) -> bool
{
    if (ck::Is_NOT_Valid(InEntity))
    {
        return Clear();
    }

    auto Owners = TArray<ck_ecs_runtime_graph_model::FRelatedEntity>{};
    auto Dependents = TArray<ck_ecs_runtime_graph_model::FRelatedEntity>{};
    auto LifetimeOwner = FCk_Handle{};
    auto ContextOwner = FCk_Handle{};

    if (InEntity.Has<ck::FFragment_LifetimeOwner>())
    {
        LifetimeOwner = InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        if (ck::IsValid(LifetimeOwner))
        {
            Owners.Add({LifetimeOwner, ECkEcsRuntimeGraphRelationship::LifetimeOwner});
        }
    }

    if (UCk_Utils_ContextOwner_UE::Has(InEntity))
    {
        ContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(InEntity);
        auto bAlreadyAdded = false;
        for (const auto& Existing : Owners)
        {
            if (Existing.Entity == ContextOwner)
            {
                bAlreadyAdded = true;
                break;
            }
        }
        if (ck::IsValid(ContextOwner) && NOT bAlreadyAdded)
        {
            Owners.Add({ContextOwner, ECkEcsRuntimeGraphRelationship::ContextOwner});
        }
    }

    for (const auto& Dependent : UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InEntity))
    {
        if (ck::IsValid(Dependent))
        {
            Dependents.Add({Dependent, ECkEcsRuntimeGraphRelationship::LifetimeDependent});
        }
    }
    Dependents.Sort(
        [](const auto& InLeft, const auto& InRight)
        {
            return InLeft.Entity < InRight.Entity;
        });

    auto DependentHandles = TArray<FCk_Handle>{};
    DependentHandles.Reserve(Dependents.Num());
    for (const auto& Dependent : Dependents)
    {
        DependentHandles.Add(Dependent.Entity);
    }

    const auto NewTopologyHash =
        ComputeTopologyHash(InEntity, LifetimeOwner, ContextOwner, DependentHandles);
    if (_LastBuiltEntity == InEntity && _LastTopologyHash == NewTopologyHash)
    {
        return false;
    }

    auto LayoutInput = FCkEcsRuntimeGraphLayoutInput{};
    LayoutInput.CenterLabel = ck_ecs_runtime_graph_model::MakeLabel(InEntity);
    for (const auto& Owner : Owners)
    {
        LayoutInput.OwnerLabels.Add(ck_ecs_runtime_graph_model::MakeLabel(Owner.Entity));
    }
    for (const auto& Dependent : Dependents)
    {
        LayoutInput.DependentLabels.Add(ck_ecs_runtime_graph_model::MakeLabel(Dependent.Entity));
    }
    const auto Layout = ck::ecs_runtime_graph::BuildLayout(LayoutInput);

    auto NewNodes = TArray<TSharedPtr<FCkEcsRuntimeGraphNode>>{};
    auto NewEdges = TArray<FCkEcsRuntimeGraphEdge>{};
    const auto CenterId = MakeStableId(InEntity);
    auto CenterNode = MakeShared<FCkEcsRuntimeGraphNode>();
    CenterNode->StableId = CenterId;
    CenterNode->Entity = InEntity;
    CenterNode->DisplayName = LayoutInput.CenterLabel;
    CenterNode->AccentColor = CkStyle::TextStrong();
    CenterNode->Position = Layout.CenterPosition;
    CenterNode->bIsCenterNode = true;
    NewNodes.Add(MoveTemp(CenterNode));

    auto AddRelatedNodes =
        [&NewNodes,
         &NewEdges,
         &CenterId](const TArray<ck_ecs_runtime_graph_model::FRelatedEntity>& InRelated,
                    const TArray<FString>& InLabels,
                    const TArray<FIntPoint>& InPositions)
    {
        for (auto Index = 0; Index < InRelated.Num(); ++Index)
        {
            const auto& Related = InRelated[Index];
            const auto RelationshipColor = ck::ecs_runtime_graph::GetRelationshipColor(
                Related.Relationship);
            const auto StableId = FCkEcsRuntimeGraphModel::MakeStableId(Related.Entity);
            auto Node = MakeShared<FCkEcsRuntimeGraphNode>();
            Node->StableId = StableId;
            Node->Entity = Related.Entity;
            Node->DisplayName = InLabels[Index];
            Node->AccentColor = RelationshipColor;
            Node->Position = InPositions[Index];
            Node->Relationship = Related.Relationship;
            NewNodes.Add(MoveTemp(Node));

            auto Edge = FCkEcsRuntimeGraphEdge{};
            Edge.SourceNodeId = CenterId;
            Edge.TargetNodeId = StableId;
            Edge.Color = RelationshipColor;
            Edge.Relationship = Related.Relationship;
            Edge.bIsDirected = true;
            NewEdges.Add(MoveTemp(Edge));
        }
    };
    AddRelatedNodes(Owners, LayoutInput.OwnerLabels, Layout.OwnerPositions);
    AddRelatedNodes(Dependents, LayoutInput.DependentLabels, Layout.DependentPositions);

    _Nodes = MoveTemp(NewNodes);
    _Edges = MoveTemp(NewEdges);
    _LastBuiltEntity = InEntity;
    _LastTopologyHash = NewTopologyHash;
    return true;
}
