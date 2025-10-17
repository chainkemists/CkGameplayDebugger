#include "CkDebuggerWidget_EntityTree.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkCore/Game/CkGame_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "SlateIM/Public/SlateIM.h"

auto FCkDebuggerWidget_EntityTree::Initialize(
    TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
    TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel) -> void
{
    SelectionModel = InSelectionModel;
    WorldModel = InWorldModel;
    bNeedsRebuild = true;
}

auto FCkDebuggerWidget_EntityTree::Draw() -> void
{
    if (!WorldModel.IsValid() || !SelectionModel.IsValid())
    {
        SlateIM::Text(TEXT("No world or selection model"));
        return;
    }
    
    // Rebuild tree if needed
    if (bNeedsRebuild || WorldModel->IsCacheDirty())
    {
        WorldModel->Refresh_EntityCache();
        BuildTreeFromEntities(WorldModel->Get_CachedEntities());
        ApplyFilter();
        bNeedsRebuild = false;
    }
    
    // Draw tree
    SlateIM::Fill();
    SlateIM::BeginScrollBox(Orient_Vertical);
    {
        for (const auto& Node : FlattenedNodes)
        {
            if (Node->bIsVisible)
            {
                DrawNode(Node);
            }
        }
        
        if (FlattenedNodes.Num() == 0)
        {
            SlateIM::Text(TEXT("No entities found"));
        }
    }
    SlateIM::EndScrollBox();
}

auto FCkDebuggerWidget_EntityTree::SetFilterText(const FString& InFilterText) -> void
{
    if (FilterText == InFilterText)
    {
        return;
    }
    
    FilterText = InFilterText;
    ApplyFilter();
}

auto FCkDebuggerWidget_EntityTree::RefreshTree() -> void
{
    bNeedsRebuild = true;
}

auto FCkDebuggerWidget_EntityTree::ExpandAll() -> void
{
    for (const auto& Node : RootNodes)
    {
        SetExpanded(Node, true, true);
    }
}

auto FCkDebuggerWidget_EntityTree::CollapseAll() -> void
{
    for (const auto& Node : RootNodes)
    {
        SetExpanded(Node, false, true);
    }
}

auto FCkDebuggerWidget_EntityTree::BuildTreeFromEntities(const TArray<FCk_Handle>& InEntities) -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(BuildTreeFromEntities);
    
    RootNodes.Empty();
    FlattenedNodes.Empty();
    
    if (!WorldModel.IsValid())
    {
        return;
    }
    
    const auto World = WorldModel->Get_SelectedWorld();
    if (!ck::IsValid(World))
    {
        return;
    }
    
    // Build map of entities to their nodes
    TMap<FCk_Handle, TSharedPtr<FCkEntityTreeNode>> EntityToNode;
    
    for (const auto& Entity : InEntities)
    {
        if (!ck::IsValid(Entity))
        {
            continue;
        }
        
        auto Node = MakeShared<FCkEntityTreeNode>();
        Node->Entity = Entity;
        EntityToNode.Add(Entity, Node);
    }
    
    // Build hierarchy
    for (const auto& Entity : InEntities)
    {
        if (!ck::IsValid(Entity))
        {
            continue;
        }
        
        auto Node = EntityToNode[Entity];
        
        // Find parent
        if (Entity.Has<ck::FFragment_LifetimeOwner>())
        {
            const auto ParentEntity = Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
            
            if (EntityToNode.Contains(ParentEntity) && 
                !UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(ParentEntity))
            {
                auto ParentNode = EntityToNode[ParentEntity];
                Node->Parent = ParentNode;
                Node->IndentLevel = ParentNode->IndentLevel + 1;
                ParentNode->Children.Add(Node);
                continue;
            }
        }
        
        // No valid parent, add to root
        RootNodes.Add(Node);
    }
    
    // Sort children alphabetically
    for (auto& [Entity, Node] : EntityToNode)
    {
        if (Node->Children.Num() > 0)
        {
            Node->Children.Sort([](const TSharedPtr<FCkEntityTreeNode>& A, const TSharedPtr<FCkEntityTreeNode>& B)
            {
                const auto NameA = UCk_Utils_Handle_UE::Get_DebugName(A->Entity).ToString();
                const auto NameB = UCk_Utils_Handle_UE::Get_DebugName(B->Entity).ToString();
                return NameA < NameB;
            });
        }
    }
    
    // Sort root nodes
    RootNodes.Sort([](const TSharedPtr<FCkEntityTreeNode>& A, const TSharedPtr<FCkEntityTreeNode>& B)
    {
        const auto NameA = UCk_Utils_Handle_UE::Get_DebugName(A->Entity).ToString();
        const auto NameB = UCk_Utils_Handle_UE::Get_DebugName(B->Entity).ToString();
        return NameA < NameB;
    });
    
    // Flatten tree for rendering
    TFunction<void(const TSharedPtr<FCkEntityTreeNode>&)> FlattenNode;
    FlattenNode = [&](const TSharedPtr<FCkEntityTreeNode>& Node)
    {
        FlattenedNodes.Add(Node);
        if (Node->bIsExpanded)
        {
            for (const auto& Child : Node->Children)
            {
                FlattenNode(Child);
            }
        }
    };
    
    for (const auto& Root : RootNodes)
    {
        FlattenNode(Root);
    }
}

