#include "SGraphNode_SmCompound.h"

#include "CkSmDebugNode_Compound.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugGraph.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "SGraphPanel.h"
#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
    // The sub-SM bubble's corner radius, and the ONE brush per radius variant that draws it.
    // GraphNodeStyle can change the radius, and a brush cannot be rebuilt per paint (R7) — so each
    // reachable radius gets its own function-local static, allocated once on first use, and the
    // paint path picks between them. Two variants cover the axis: Card and Minimal share the
    // surface's own radius, Dense halves it.
    constexpr auto Compound_CornerRadius = 8.0f;

    auto Get_CompoundBorderBrush() -> const FSlateBrush*
    {
        static auto Standard = FSlateRoundedBoxBrush(FLinearColor::White, Compound_CornerRadius);
        static auto Tight    = FSlateRoundedBoxBrush(FLinearColor::White, Compound_CornerRadius * 0.5f);

        return ck_sm_debugger_axes::Get_NodeRadiusScale() < 1.0f ? &Tight : &Standard;
    }

    auto Get_CompoundFillBrush() -> const FSlateBrush*
    {
        static auto Standard = FSlateRoundedBoxBrush(FLinearColor::White, Compound_CornerRadius - 1.0f);
        static auto Tight    = FSlateRoundedBoxBrush(FLinearColor::White, (Compound_CornerRadius - 1.0f) * 0.5f);

        return ck_sm_debugger_axes::Get_NodeRadiusScale() < 1.0f ? &Tight : &Standard;
    }

    // Padding around the child AABB when computing the compound's own bounds.
    // Matches the visual margin expected between the border and the outermost states.
    constexpr auto Compound_ChildPaddingX = 40.0f;
    constexpr auto Compound_ChildPaddingY = 48.0f;   // extra room on top for the label
    constexpr auto Compound_MinWidth      = 160.0f;
    constexpr auto Compound_MinHeight     = 120.0f;

    // Default state node footprint used when the live Slate widget isn't available
    // to provide a real desired size.
    constexpr auto State_AssumedWidth  = 180.0f;
    constexpr auto State_AssumedHeight = 80.0f;

    // Translate the compound's grouped children by a delta, recursing into nested
    // compounds (a child state that owns its own sub-SM block) so the whole
    // subtree moves as one. Visited-set guards against malformed parent cycles.
    auto TranslateCompoundChildren(
        UCkSmDebugGraph& InGraph,
        const UCkSmDebugNode_Compound& InCompound,
        const FVector2D& InDelta,
        TSet<const UCkSmDebugNode_Compound*>& InOutVisited)
        -> void
    {
        if (InOutVisited.Contains(&InCompound))
        { return; }
        InOutVisited.Add(&InCompound);

        for (const auto ChildStateIdx : InCompound.Get_SubSmStateIndices())
        {
            auto* ChildNode = InGraph.FindStateNode(ChildStateIdx);
            if (ck::IsValid(ChildNode))
            {
                ChildNode->Modify();
                ChildNode->NodePosX = static_cast<int32>(ChildNode->NodePosX + InDelta.X);
                ChildNode->NodePosY = static_cast<int32>(ChildNode->NodePosY + InDelta.Y);
            }

            if (auto* NestedCompound = InGraph.FindCompoundNode(ChildStateIdx); ck::IsValid(NestedCompound))
            {
                NestedCompound->Modify();
                NestedCompound->NodePosX = static_cast<int32>(NestedCompound->NodePosX + InDelta.X);
                NestedCompound->NodePosY = static_cast<int32>(NestedCompound->NodePosY + InDelta.Y);
                TranslateCompoundChildren(InGraph, *NestedCompound, InDelta, InOutVisited);
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    Construct(
        const FArguments& InArgs,
        UCkSmDebugNode_Compound* InNode)
    -> void
{
    _CompoundNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    MoveTo(
        const FVector2f& InNewPosition,
        FNodeSet& InNodeFilter,
        bool bMarkDirty)
    -> void
{
    if (ck::Is_NOT_Valid(_CompoundNode))
    {
        SGraphNode::MoveTo(InNewPosition, InNodeFilter, bMarkDirty);
        return;
    }

    // Translate every grouped child node by the same delta so the "box" grabs its
    // contents. Delta is measured across the base call from the node's authoritative
    // position — the base early-outs when we're already in the node filter, and a
    // cached last-known position goes stale when an ENCLOSING compound translates
    // this node directly (nested sub-SMs); both would teleport the children.
    const auto PreMovePos = FVector2D(_CompoundNode->NodePosX, _CompoundNode->NodePosY);

    SGraphNode::MoveTo(InNewPosition, InNodeFilter, bMarkDirty);

    const auto Delta = FVector2D(_CompoundNode->NodePosX, _CompoundNode->NodePosY) - PreMovePos;
    if (Delta.IsNearlyZero())
    { return; }

    auto* Graph = Cast<UCkSmDebugGraph>(_CompoundNode->GetGraph());
    if (ck::Is_NOT_Valid(Graph))
    { return; }

    auto Visited = TSet<const UCkSmDebugNode_Compound*>{};
    TranslateCompoundChildren(*Graph, *_CompoundNode, Delta, Visited);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    RecomputeBoundsFromChildren()
    -> bool
{
    if (ck::Is_NOT_Valid(_CompoundNode))
    { return false; }

    auto* Graph = Cast<UCkSmDebugGraph>(_CompoundNode->GetGraph());
    if (ck::Is_NOT_Valid(Graph))
    { return false; }

    const auto& ChildIndices = _CompoundNode->Get_SubSmStateIndices();
    if (ChildIndices.Num() == 0)
    { return false; }

    auto HasAnyChild = false;
    auto MinX = TNumericLimits<float>::Max();
    auto MinY = TNumericLimits<float>::Max();
    auto MaxX = TNumericLimits<float>::Lowest();
    auto MaxY = TNumericLimits<float>::Lowest();

    const auto OwnerPanel = GetOwnerPanel();

    for (const auto ChildStateIdx : ChildIndices)
    {
        const auto* ChildNode = Graph->FindStateNode(ChildStateIdx);
        if (ck::Is_NOT_Valid(ChildNode))
        { continue; }

        const auto ChildX = static_cast<float>(ChildNode->NodePosX);
        const auto ChildY = static_cast<float>(ChildNode->NodePosY);

        // Prefer the live widget's real footprint — the fixed assumption under-
        // sizes states carrying task rows, leaving them spilling past the border.
        auto ChildW = State_AssumedWidth;
        auto ChildH = State_AssumedHeight;
        if (OwnerPanel.IsValid())
        {
            if (const auto ChildWidget = OwnerPanel->GetNodeWidgetFromGuid(ChildNode->NodeGuid);
                ChildWidget.IsValid())
            {
                const auto Desired = ChildWidget->GetDesiredSize();
                if (Desired.X > 1.0f && Desired.Y > 1.0f)
                {
                    ChildW = static_cast<float>(Desired.X);
                    ChildH = static_cast<float>(Desired.Y);
                }
            }
        }

        MinX = FMath::Min(MinX, ChildX);
        MinY = FMath::Min(MinY, ChildY);
        MaxX = FMath::Max(MaxX, ChildX + ChildW);
        MaxY = FMath::Max(MaxY, ChildY + ChildH);
        HasAnyChild = true;
    }

    if (NOT HasAnyChild)
    { return false; }

    // Anchor the compound to its current top-left and grow the box to contain all children
    // (plus padding). Never shrinks past the original box — so children laid out tightly
    // don't cause the box to snap smaller than the initial visual.
    const auto CurrentX = static_cast<float>(_CompoundNode->NodePosX);
    const auto CurrentY = static_cast<float>(_CompoundNode->NodePosY);
    const auto CurrentW = _CompoundNode->Get_CompoundWidth();
    const auto CurrentH = _CompoundNode->Get_CompoundHeight();

    // Required extent relative to the compound's current origin.
    const auto RequiredRightEdge  = MaxX + Compound_ChildPaddingX;
    const auto RequiredBottomEdge = MaxY + Compound_ChildPaddingX;
    const auto RequiredLeftEdge   = MinX - Compound_ChildPaddingX;
    const auto RequiredTopEdge    = MinY - Compound_ChildPaddingY;

    const auto NewX = FMath::Min(CurrentX, RequiredLeftEdge);
    const auto NewY = FMath::Min(CurrentY, RequiredTopEdge);
    const auto NewRight  = FMath::Max(CurrentX + CurrentW, RequiredRightEdge);
    const auto NewBottom = FMath::Max(CurrentY + CurrentH, RequiredBottomEdge);
    const auto NewW = FMath::Max(Compound_MinWidth,  NewRight  - NewX);
    const auto NewH = FMath::Max(Compound_MinHeight, NewBottom - NewY);

    if (FMath::IsNearlyEqual(NewX, CurrentX) && FMath::IsNearlyEqual(NewY, CurrentY) &&
        FMath::IsNearlyEqual(NewW, CurrentW) && FMath::IsNearlyEqual(NewH, CurrentH))
    { return false; }

    _CompoundNode->Populate(
        _CompoundNode->Get_ParentStateIndex(),
        _CompoundNode->Get_CompoundLabel(),
        NewW, NewH,
        ChildIndices);

    _CompoundNode->NodePosX = static_cast<int32>(NewX);
    _CompoundNode->NodePosY = static_cast<int32>(NewY);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime)
    -> void
{
    SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    // Fade the bubble border between grey (inactive) and blue (holds the active
    // sub-state), mirroring the state-node border fade. Driven here (widget Tick,
    // mode-independent) rather than the graph so it stays smooth in any mode.
    {
        const auto Target = (ck::IsValid(_CompoundNode) && _CompoundNode->Get_HasActiveSubState()) ? 1.0f : 0.0f;
        const auto Step = FCkSmDebuggerStyle::Sm_HighlightFadeDuration > 0.0f
            ? InDeltaTime / FCkSmDebuggerStyle::Sm_HighlightFadeDuration : 1.0f;
        if (_BubbleGlowAlpha < Target)
        { _BubbleGlowAlpha = FMath::Min(Target, _BubbleGlowAlpha + Step); }
        else if (_BubbleGlowAlpha > Target)
        { _BubbleGlowAlpha = FMath::Max(Target, _BubbleGlowAlpha - Step); }
    }

    if (RecomputeBoundsFromChildren())
    { UpdateGraphNode(); }
}

// --------------------------------------------------------------------------------------------------------------------

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_SmCompound::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    auto CompoundWidth = _CompoundNode ? _CompoundNode->Get_CompoundWidth() : 200.0f;
    auto CompoundHeight = _CompoundNode ? _CompoundNode->Get_CompoundHeight() : 150.0f;

    auto Label = _CompoundNode ? _CompoundNode->Get_CompoundLabel() : TEXT("Sub-SM");
    auto NameDepth = 1;
    if (_CompoundNode)
    {
        if (auto* Graph = Cast<UCkSmDebugGraph>(_CompoundNode->GetGraph()))
        { NameDepth = Graph->LayoutParams.NameDepth; }
    }
    auto DisplayLabel = SCkDebug_NameLabel::Get_ShortName(Label, NameDepth);

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [
            SNew(SBox)
                .WidthOverride(CompoundWidth)
                .HeightOverride(CompoundHeight)
                .Visibility(EVisibility::HitTestInvisible)
                [
                    SNew(SOverlay)
                        .Visibility(EVisibility::HitTestInvisible)

                    // Hidden pin area for connection geometry
                    + SOverlay::Slot()
                        .HAlign(HAlign_Fill)
                        .VAlign(VAlign_Fill)
                        [
                            SAssignNew(RightNodeBox, SVerticalBox)
                        ]

                    // Label at top-left
                    + SOverlay::Slot()
                        .HAlign(HAlign_Left)
                        .VAlign(VAlign_Top)
                        .Padding(8.0f, 4.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(DisplayLabel))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                                .ColorAndOpacity_Lambda([CompoundNodePtr = _CompoundNode]()
                                {
                                    auto Color = CkStyle::OverlayOf(CkStyle::Accent(), 0.753f);
                                    if (CompoundNodePtr && NOT CompoundNodePtr->Get_IsParentStateActive())
                                    { Color.A *= 0.35f; }
                                    return FSlateColor(Color);
                                })
                        ]
                ]
        ];

    CreatePinWidgets();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    CreatePinWidgets()
    -> void
{
    if (NOT GraphNode) { return; }

    for (auto* Pin : GraphNode->Pins)
    {
        if (Pin && NOT Pin->bHidden)
        {
            auto PinWidget = SNew(SGraphPin, Pin);
            PinWidget->SetIsEditable(false);
            PinWidget->SetVisibility(EVisibility::Collapsed);
            AddPin(PinWidget);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    AddPin(
        const TSharedRef<SGraphPin>& PinToAdd)
    -> void
{
    PinToAdd->SetOwner(SharedThis(this));

    RightNodeBox->AddSlot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .FillHeight(1.0f)
        [
            PinToAdd
        ];

    InputPins.Add(PinToAdd);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const
    -> int32
{
    auto ContainerLayer = LayerId - 2;

    auto Size = AllottedGeometry.GetLocalSize();
    auto W = static_cast<float>(Size.X);
    auto H = static_cast<float>(Size.Y);

    // GraphNodeStyle, read live — OnPaint runs every frame, so no binding is needed or possible.
    // Both are SCALES on the bubble's own tuned geometry: Card divides the role by itself, so
    // Classic paints exactly the 1px border and 0.22/0.1 alphas it shipped with.
    const auto BorderThickness = 1.0f * ck_sm_debugger_axes::Get_NodeBorderScale();
    const auto DimScale        = ck_sm_debugger_axes::Get_NodeDimScale();

    // The bubble's intensity fades by _BubbleGlowAlpha (see Tick): it brightens a
    // little (via alpha) when it holds the active sub-state and dims back when it
    // doesn't, both directions smoothly. Colour stays a constant neutral grey-blue
    // — only the active STATE inside carries the green.
    constexpr auto ActiveBorderAlpha = 0.22f;
    constexpr auto InactiveBorderAlpha = 0.1f;
    constexpr auto ActiveFillAlpha = 0.02f;
    constexpr auto InactiveFillAlpha = 0.01f;

    auto BorderAlpha = FMath::Lerp(InactiveBorderAlpha, ActiveBorderAlpha, _BubbleGlowAlpha) * DimScale;
    // Minimal is border-only: the bubble keeps its outline and drops the wash entirely.
    auto FillAlpha = ck_sm_debugger_axes::Get_NodeDrawsFill()
        ? FMath::Lerp(InactiveFillAlpha, ActiveFillAlpha, _BubbleGlowAlpha) * DimScale
        : 0.0f;

    // Faint neutral container: the active STATE inside (its green edge bar) is the
    // eye-catcher, so the bubble stays a subtle grey-blue box that only brightens
    // a little (via alpha) when it holds the active sub-state. No green fill — it
    // never competes with the active state.
    auto BorderColor = CkStyle::TextDim();
    BorderColor.A = BorderAlpha;

    // --- Border ---
    {
        FSlateDrawElement::MakeBox(
            OutDrawElements, ContainerLayer,
            AllottedGeometry.ToPaintGeometry(),
            Get_CompoundBorderBrush(), ESlateDrawEffect::None, BorderColor);
    }

    // --- Near-transparent fill ---
    {
        auto FillGeom = AllottedGeometry.MakeChild(
            FVector2f(W - BorderThickness * 2, H - BorderThickness * 2),
            FSlateLayoutTransform(FVector2f(BorderThickness, BorderThickness)));

        auto FillColor = CkStyle::Bg3();
        FillColor.A = FillAlpha;

        FSlateDrawElement::MakeBox(
            OutDrawElements, ContainerLayer + 1,
            FillGeom.ToPaintGeometry(),
            Get_CompoundFillBrush(), ESlateDrawEffect::None, FillColor);
    }

    return SGraphNode::OnPaint(
        Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

// --------------------------------------------------------------------------------------------------------------------
