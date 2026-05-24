#include "SGraphNode_GoapAction.h"

#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"

#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

namespace
{
    auto Compute_TagLeaf(const FGameplayTag& InTag) -> FString
    {
        const auto Full = InTag.ToString();
        int32 Idx = INDEX_NONE;
        if (Full.FindLastChar(TEXT('.'), Idx) && Idx != INDEX_NONE)
        { return Full.Mid(Idx + 1); }
        return Full;
    }

    // Outer border color for the action card. Priority: selected > failed >
    // in-plan > composite > atomic leaf. The atomic-leaf tint matches the
    // legend's "Leaf — atomic action" swatch (blue #60a5fa). Falls back to
    // Color_Border_Strong only if the node pointer is null (invariant
    // violation — every real action node carries the leaf path).
    auto Compute_BorderColor(const UCkGoapDebugNode_Action* InNode) -> FLinearColor
    {
        if (NOT InNode) { return FCkGoapDebuggerStyle::Color_Border_Strong; }

        if (InNode->Get_IsSelected())
        { return FCkGoapDebuggerStyle::Color_Status_Selected; }

        if (InNode->Get_IsFailureBlocked())
        { return FCkGoapDebuggerStyle::Color_Status_Failed; }

        if (InNode->Get_IsInPlan())
        { return FCkGoapDebuggerStyle::Color_Status_PlanningBdr; }

        if (InNode->Get_IsComposite())
        { return FCkGoapDebuggerStyle::Color_Status_Composite; }

        // Atomic leaf — explicit tint matching the legend.
        return FCkGoapDebuggerStyle::Color_Status_Planning;
    }

    // Small filled circle for port indicator dots.
    auto MakeDot(const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(7.0f)
            .HeightOverride(7.0f)
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(InColor)
            ];
    }
}

// ====================================================================================================================

auto
    SGraphNode_GoapAction::
    Construct(
        const FArguments& /*InArgs*/,
        UCkGoapDebugNode_Action* InNode)
    -> void
{
    _ActionNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// ====================================================================================================================

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_GoapAction::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    if (NOT _ActionNode)
    {
        GetOrAddSlot(ENodeZone::Center)
        [
            SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Surface)
        ];
        return;
    }

    const auto& Snap = _ActionNode->Get_Snapshot();

    // Live-binding capture: TWeakObjectPtr is robust to PIE teardown — Slate
    // outlives the UObject during world transitions. All TAttribute lambdas
    // below null-check via .Get() before deref.
    const auto WeakNode = TWeakObjectPtr<UCkGoapDebugNode_Action>(_ActionNode);

    // Header row: name + cost. ClassName and Cost are snapshot fields that
    // only change on topology rebuild (which spawns a new widget), so they
    // are safe to capture at construction time. The display-depth is read
    // off the owning graph so toggling the toolbar's name-depth control
    // re-renders every action card (the topology hash includes NameDepth, so
    // a depth change forces a rebuild via the graph pane's gate).
    const auto OwningDepth = [this]() -> int32
    {
        if (auto* G = Cast<UCkGoapDebugGraph>(GraphNode != nullptr ? GraphNode->GetGraph() : nullptr))
        { return G->NameDepth; }
        return 1;
    }();

    auto Header = SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FCkGoapDebugger_NameParams::ComputeDisplayName(Snap.ClassName, OwningDepth)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("$%.0f"), Snap.Cost)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
            ];

    // Composite bar — small purple "▸ leaf" strip just below the header.
    // ActionTag is a snapshot field (set on topology rebuild). Visibility is
    // bound live to Get_IsComposite() so that future per-tick flips don't
    // require widget recreation.
    const auto CompositeText = FString::Printf(TEXT("\x25B8 %s"), *Compute_TagLeaf(Snap.ActionTag));

    auto CompositeBar = SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FLinearColor(
            FCkGoapDebuggerStyle::Color_Status_Composite.R,
            FCkGoapDebuggerStyle::Color_Status_Composite.G,
            FCkGoapDebuggerStyle::Color_Status_Composite.B,
            0.15f))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 1.0f))
        .Visibility(TAttribute<EVisibility>::CreateLambda([WeakNode]()
        {
            const auto* Node = WeakNode.Get();
            if (Node == nullptr) { return EVisibility::Collapsed; }
            return Node->Get_IsComposite() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
        }))
        [
            SNew(STextBlock)
                .Text(FText::FromString(CompositeText))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Composite))
        ];

    // Plan-step badge overlay. Visibility and number both come from
    // _ActionNode flags mutated by UCkGoapDebugGraph::UpdateRuntimeState, so
    // they MUST be bound via TAttribute — capturing the int at construction
    // produces stale visuals (or, worse, requires NotifyGraphChanged to
    // rebuild the whole node widget, which is the flicker we're fixing).
    auto StepBadge = SNew(SBox)
        .WidthOverride(18.0f)
        .HeightOverride(18.0f)
        .Visibility(TAttribute<EVisibility>::CreateLambda([WeakNode]()
        {
            const auto* Node = WeakNode.Get();
            if (Node == nullptr) { return EVisibility::Collapsed; }
            const auto Show = Node->Get_IsInPlan() && Node->Get_PlanStepIndex() > 0;
            return Show ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
        }))
        [
            SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Status_PlanningBdr)
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(TAttribute<FText>::CreateLambda([WeakNode]()
                        {
                            const auto* Node = WeakNode.Get();
                            if (Node == nullptr) { return FText::GetEmpty(); }
                            return FText::FromString(FString::FromInt(Node->Get_PlanStepIndex()));
                        }))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        .ColorAndOpacity(FSlateColor(FLinearColor::White))
                ]
        ];

    const auto BodyContent = BuildBody();

    // Card chrome: outer border in the highlight color (live-bound — driven
    // by IsSelected / IsFailureBlocked / IsInPlan / IsComposite flags that
    // UpdateRuntimeState mutates per tick), inner panel in surface color.
    auto Card = SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([WeakNode]()
        {
            const auto* Node = WeakNode.Get();
            return FSlateColor(Compute_BorderColor(Node));
        }))
        .Padding(FMargin(2.0f))  // border thickness
        [
            SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Surface)
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                            .AutoHeight()
                            [ Header ]

                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f)
                            [ CompositeBar ]

                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f)
                            [ BodyContent ]
                ]
        ];

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
                .WidthOverride(FCkGoapDebuggerStyle::GraphNode_Width)
                .MinDesiredHeight(FCkGoapDebuggerStyle::GraphNode_MinHeight)
                [
                    SNew(SOverlay)

                        // Pin overlay — fills entire node so connection geometry attaches to the boundary.
                        + SOverlay::Slot()
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Fill)
                            [
                                SAssignNew(RightNodeBox, SVerticalBox)
                                    .Visibility(EVisibility::HitTestInvisible)
                            ]

                        // Card body.
                        + SOverlay::Slot()
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Fill)
                            [
                                Card
                            ]

                        // Plan-step badge in the top-left corner.
                        + SOverlay::Slot()
                            .HAlign(HAlign_Left)
                            .VAlign(VAlign_Top)
                            .Padding(FMargin(-6.0f, -6.0f, 0.0f, 0.0f))
                            [
                                StepBadge
                            ]
                ]
        ];

    CreatePinWidgets();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// ====================================================================================================================