auto FCkDebuggerWidget_EntityTree::ApplyFilter() -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(ApplyFilter);
    
    VisibleCount = 0;
    
    if (FilterText.IsEmpty())
    {
        // No filter, show all
        TFunction<void(const TSharedPtr<FCkEntityTreeNode>&)> ShowAll;
        ShowAll = [&](const TSharedPtr<FCkEntityTreeNode>& Node)
        {
            Node->bIsVisible = true;
            Node->bMatchesFilter = false;
            VisibleCount++;
            for (const auto& Child : Node->Children)
            {
                ShowAll(Child);
            }
        };
        
        for (const auto& Root : RootNodes)
        {
            ShowAll(Root);
        }
        return;
    }
    
    const auto FilterLower = FilterText.ToLower();
    
    // First pass: mark nodes that match filter
    TFunction<bool(const TSharedPtr<FCkEntityTreeNode>&)> MarkMatches;
    MarkMatches = [&](const TSharedPtr<FCkEntityTreeNode>& Node) -> bool
    {
        const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Node->Entity).ToString().ToLower();
        const bool bThisMatches = DebugName.Contains(FilterLower);
        
        bool bAnyChildMatches = false;
        for (const auto& Child : Node->Children)
        {
            if (MarkMatches(Child))
            {
                bAnyChildMatches = true;
            }
        }
        
        Node->bMatchesFilter = bThisMatches;
        Node->bIsVisible = bThisMatches || bAnyChildMatches;
        
        // Auto-expand nodes with matching children
        if (bAnyChildMatches)
        {
            Node->bIsExpanded = true;
        }
        
        if (Node->bIsVisible)
        {
            VisibleCount++;
        }
        
        return Node->bIsVisible;
    };
    
    for (const auto& Root : RootNodes)
    {
        MarkMatches(Root);
    }
    
    // Rebuild flattened list
    FlattenedNodes.Empty();
    TFunction<void(const TSharedPtr<FCkEntityTreeNode>&)> FlattenNode;
    FlattenNode = [&](const TSharedPtr<FCkEntityTreeNode>& Node)
    {
        if (!Node->bIsVisible)
        {
            return;
        }
        
        FlattenedNodes.Add(Node);
        if (Node->bIsExpanded)
        {
            for (const auto& Child : Node->Children)
            {
                FlattenNode(Child);
            }
        }
    };
    
    for (const auto& Root : RootNodes)
    {
        FlattenNode(Root);
    }
}

auto FCkDebuggerWidget_EntityTree::DrawNode(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void
{
    if (!InNode.IsValid() || !ck::IsValid(InNode->Entity))
    {
        return;
    }
    
    const auto& Entity = InNode->Entity;
    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity).ToString();
    const bool bIsSelected = IsNodeSelected(Entity);
    const bool bIsLocalPlayer = ShouldHighlightAsLocalPlayer(Entity);
    const bool bHasChildren = InNode->Children.Num() > 0;
    
    SlateIM::BeginHorizontalStack();
    {
        // Indentation
        if (InNode->IndentLevel > 0)
        {
            SlateIM::AutoSize();
            SlateIM::Spacer(FVector2D(InNode->IndentLevel * 20.0f, 0.0f));
        }
        
        // Expand/collapse indicator
        SlateIM::AutoSize();
        SlateIM::MinWidth(20.0f);
        if (bHasChildren)
        {
            const FString ExpandIcon = InNode->bIsExpanded ? TEXT("v") : TEXT(">");
            if (SlateIM::Button(ExpandIcon))
            {
                InNode->bIsExpanded = !InNode->bIsExpanded;
                ApplyFilter(); // Rebuild flattened list
            }
        }
        else
        {
            SlateIM::Spacer(FVector2D(20.0f, 0.0f));
        }
        
        // Entity name as clickable text with border
        SlateIM::Fill();
        SlateIM::HAlign(HAlign_Left);
        
        FString DisplayText = FString::Printf(TEXT("%s [%s]"), 
            *DebugName, 
            *Entity.Get_Entity().ToString());
        
        // Add selection indicator prefix
        if (bIsSelected)
        {
            DisplayText = TEXT("► ") + DisplayText;
        }
        
        // Determine text color
        FLinearColor TextColor = FLinearColor::White;
        if (bIsLocalPlayer)
        {
            TextColor = FLinearColor::Yellow;
        }
        else if (bIsSelected)
        {
            TextColor = FLinearColor(0.5f, 0.8f, 1.0f); // Light blue
        }
        else if (!FilterText.IsEmpty() && !InNode->bMatchesFilter)
        {
            TextColor = FLinearColor(0.6f, 0.6f, 0.6f); // Dimmed for context
        }
        
        // Draw text
        SlateIM::Text(DisplayText, TextColor);
        
        // Check for click on the text
        if (SlateIM::IsHovered())
        {
            if (SlateIM::IsKeyPressed(EKeys::LeftMouseButton))
            {
                const bool bCtrlHeld = FSlateApplication::Get().GetModifierKeys().IsControlDown();
                HandleNodeClick(InNode, bCtrlHeld);
            }
        }
    }
    SlateIM::EndHorizontalStack();
}

