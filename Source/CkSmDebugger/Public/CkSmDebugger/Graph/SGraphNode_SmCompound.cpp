#include "SGraphNode_SmCompound.h"

#include "CkSmDebugNode_Compound.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugGraph.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
    // Padding around the child AABB when computing the compound's own bounds.
    // Matches the visual margin expected between the border and the outermost states.
    constexpr auto Compound_ChildPaddingX = 40.0f;
    constexpr auto Compound_ChildPaddingY = 48.0f;   // extra room on top for the label
    constexpr auto Compound_MinWidth      = 160.0f;
    constexpr auto Compound_MinHeight     = 120.0f;

    // Default state node footprint used when the UEdGraphNode doesn't provide one.
    constexpr auto State_AssumedWidth  = 180.0f;
    constexpr auto State_AssumedHeight = 80.0f;
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

    if (ck::IsValid(InNode))
    {
        _LastKnownPosition = FVector2D(InNode->NodePosX, InNode->NodePosY);
        _HasLastKnownPosition = true;
    }

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
    const auto NewPos2D = FVector2D(InNewPosition);

    // Translate every grouped child node by the same delta so the "box" grabs its contents.
    auto Delta = FVector2D::ZeroVector;
    if (_HasLastKnownPosition)
    { Delta = NewPos2D - _LastKnownPosition; }
    else if (ck::IsValid(_CompoundNode))
    { Delta = NewPos2D - FVector2D(_CompoundNode->NodePosX, _CompoundNode->NodePosY); }

    SGraphNode::MoveTo(InNewPosition, InNodeFilter, bMarkDirty);
    _LastKnownPosition = NewPos2D;
    _HasLastKnownPosition = true;

    if (Delta.IsNearlyZero())
    { return; }

    if (ck::Is_NOT_Valid(_CompoundNode))
    { return; }

    auto* Graph = Cast<UCkSmDebugGraph>(_CompoundNode->GetGraph());
    if (ck::Is_NOT_Valid(Graph))
    { return; }

    // Mark ourselves in the filter so nested MoveTo calls from child nodes don't re-move us.
    InNodeFilter.Add(SharedThis(this));

    for (const auto ChildStateIdx : _CompoundNode->Get_SubSmStateIndices())
    {
        auto* ChildNode = Graph->FindStateNode(ChildStateIdx);
        if (ck::Is_NOT_Valid(ChildNode))
        { continue; }

        ChildNode->Modify();
        ChildNode->NodePosX = static_cast<int32>(ChildNode->NodePosX + Delta.X);
        ChildNode->NodePosY = static_cast<int32>(ChildNode->NodePosY + Delta.Y);
    }
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

    for (const auto ChildStateIdx : ChildIndices)
    {
        const auto* ChildNode = Graph->FindStateNode(ChildStateIdx);
        if (ck::Is_NOT_Valid(ChildNode))
        { continue; }

        const auto ChildX = static_cast<float>(ChildNode->NodePosX);
        const auto ChildY = static_cast<float>(ChildNode->NodePosY);

        MinX = FMath::Min(MinX, ChildX);
        MinY = FMath::Min(MinY, ChildY);
        MaxX = FMath::Max(MaxX, ChildX + State_AssumedWidth);
        MaxY = FMath::Max(MaxY, ChildY + State_AssumedHeight);
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

    _LastKnownPosition = FVector2D(NewX, NewY);
    _HasLastKnownPosition = true;

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
    auto DisplayLabel = FCkSmLayoutParams::ComputeDisplayName(Label, NameDepth);

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
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                .ColorAndOpacity_Lambda([CompoundNodePtr = _CompoundNode]()
                                {
                                    auto Color = FCkSmDebuggerStyle::Color_Sm_SubSmLabel;
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

    constexpr auto CornerRadius = 8.0f;
    constexpr auto BorderThickness = 1.0f;

    // The whole bubble fades by _BubbleGlowAlpha (see Tick): grey → blue border
    // AND dim → brighter intensity, both directions. Driving the intensity off
    // the same fade alpha — rather than snapping it on parent-active — keeps the
    // fade-OUT smooth; otherwise the bubble snapped straight back to faint on
    // exit even while the colour was still mid-fade.
    constexpr auto ActiveBorderAlpha = 0.4f;
    constexpr auto InactiveBorderAlpha = 0.15f;
    constexpr auto ActiveFillAlpha = 0.02f;
    constexpr auto InactiveFillAlpha = 0.01f;

    auto BorderAlpha = FMath::Lerp(InactiveBorderAlpha, ActiveBorderAlpha, _BubbleGlowAlpha);
    auto FillAlpha = FMath::Lerp(InactiveFillAlpha, ActiveFillAlpha, _BubbleGlowAlpha);

    auto BorderColor = FMath::Lerp(
        FCkSmDebuggerStyle::Color_Sm_SubSmInactiveBorder,
        FCkSmDebuggerStyle::Color_Sm_SubSmCurrentBorder,
        _BubbleGlowAlpha);
    BorderColor.A = BorderAlpha;

    // --- Border ---
    {
        static auto BorderBrush = FSlateRoundedBoxBrush(FLinearColor::White, CornerRadius);

        FSlateDrawElement::MakeBox(
            OutDrawElements, ContainerLayer,
            AllottedGeometry.ToPaintGeometry(),
            &BorderBrush, ESlateDrawEffect::None, BorderColor);
    }

    // --- Near-transparent fill ---
    {
        static auto FillBrush = FSlateRoundedBoxBrush(FLinearColor::White, CornerRadius - 1.0f);
        auto FillGeom = AllottedGeometry.MakeChild(
            FVector2f(W - BorderThickness * 2, H - BorderThickness * 2),
            FSlateLayoutTransform(FVector2f(BorderThickness, BorderThickness)));

        auto FillColor = FCkSmDebuggerStyle::Color_Sm_SubSmNodeBackground;
        FillColor.A = FillAlpha;

        FSlateDrawElement::MakeBox(
            OutDrawElements, ContainerLayer + 1,
            FillGeom.ToPaintGeometry(),
            &FillBrush, ESlateDrawEffect::None, FillColor);
    }

    return SGraphNode::OnPaint(
        Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

// --------------------------------------------------------------------------------------------------------------------
