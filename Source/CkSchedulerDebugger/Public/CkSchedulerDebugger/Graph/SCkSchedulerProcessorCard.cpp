#include "CkSchedulerDebugger/Graph/SCkSchedulerProcessorCard.h"

#include "CkCore/Format/CkFormat.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkSchedulerProcessorCard::Construct(const FArguments& InArgs) -> void
{
    _Node = InArgs._Node;
    _Selected = InArgs._Selected;
    const auto Node = _Node.Pin();
    const auto Variant = Node.IsValid() && Node->Processor.IsGhost
                             ? ECkDebug_NodePillVariant::Inactive
                             : ECkDebug_NodePillVariant::InPlan;
    const auto MetaFont = ck::debug_axes::ScaledFont("Regular", CkStyle::NodeMetaFontSize());
    const auto WeakCard = TWeakPtr<SCkSchedulerProcessorCard>(SharedThis(this));

    ChildSlot[SAssignNew(_NodePill, SCkDebug_NodePill)
                  .Variant(Variant)
                  .StepIndex(-1)
                  .ShowCost(false)
                  .Title(Node.IsValid() ? FText::FromString(Node->Processor.DisplayName)
                                        : FText::GetEmpty())
                  .AccentColor_Lambda(
                      [WeakCard]()
                      {
                          const auto Card = WeakCard.Pin();
                          return Card.IsValid() ? Card->Get_GroupAccentColor()
                                                : FLinearColor::Transparent;
                      })
                  .BorderColorOverride_Lambda(
                      [WeakCard]()
                      {
                          const auto Card = WeakCard.Pin();
                          return Card.IsValid() ? Card->Get_DirtyBorderColor()
                                                : FLinearColor::Transparent;
                      })
                  .Selected(_Selected)
                  .BodyContent()[SNew(SHorizontalBox)

                                 + SHorizontalBox::Slot()
                                       .AutoWidth()
                                       .VAlign(VAlign_Center)
                                       .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                           [SNew(STextBlock)
                                                .Text_Lambda(
                                                    [WeakCard]()
                                                    {
                                                        const auto Card = WeakCard.Pin();
                                                        return Card.IsValid()
                                                                   ? Card->Get_TimingText()
                                                                   : FText::GetEmpty();
                                                    })
                                                .Font(MetaFont)
                                                .ColorAndOpacity_Lambda(
                                                    [WeakCard]()
                                                    {
                                                        const auto Card = WeakCard.Pin();
                                                        return Card.IsValid()
                                                                   ? Card->Get_TimingColor()
                                                                   : FSlateColor(
                                                                         FLinearColor::Transparent);
                                                    })]

                                 + SHorizontalBox::Slot()
                                       .AutoWidth()
                                       .VAlign(VAlign_Center)
                                       .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                           [SNew(STextBlock)
                                                .Text_Lambda(
                                                    [WeakCard]()
                                                    {
                                                        const auto Card = WeakCard.Pin();
                                                        return Card.IsValid()
                                                                   ? Card->Get_ExecutionOrderText()
                                                                   : FText::GetEmpty();
                                                    })
                                                .Font(MetaFont)
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))]

                                 + SHorizontalBox::Slot()
                                       .AutoWidth()
                                       .VAlign(VAlign_Center)
                                       .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                                           [SNew(STextBlock)
                                                .Text(FText::FromString(FString(UTF8TEXT("●"))))
                                                .Font(MetaFont)
                                                .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
                                                .Visibility_Lambda(
                                                    [WeakCard]()
                                                    {
                                                        const auto Card = WeakCard.Pin();
                                                        return Card.IsValid()
                                                                   ? Card->Get_DirtyVisibility()
                                                                   : EVisibility::Collapsed;
                                                    })]

                                 + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                       [SNew(STextBlock)
                                            .Text(FText::FromString(FString(UTF8TEXT("◆"))))
                                            .Font(MetaFont)
                                            .ColorAndOpacity(FSlateColor(CkStyle::Info()))
                                            .Visibility_Lambda(
                                                [WeakCard]()
                                                {
                                                    const auto Card = WeakCard.Pin();
                                                    return Card.IsValid()
                                                               ? Card->Get_ParallelVisibility()
                                                               : EVisibility::Collapsed;
                                                })]]];
}

auto SCkSchedulerProcessorCard::Set_Selected(bool InSelected) -> void
{
    if (_Selected == InSelected)
    {
        return;
    }

    _Selected = InSelected;
    if (_NodePill.IsValid())
    {
        _NodePill->Set_Selected(_Selected);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkSchedulerProcessorCard::Get_TimingText() const -> FText
{
    const auto Node = _Node.Pin();
    return Node.IsValid()
               ? FText::FromString(ck::Format_UE(TEXT("{:.3f} ms"), Node->Processor.MainPassTimeMs))
               : FText::GetEmpty();
}

auto SCkSchedulerProcessorCard::Get_TimingColor() const -> FSlateColor
{
    const auto Node = _Node.Pin();
    return FSlateColor(
        Node.IsValid() ? FCkSchedulerDebuggerStyle::Get_TimingColor(Node->Processor.MainPassTimeMs)
                       : CkStyle::TextMute());
}

auto SCkSchedulerProcessorCard::Get_ExecutionOrderText() const -> FText
{
    const auto Node = _Node.Pin();
    return Node.IsValid()
               ? FText::FromString(ck::Format_UE(TEXT("#{}"), Node->Processor.ExecutionOrder))
               : FText::GetEmpty();
}

auto SCkSchedulerProcessorCard::Get_DirtyVisibility() const -> EVisibility
{
    const auto Node = _Node.Pin();
    return Node.IsValid() && Node->Processor.HasDirtyMarker ? EVisibility::Visible
                                                            : EVisibility::Collapsed;
}

auto SCkSchedulerProcessorCard::Get_ParallelVisibility() const -> EVisibility
{
    const auto Node = _Node.Pin();
    return Node.IsValid() && Node->Processor.IsParallel ? EVisibility::Visible
                                                        : EVisibility::Collapsed;
}

auto SCkSchedulerProcessorCard::Get_DirtyBorderColor() const -> FLinearColor
{
    const auto Node = _Node.Pin();
    return Node.IsValid() && Node->Processor.WasDirtyThisFrame ? CkStyle::Warn()
                                                               : FLinearColor::Transparent;
}

auto SCkSchedulerProcessorCard::Get_GroupAccentColor() const -> FLinearColor
{
    const auto Node = _Node.Pin();
    return Node.IsValid() ? FCkSchedulerDebuggerStyle::Get_GroupColor(Node->Processor.GroupName)
                          : FLinearColor::Transparent;
}

// --------------------------------------------------------------------------------------------------------------------