auto
    SGraphNode_GoapAction::
    BuildBody()
    -> TSharedRef<SWidget>
{
    const auto& Snap = _ActionNode->Get_Snapshot();

    // Build per-key WS lookup so the dot color reflects current satisfaction state.
    auto LeftCol  = SNew(SVerticalBox);
    auto RightCol = SNew(SVerticalBox);

    // Preconditions: green if any condition's authored Value matches an
    // arbitrary "sat" check is non-trivial; for D5 use red/green based on
    // whether the condition's Value field is true (treating the snapshot
    // truthiness as a quick visual signal). A future revision should pull
    // from the parent Planner's WorldState array.
    for (const auto& Pre : Snap.Preconditions)
    {
        const auto Sat = Pre.Value;
        const auto DotColor = Sat
            ? FCkGoapDebuggerStyle::Color_Status_PlanFound
            : FCkGoapDebuggerStyle::Color_Status_Failed;

        LeftCol->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 1.0f)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                        [ MakeDot(DotColor) ]
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Compute_TagLeaf(Pre.Key)))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
            ];
    }

    for (const auto& Eff : Snap.Effects)
    {
        RightCol->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 1.0f)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Compute_TagLeaf(Eff.Key)))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                        [ MakeDot(FCkGoapDebuggerStyle::Color_Status_Planning) ]
            ];
    }

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [ LeftCol ]

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [ RightCol ];
}

// ====================================================================================================================

auto
    SGraphNode_GoapAction::
    CreatePinWidgets()
    -> void
{
    if (NOT GraphNode) { return; }

    // Only emit a single output-pin widget — the connection policy resolves
    // wire geometry to the node boundary, so the visual pin is purely for
    // anchoring. HitTestInvisible to keep right-click on the card.
    for (auto* Pin : GraphNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && NOT Pin->bHidden)
        {
            auto PinWidget = SNew(SGraphPin, Pin);
            PinWidget->SetIsEditable(false);
            PinWidget->SetVisibility(EVisibility::HitTestInvisible);
            AddPin(PinWidget);
            break;
        }
    }
}

// ====================================================================================================================

auto
    SGraphNode_GoapAction::
    AddPin(
        const TSharedRef<SGraphPin>& InPinToAdd)
    -> void
{
    InPinToAdd->SetOwner(SharedThis(this));

    RightNodeBox->AddSlot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .FillHeight(1.0f)
        [
            InPinToAdd
        ];

    OutputPins.Add(InPinToAdd);
}

// ====================================================================================================================