auto FCkDebuggerWidget_EntityTree::HandleNodeClick(
    const TSharedPtr<FCkEntityTreeNode>& InNode, 
    bool bCtrlHeld) -> void
{
    if (!SelectionModel.IsValid() || !WorldModel.IsValid())
    {
        return;
    }
    
    const auto World = WorldModel->Get_SelectedWorld();
    if (!ck::IsValid(World))
    {
        return;
    }
    
    if (bCtrlHeld)
    {
        // Multi-select: toggle
        SelectionModel->Toggle_SelectedEntity(InNode->Entity);
    }
    else
    {
        // Single select: replace
        SelectionModel->Set_SelectedEntities({InNode->Entity});
    }
}

auto FCkDebuggerWidget_EntityTree::IsNodeSelected(const FCk_Handle& InEntity) const -> bool
{
    if (!SelectionModel.IsValid())
    {
        return false;
    }
    
    return SelectionModel->IsSelected(InEntity);
}

auto FCkDebuggerWidget_EntityTree::ShouldHighlightAsLocalPlayer(const FCk_Handle& InEntity) const -> bool
{
    if (!WorldModel.IsValid())
    {
        return false;
    }
    
    const auto World = WorldModel->Get_SelectedWorld();
    if (!ck::IsValid(World))
    {
        return false;
    }
    
    const auto EntityActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(InEntity);
    if (!ck::IsValid(EntityActor))
    {
        return false;
    }
    
    const auto LocalPlayer = World->GetFirstLocalPlayerFromController();
    if (!LocalPlayer)
    {
        return false;
    }
    
    const auto LocalPlayerController = LocalPlayer->PlayerController;
    if (!LocalPlayerController)
    {
        return false;
    }
    
    const auto LocalPawn = LocalPlayerController->GetPawn();
    return EntityActor == LocalPawn;
}

auto FCkDebuggerWidget_EntityTree::SetExpanded(
    const TSharedPtr<FCkEntityTreeNode>& InNode, 
    bool bExpanded, 
    bool bRecursive) -> void
{
    if (!InNode.IsValid())
    {
        return;
    }
    
    InNode->bIsExpanded = bExpanded;
    
    if (bRecursive)
    {
        for (const auto& Child : InNode->Children)
        {
            SetExpanded(Child, bExpanded, true);
        }
    }
    
    // Rebuild flattened list after expansion changes
    FlattenedNodes.Empty();
    TFunction<void(const TSharedPtr<FCkEntityTreeNode>&)> FlattenNode;
    FlattenNode = [&](const TSharedPtr<FCkEntityTreeNode>& Node)
    {
        if (!Node->bIsVisible)
        {
            return;
        }
        
        FlattenedNodes.Add(Node);
        if (Node->bIsExpanded)
        {
            for (const auto& Child : Node->Children)
            {
                FlattenNode(Child);
            }
        }
    };
    
    for (const auto& Root : RootNodes)
    {
        FlattenNode(Root);
    }
}

auto FCkDebuggerWidget_EntityTree::Get_VisibleEntityCount() const -> int32
{
    return VisibleCount;
}

auto FCkDebuggerWidget_EntityTree::Get_TotalEntityCount() const -> int32
{
    return WorldModel.IsValid() ? WorldModel->Get_CachedEntities().Num() : 0;
}