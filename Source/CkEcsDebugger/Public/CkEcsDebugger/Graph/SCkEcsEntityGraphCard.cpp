#include "CkEcsDebugger/Graph/SCkEcsEntityGraphCard.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"
#include "Widgets/Layout/SBorder.h"

auto SCkEcsEntityGraphCard::Construct(const FArguments& InArgs) -> void
{
    _Node = InArgs._Node;
    _bSelected = InArgs._bSelected;
    RebuildCard();
}

auto SCkEcsEntityGraphCard::SetNode(TSharedPtr<FCkEcsRuntimeGraphNode> InNode) -> void
{
    const auto bRequiresRebuild = NOT _Node.IsValid() || NOT InNode.IsValid() ||
                                  _Node->DisplayName != InNode->DisplayName ||
                                  _Node->Relationship != InNode->Relationship ||
                                  _Node->bIsCenterNode != InNode->bIsCenterNode;
    _Node = MoveTemp(InNode);
    if (bRequiresRebuild)
    {
        RebuildCard();
    }
}

auto SCkEcsEntityGraphCard::SetSelected(const bool bInSelected) -> void
{
    _bSelected = bInSelected;
    if (_NodePill.IsValid())
    {
        _NodePill->Set_Selected(_bSelected);
    }
}

auto SCkEcsEntityGraphCard::RebuildCard() -> void
{
    const auto WeakCard = TWeakPtr<SCkEcsEntityGraphCard>(
        StaticCastSharedRef<SCkEcsEntityGraphCard>(AsShared()));
    const auto Title = _Node.IsValid() ? FText::FromString(_Node->DisplayName) : FText::GetEmpty();
    const auto Variant = GetVariant();

    ChildSlot[SAssignNew(_NodePill, SCkDebug_NodePill)
                  .Variant(Variant)
                  .StepIndex(-1)
                  .ShowCost(false)
                  .Title(Title)
                  .AccentColor_Lambda(
                      [WeakCard]()
                      {
                          const auto Card = WeakCard.Pin();
                          return Card.IsValid() ? Card->GetAccentColor()
                                                : FLinearColor::Transparent;
                      })
                  .BorderColorOverride_Lambda(
                      [WeakCard]()
                      {
                          const auto Card = WeakCard.Pin();
                          return Card.IsValid() ? Card->GetBorderColor()
                                                : FLinearColor::Transparent;
                      })
                  .OpacityOverride_Lambda(
                      [WeakCard]()
                      {
                          const auto Card = WeakCard.Pin();
                          return Card.IsValid() ? Card->GetOpacity() : 0.0f;
                      })
                  .Selected(_bSelected)];
}

auto SCkEcsEntityGraphCard::GetAccentColor() const -> FLinearColor
{
    return _Node.IsValid() ? _Node->AccentColor : FLinearColor::Transparent;
}

auto SCkEcsEntityGraphCard::GetBorderColor() const -> FLinearColor
{
    return _Node.IsValid() ? ck::ecs_runtime_graph::GetRelationshipColor(_Node->Relationship)
                           : FLinearColor::Transparent;
}

auto SCkEcsEntityGraphCard::GetOpacity() const -> float
{
    return _Node.IsValid() && NOT _Node->bIsFilterMatch ? 0.30f : 1.0f;
}

auto SCkEcsEntityGraphCard::GetVariant() const -> ECkDebug_NodePillVariant
{
    return _Node.IsValid() && _Node->bIsCenterNode ? ECkDebug_NodePillVariant::InPlan
                                                   : ECkDebug_NodePillVariant::Inactive;
}
