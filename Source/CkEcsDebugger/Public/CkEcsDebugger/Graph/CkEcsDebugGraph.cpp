#include "CkEcsDebugGraph.h"

#include "CkEcsDebugNode_Entity.h"
#include "CkEcsDebugGraphSchema.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"

#include "EdGraph/EdGraphPin.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
// =====================================================================================================================

auto
    UCkEcsDebugGraph::
    RebuildFromEntity(
        const FCk_Handle& InEntity)
    -> bool
{
    if (ck::Is_NOT_Valid(InEntity))
    {
        if (_EntityToNode.Num() > 0)
        {
            ClearGraph();
            _LastBuiltEntity = FCk_Handle();
            return true;
        }
        return false;
    }

    // Simple topology hash: entity handle + dependent count
    auto DependentCount = 0;
    if (InEntity.Has<ck::FFragment_LifetimeDependents>())
    {
        DependentCount = UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InEntity).Num();
    }

    auto NewHash = GetTypeHash(InEntity) ^ GetTypeHash(DependentCount);

    if (NewHash == _TopologyHash && InEntity == _LastBuiltEntity && _TopologyHash != 0)
    {
        return false;
    }

    ClearGraph();
    GatherAndBuild(InEntity);

    _TopologyHash = NewHash;
    _LastBuiltEntity = InEntity;

    NotifyGraphChanged();
    return true;
}

// =====================================================================================================================

auto
    UCkEcsDebugGraph::
    FindNodeByEntity(
        const FCk_Handle& InEntity) const
    -> UCkEcsDebugNode_Entity*
{
    if (auto* Found = _EntityToNode.Find(InEntity))
    {
        return *Found;
    }
    return nullptr;
}

// =====================================================================================================================

auto
    UCkEcsDebugGraph::
    ClearGraph()
    -> void
{
    SetSuppressNotifications(true);

    // Remove all nodes
    auto NodesToRemove = TArray<UEdGraphNode*>(Nodes);
    for (auto* Node : NodesToRemove)
    {
        RemoveNode(Node);
    }

    _EntityToNode.Reset();
    _TopologyHash = 0;

    SetSuppressNotifications(false);
}

// =====================================================================================================================

