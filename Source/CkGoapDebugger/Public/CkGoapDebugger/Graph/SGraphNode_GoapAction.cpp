#include "SGraphNode_GoapAction.h"

#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DecisionModel.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

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
    // in-plan > composite > atomic leaf. Composite is a TIER distinction, not a severity, so it
    // reads from the categorical purple role rather than a tone. Falls back to the inactive
    // node-border role only if the node pointer is null (invariant violation — every real
    // action node carries the leaf path).
    auto Compute_BorderColor(const UCkGoapDebugNode_Action* InNode) -> FLinearColor
    {
        if (NOT InNode) { return CkStyle::NodeBorder_Inactive(); }

        if (InNode->Get_IsSelected())
        { return CkStyle::Warn(); }

        if (InNode->Get_IsFailureBlocked())
        { return CkStyle::Err(); }

        if (InNode->Get_IsInPlan())
        { return CkStyle::NodeBorder_InPlan(); }

        if (InNode->Get_IsComposite())
        { return CkStyle::CategoryAge(); }

        // Atomic leaf — explicit tint matching the legend.
        return CkStyle::Accent();
    }

    // Small filled circle for port indicator dots. Accepts a TAttribute so
    // satisfaction dots can live-bind to the graph's world-state view.
    auto MakeDot(const TAttribute<FSlateColor>& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(ck_goap_debugger_axes::Get_DotSize())
            .HeightOverride(ck_goap_debugger_axes::Get_DotSize())
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(InColor)
            ];
    }

    // Current value of a WS key as seen by the node's owning graph; unset when
    // the key isn't in the Planner's resolved WS (or the node/graph is gone).
    auto Query_LiveWorldState(
        const TWeakObjectPtr<UCkGoapDebugNode_Action>& InWeakNode,
        const FGameplayTag& InKey) -> TOptional<bool>
    {
        const auto* Node = InWeakNode.Get();
        if (Node == nullptr) { return {}; }

        const auto* Graph = Cast<UCkGoapDebugGraph>(Node->GetGraph());
        if (Graph == nullptr) { return {}; }

        return Graph->Get_LiveWorldStateValue(InKey);
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
                .BorderBackgroundColor(CkStyle::NodeFill_Inactive())
        ];
        return;
    }

    const auto& Snap = _ActionNode->Get_Snapshot();

    // Live-binding capture: TWeakObjectPtr is robust to PIE teardown — Slate
    // outlives the UObject during world transitions. All TAttribute lambdas
    // below null-check via .Get() before deref.
    const auto WeakNode = TWeakObjectPtr<UCkGoapDebugNode_Action>(_ActionNode);

    // Header row: name + cost. ClassName is a topology-stable snapshot field
    // (only changes on rebuild → new widget), safe to capture at construction.
    // Cost, however, changes at runtime via Request_SetChildActionCost WITHOUT
    // a topology change, so it MUST be live-bound via TAttribute (like the step
    // badge below) — UpdateRuntimeState refreshes the node snapshot's cost each
    // tick. The display-depth is read off the owning graph so toggling the
    // toolbar's name-depth control re-renders every action card (the topology
    // hash includes NameDepth, so a depth change forces a rebuild).
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
                    .Text(FText::FromString(SCkDebug_NameLabel::Get_ShortName(Snap.ClassName, OwningDepth)))
                    .Font(CkStyle::BoldFont(CkStyle::NodeTitleFontSize()))
                    .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(TAttribute<FText>::CreateLambda([WeakNode]()
                    {
                        const auto* Node = WeakNode.Get();
                        if (Node == nullptr) { return FText::GetEmpty(); }
                        return FText::FromString(FString::Printf(TEXT("$%.0f"), Node->Get_Snapshot().Cost));
                    }))
                    .Font(CkStyle::BoldFont(CkStyle::NodeCostFontSize()))
                    .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
            ];

    // Role line — the mockup's "◆● ACTION+PLANNER / ● FALLBACK / ● ACTION"
    // strip. Composite state and cost both mutate at runtime, so the whole
    // line is live-bound (fallback = cost at/above the always-valid-plan
    // convention floor).
    auto RoleLine = SNew(STextBlock)
        .Text(TAttribute<FText>::CreateLambda([WeakNode]()
        {
            const auto* Node = WeakNode.Get();
            if (Node == nullptr) { return FText::GetEmpty(); }
            if (Node->Get_IsComposite())
            { return FText::FromString(TEXT("\x25C6\x25CF ACTION+PLANNER")); }
            if (Node->Get_Snapshot().Cost >= ck_goap_debugger_decision_model::k_FallbackCostFloor)
            { return FText::FromString(TEXT("\x25CF FALLBACK")); }
            return FText::FromString(TEXT("\x25CF ACTION"));
        }))
        .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
        .ColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([WeakNode]() -> FSlateColor
        {
            const auto* Node = WeakNode.Get();
            if (Node != nullptr &&
                Node->Get_Snapshot().Cost >= ck_goap_debugger_decision_model::k_FallbackCostFloor)
            { return FSlateColor(CkStyle::Warn()); }
            return FSlateColor(CkStyle::TextMute());
        }));

    // Composite bar — small purple "▸ leaf" strip just below the header.
    // ActionTag is a snapshot field (set on topology rebuild). Visibility is
    // bound live to Get_IsComposite() so that future per-tick flips don't
    // require widget recreation.
    const auto CompositeText = FString::Printf(TEXT("\x203A %s"), *Compute_TagLeaf(Snap.ActionTag));

    auto CompositeBar = SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::CategoryAge(), 0.15f))
        .Padding(FMargin{CkStyle::SpaceS, 1.0f})
        .Visibility(TAttribute<EVisibility>::CreateLambda([WeakNode]()
        {
            const auto* Node = WeakNode.Get();
            if (Node == nullptr) { return EVisibility::Collapsed; }
            return Node->Get_IsComposite() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
        }))
        [
            SNew(STextBlock)
                .Text(FText::FromString(CompositeText))
                .Font(CkStyle::RegularFont(CkStyle::NodeMetaFontSize()))
                .ColorAndOpacity(FSlateColor(CkStyle::CategoryAge()))
        ];

    // Plan-step badge overlay. Visibility and number both come from
    // _ActionNode flags mutated by UCkGoapDebugGraph::UpdateRuntimeState, so
    // they MUST be bound via TAttribute — capturing the int at construction
    // produces stale visuals (or, worse, requires NotifyGraphChanged to
    // rebuild the whole node widget, which is the flicker we're fixing).
    auto StepBadge = SNew(SBox)
        .WidthOverride(ck_goap_debugger_axes::Get_IconSize())
        .HeightOverride(ck_goap_debugger_axes::Get_IconSize())
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
                .BorderBackgroundColor(CkStyle::NodeBorder_InPlan())
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
                        .Font(CkStyle::BoldFont(CkStyle::NodeCostFontSize()))
                        .ColorAndOpacity(FSlateColor(CkStyle::TextStrong()))
                ]
        ];

    const auto BodyContent = BuildBody();

    // Card chrome: outer border in the highlight color, inner panel in the matching node-fill
    // role. Both are live-bound — IsSelected / IsFailureBlocked / IsInPlan / IsComposite are
    // mutated per tick by UpdateRuntimeState, and a construction-time read would need a node-widget
    // recreation (the flicker this binding exists to avoid) to show through.
    auto Card = SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([WeakNode]()
        {
            const auto* Node = WeakNode.Get();
            return FSlateColor(Compute_BorderColor(Node));
        }))
        .Padding(FMargin(CkStyle::NodeBorderThickness()))
        [
            SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                // Fill follows the in-plan flag, so it live-binds exactly like the border does.
                .BorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([WeakNode]()
                {
                    const auto* Node = WeakNode.Get();
                    if (Node == nullptr) { return FSlateColor(CkStyle::NodeFill_Inactive()); }
                    return FSlateColor(Node->Get_IsInPlan()
                        ? CkStyle::NodeFill_InPlan()
                        : CkStyle::NodeFill_Inactive());
                }))
                .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                            .AutoHeight()
                            [ Header ]

                        + SVerticalBox::Slot()
                            .AutoHeight()
                            [ RoleLine ]

                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
                            [ CompositeBar ]

                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                            [ BodyContent ]
                ]
        ];

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
                // Self-sizing: Slate's own desired-size drives the card width
                // between Min and Max, so long WS key names never clip even if
                // the layout-side font measurement drifts from the real fonts.
                // Measure_NodeSize_Graph still feeds the layered layout so
                // column spacing reserves at least this card's footprint.
                .MinDesiredWidth(FMath::Max(FCkGoapDebuggerStyle::GraphNode_Width, _ActionNode->Get_NodeWidth()))
                .MaxDesiredWidth(FCkGoapDebuggerStyle::GraphNode_MaxWidth)
                .MinDesiredHeight(FCkGoapDebuggerStyle::GraphNode_MinHeight)
                [
                    SNew(SOverlay)

                        // Active-chain glow — the mockup's ".nd.on" accent halo.
                        // 9-slice alpha-falloff brush tinted accent, drawn behind
                        // the card and extended past its bounds; live-bound to
                        // the in-plan flag UpdateRuntimeState maintains.
                        + SOverlay::Slot()
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Fill)
                            .Padding(FMargin(-10.0f))
                            [
                                SNew(SImage)
                                    .Image(FCkDebuggerCommonStyle::Get_GlowTightBrush())
                                    .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                    .Visibility(TAttribute<EVisibility>::CreateLambda([WeakNode]()
                                    {
                                        const auto* Node = WeakNode.Get();
                                        if (Node == nullptr) { return EVisibility::Collapsed; }
                                        return Node->Get_IsInPlan()
                                            ? EVisibility::HitTestInvisible
                                            : EVisibility::Collapsed;
                                    }))
                            ]

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
    const auto WeakNode = TWeakObjectPtr<UCkGoapDebugNode_Action>(_ActionNode);

    auto LeftCol  = SNew(SVerticalBox);
    auto RightCol = SNew(SVerticalBox);

    // Name-sorted display copies — the snapshot keeps authoring order (it
    // feeds the topology hash), the card shows a stable alphabetical list.
    auto SortedPre = Snap.Preconditions;
    SortedPre.Sort([](const FCkGoapDebugger_Condition& A, const FCkGoapDebugger_Condition& B)
    { return Compute_TagLeaf(A.Key) < Compute_TagLeaf(B.Key); });

    auto SortedEff = Snap.Effects;
    SortedEff.Sort([](const FCkGoapDebugger_Condition& A, const FCkGoapDebugger_Condition& B)
    { return Compute_TagLeaf(A.Key) < Compute_TagLeaf(B.Key); });

    // Preconditions: the dot is the LIVE satisfaction state — green when the
    // Planner's resolved WS currently matches the authored desired value, red
    // when it doesn't, dim when the key isn't in the WS snapshot. Bound via
    // TAttribute (per the SGraphNode live-bind invariant) so per-tick WS flips
    // repaint without widget recreation. The authored desired value renders as
    // a dim "= true/false" suffix so both halves of the comparison are visible.
    for (const auto& Pre : SortedPre)
    {
        const auto Key          = Pre.Key;
        const auto DesiredValue = Pre.Value;

        // "Exp -> Cur" squares: the left square is the authored desired VALUE
        // (green = true, red = false; static), the right square is the live
        // current VALUE from the Planner's resolved WS (green/red, dim when
        // the key isn't in the WS). Satisfied reads as "both squares match".
        const auto ExpColor = DesiredValue
            ? CkStyle::Ok()
            : CkStyle::Err();

        auto CurColorAttr = TAttribute<FSlateColor>::CreateLambda([WeakNode, Key]()
        {
            const auto Current = Query_LiveWorldState(WeakNode, Key);
            if (NOT Current.IsSet())
            { return FSlateColor(CkStyle::TextMute()); }
            return FSlateColor(*Current
                ? CkStyle::Ok()
                : CkStyle::Err());
        });

        auto TooltipAttr = TAttribute<FText>::CreateLambda([WeakNode, Key, DesiredValue]()
        {
            const auto Current = Query_LiveWorldState(WeakNode, Key);
            const auto CurrentStr = Current.IsSet()
                ? FString(*Current ? TEXT("TRUE") : TEXT("false"))
                : FString(TEXT("(not in WorldState)"));
            const auto SatStr = Current.IsSet()
                ? FString(*Current == DesiredValue ? TEXT("satisfied") : TEXT("NOT satisfied"))
                : FString(TEXT("unknown"));
            return FText::FromString(FString::Printf(
                TEXT("%s\nWants: %s\nCurrent: %s — %s"),
                *Key.ToString(),
                DesiredValue ? TEXT("true") : TEXT("false"),
                *CurrentStr,
                *SatStr));
        });

        LeftCol->AddSlot()
            .AutoHeight()
            .Padding(ck_goap_debugger_axes::Apply_RowDensity(FMargin{0.0f, 1.0f}))
            [
                SNew(SHorizontalBox)
                    .ToolTipText(TooltipAttr)
                    // Expected-value square (authored)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [ MakeDot(FSlateColor(ExpColor)) ]

                    // "->"
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(2.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("\x2192")))
                                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                        ]

                    // Current-value square (live WS)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                        [ MakeDot(CurColorAttr) ]

                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Compute_TagLeaf(Pre.Key)))
                                .Font(CkStyle::RegularFont(CkStyle::NodeMetaFontSize()))
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
            ];
    }

    for (const auto& Eff : SortedEff)
    {
        RightCol->AddSlot()
            .AutoHeight()
            .Padding(ck_goap_debugger_axes::Apply_RowDensity(FMargin{0.0f, 1.0f}))
            [
                SNew(SHorizontalBox)
                    .ToolTipText(FText::FromString(Eff.Key.ToString()))
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Compute_TagLeaf(Eff.Key)))
                                .Font(CkStyle::RegularFont(CkStyle::NodeMetaFontSize()))
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                        [ MakeDot(FSlateColor(CkStyle::Accent())) ]
            ];
    }

    // Columns take their NATURAL width with a stretchy gap between — a 50/50
    // Fill split clips the wider column at half the card even when the card
    // is wide enough for both (the cause of mid-chain cards ellipsizing
    // their precondition names).
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
            .AutoWidth()
            [ LeftCol ]

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SSpacer)
                    .Size(FVector2D(12.0f, 1.0f))
            ]

        + SHorizontalBox::Slot()
            .AutoWidth()
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