auto
    UCkEcsDebugGraph::
    GatherAndBuild(
        const FCk_Handle& InEntity)
    -> void
{
    SetSuppressNotifications(true);

    // Gather relationships
    struct FRelatedEntity
    {
        FCk_Handle Entity;
        ECkEcsDebugEdgeType EdgeType;
        FLinearColor Color;
    };

    auto Owners = TArray<FRelatedEntity>{};
    auto Dependents = TArray<FRelatedEntity>{};

    // Lifetime owner
    if (InEntity.Has<ck::FFragment_LifetimeOwner>())
    {
        const auto& Owner = InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        if (ck::IsValid(Owner))
        {
            Owners.Add({ Owner, ECkEcsDebugEdgeType::LifetimeOwner, CkDebugStyle::Relationship() });
        }
    }

    // Context owner
    if (UCk_Utils_ContextOwner_UE::Has(InEntity))
    {
        const auto ContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(InEntity);
        if (ck::IsValid(ContextOwner) && NOT _EntityToNode.Contains(ContextOwner))
        {
            // Avoid duplicate if context owner == lifetime owner
            auto bAlreadyAdded = false;
            for (const auto& Existing : Owners)
            {
                if (Existing.Entity == ContextOwner) { bAlreadyAdded = true; break; }
            }
            if (NOT bAlreadyAdded)
            {
                Owners.Add({ ContextOwner, ECkEcsDebugEdgeType::ContextOwner, CkDebugStyle::Reference() });
            }
        }
    }

    // All lifetime dependents
    const auto LifetimeDependents = UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InEntity);
    for (const auto& Dependent : LifetimeDependents)
    {
        if (ck::IsValid(Dependent))
        {
            Dependents.Add({ Dependent, ECkEcsDebugEdgeType::LifetimeDependent, CkDebugStyle::Transform() });
        }
    }

    // Helper: estimate node width from label length
    auto EstimateNodeWidth = [this](const FString& InLabel) -> int32
    {
        return FMath::Max(
            LayoutParams.MinNodeWidth,
            static_cast<int32>(InLabel.Len() * LayoutParams.CharWidthEstimate) + LayoutParams.NodePaddingX);
    };

    auto Gap = LayoutParams.NodeGap;

    // Pre-compute labels
    auto CenterLabel = MakeEntityLabel(InEntity);

    auto OwnerLabels = TArray<FString>{};
    for (const auto& O : Owners) { OwnerLabels.Add(MakeEntityLabel(O.Entity)); }

    auto DepLabels = TArray<FString>{};
    for (const auto& D : Dependents) { DepLabels.Add(MakeEntityLabel(D.Entity)); }

    // --- Compute all row widths to determine dynamic SpacingY ---
    auto MaxPerRow = LayoutParams.MaxDependentsPerRow;

    // Owner row total width
    auto OwnerWidths = TArray<int32>{};
    auto OwnerTotalWidth = 0;
    for (auto i = 0; i < Owners.Num(); ++i)
    {
        auto W = EstimateNodeWidth(OwnerLabels[i]);
        OwnerWidths.Add(W);
        OwnerTotalWidth += W;
    }
    if (Owners.Num() > 1) { OwnerTotalWidth += Gap * (Owners.Num() - 1); }

    // Center node width
    auto CenterWidth = EstimateNodeWidth(CenterLabel);

    // Dependent row widths — find the widest row
    auto MaxDepRowWidth = 0;
    for (auto RowStart = 0; RowStart < Dependents.Num(); RowStart += MaxPerRow)
    {
        auto RowSize = FMath::Min(MaxPerRow, Dependents.Num() - RowStart);
        auto RowWidth = 0;
        for (auto j = RowStart; j < RowStart + RowSize; ++j)
        {
            RowWidth += EstimateNodeWidth(DepLabels[j]);
        }
        if (RowSize > 1) { RowWidth += Gap * (RowSize - 1); }
        MaxDepRowWidth = FMath::Max(MaxDepRowWidth, RowWidth);
    }

    // Dynamic SpacingY: at least MinSpacingY, but scale with widest row so layers stay distinct
    auto WidestRow = FMath::Max3(OwnerTotalWidth, CenterWidth, MaxDepRowWidth);
    auto DynamicSpacingY = FMath::Max(LayoutParams.SpacingY, WidestRow / 6);

    // Layout positions
    auto CenterX = 0;
    auto CenterY = DynamicSpacingY;

    // --- Owner layer (above center) ---
    auto OwnerY = 0;

    auto OwnerPositions = TArray<int32>{};
    {
        auto CurX = -OwnerTotalWidth / 2;
        for (auto i = 0; i < Owners.Num(); ++i)
        {
            OwnerPositions.Add(CurX + OwnerWidths[i] / 2);
            CurX += OwnerWidths[i] + Gap;
        }
    }

    // --- Dependent layer (below center) ---
    auto DepStartY = CenterY + DynamicSpacingY;

    // Create center node
    auto* CenterNode = CreateEntityNode(
        InEntity,
        CenterLabel,
        CkDebugStyle::TextStrong(),
        true,
        ECkEcsDebugEdgeType::None,
        CenterX,
        CenterY);

    // Create owner nodes
    for (auto i = 0; i < Owners.Num(); ++i)
    {
        const auto& OwnerData = Owners[i];

        auto* OwnerNode = CreateEntityNode(
            OwnerData.Entity,
            OwnerLabels[i],
            OwnerData.Color,
            false,
            OwnerData.EdgeType,
            OwnerPositions[i],
            OwnerY);

        ConnectNodes(CenterNode, OwnerNode);
    }

    // Create dependent nodes — compute positions per row based on actual widths
    for (auto i = 0; i < Dependents.Num(); ++i)
    {
        const auto& DepData = Dependents[i];
        auto RowIndex = i / MaxPerRow;
        auto ColIndex = i % MaxPerRow;
        auto RowStart = RowIndex * MaxPerRow;
        auto RowSize = FMath::Min(MaxPerRow, Dependents.Num() - RowStart);

        // Compute row layout with gaps
        auto RowTotalWidth = 0;
        auto RowWidths = TArray<int32>{};
        for (auto j = RowStart; j < RowStart + RowSize; ++j)
        {
            auto W = EstimateNodeWidth(DepLabels[j]);
            RowWidths.Add(W);
            RowTotalWidth += W;
        }
        if (RowSize > 1) { RowTotalWidth += Gap * (RowSize - 1); }

        auto CurX = -RowTotalWidth / 2;
        auto PosX = CurX;
        for (auto j = 0; j <= ColIndex; ++j)
        {
            PosX = CurX + RowWidths[j] / 2;
            CurX += RowWidths[j] + Gap;
        }
        auto PosY = DepStartY + RowIndex * LayoutParams.SpacingY;

        auto* DepNode = CreateEntityNode(
            DepData.Entity,
            DepLabels[i],
            DepData.Color,
            false,
            DepData.EdgeType,
            PosX,
            PosY);

        // Wire: center → dependent (center's output to dependent's input)
        ConnectNodes(CenterNode, DepNode);
    }

    SetSuppressNotifications(false);
}

// =====================================================================================================================

auto
    UCkEcsDebugGraph::
    CreateEntityNode(
        const FCk_Handle& InEntity,
        const FString& InDisplayName,
        const FLinearColor& InAccentColor,
        bool InIsCenterNode,
        ECkEcsDebugEdgeType InEdgeType,
        int32 InPosX,
        int32 InPosY)
    -> UCkEcsDebugNode_Entity*
{
    auto* Node = NewObject<UCkEcsDebugNode_Entity>(this);
    Node->Populate(InEntity, InDisplayName, InAccentColor, InIsCenterNode, InEdgeType);

    Node->NodePosX = InPosX;
    Node->NodePosY = InPosY;

    Node->SetFlags(RF_Transactional);
    AddNode(Node, false, false);
    Node->AllocateDefaultPins();

    _EntityToNode.Add(InEntity, Node);

    return Node;
}

// =====================================================================================================================

auto
    UCkEcsDebugGraph::
    ConnectNodes(
        UCkEcsDebugNode_Entity* InSourceNode,
        UCkEcsDebugNode_Entity* InTargetNode)
    -> void
{
    if (NOT InSourceNode || NOT InTargetNode)
    { return; }

    // Find output pin on source, input pin on target
    UEdGraphPin* OutputPin = nullptr;
    UEdGraphPin* InputPin = nullptr;

    for (auto* Pin : InSourceNode->Pins)
    {
        if (Pin->Direction == EGPD_Output) { OutputPin = Pin; break; }
    }

    for (auto* Pin : InTargetNode->Pins)
    {
        if (Pin->Direction == EGPD_Input) { InputPin = Pin; break; }
    }

    if (OutputPin && InputPin)
    {
        OutputPin->MakeLinkTo(InputPin);
    }
}

// =====================================================================================================================

auto
    UCkEcsDebugGraph::
    MakeEntityLabel(
        const FCk_Handle& InEntity)
    -> FString
{
    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InEntity);
    return DebugName.ToString();
}

// =====================================================================================================================
