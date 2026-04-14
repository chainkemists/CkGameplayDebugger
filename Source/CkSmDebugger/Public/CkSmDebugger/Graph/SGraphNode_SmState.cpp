#include "SGraphNode_SmState.h"

#include "CkSmDebugNode_State.h"
#include "CkSmDebugGraph.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Styling/AppStyle.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    Construct(
        const FArguments& InArgs,
        UCkSmDebugNode_State* InNode)
    -> void
{
    _StateNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_SmState::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    auto FullName = _StateNode ? _StateNode->Get_StateName() : TEXT("Unknown");
    auto NameDepth = 1;
    if (_StateNode)
    {
        if (auto* Graph = Cast<UCkSmDebugGraph>(_StateNode->GetGraph()))
        { NameDepth = Graph->LayoutParams.NameDepth; }
    }
    auto StateName = FCkSmLayoutParams::ComputeDisplayName(FullName, NameDepth);
    auto PinPadding = FCkSmDebuggerStyle::Sm_PinPadding;
    auto BpStyle = _StateNode ? _StateNode->Get_BreakpointStyle() : 0;

    auto BpRed  = FCkSmDebuggerStyle::Color_Sm_Breakpoint;
    auto BpHollow = FLinearColor(BpRed.R, BpRed.G, BpRed.B, 0.25f);

    // Style 22/23: breakpoint indicators are INSIDE the widget (replace state-color icon).
    // Hollow when unset, filled when set. Always visible.
    // 22 = squares, 23 = diamonds (45° rotated squares).
    auto bUseInlineBreakpoints = (BpStyle == 22 || BpStyle == 23);
    auto bUseDiamondShape = (BpStyle == 23);

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        [
            SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.Body")))
                .Padding(0.0f)
                .BorderBackgroundColor(this, &SGraphNode_SmState::GetBorderBackgroundColor)
                [
                    SNew(SOverlay)

                    // Pin area — fills entire node for connection geometry
                    + SOverlay::Slot()
                        .HAlign(HAlign_Fill)
                        .VAlign(VAlign_Fill)
                        [
                            SAssignNew(RightNodeBox, SVerticalBox)
                        ]

                    // Content area
                    + SOverlay::Slot()
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBorder)
                                .BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.ColorSpill")))
                                .BorderBackgroundColor(FCkSmDebuggerStyle::Color_Sm_TitleShadow)
                                .Padding(PinPadding)
                                .Visibility(EVisibility::SelfHitTestInvisible)
                                [
                                    SNew(SVerticalBox)

                                    // Header row: icon + name + exit icon
                                    + SVerticalBox::Slot()
                                        .AutoHeight()
                                        [
                                            SNew(SHorizontalBox)

                                            // Left icon: entry breakpoint (Style 22/23) or state color square
                                            + SHorizontalBox::Slot()
                                                .AutoWidth()
                                                .VAlign(VAlign_Center)
                                                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                                                [
                                                    SNew(SBox)
                                                        .WidthOverride(8.0f)
                                                        .HeightOverride(8.0f)
                                                        [
                                                            bUseInlineBreakpoints
                                                            ? StaticCastSharedRef<SWidget>(
                                                                SNew(SBorder)
                                                                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                                                    .BorderBackgroundColor_Lambda([StateNodePtr = _StateNode, BpRed, BpHollow]()
                                                                    {
                                                                        return (StateNodePtr && StateNodePtr->Get_HasEntryBreakpoint())
                                                                            ? BpRed : BpHollow;
                                                                    })
                                                                    .RenderTransformPivot(FVector2D(0.5, 0.5))
                                                                    .RenderTransform(bUseDiamondShape
                                                                        ? FSlateRenderTransform(FQuat2D(PI / 4.0))
                                                                        : FSlateRenderTransform())
                                                              )
                                                            : StaticCastSharedRef<SWidget>(
                                                                SNew(SBorder)
                                                                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                                                    .BorderBackgroundColor(_StateNode ? _StateNode->Get_StateColor() : FLinearColor::White)
                                                              )
                                                        ]
                                                ]

                                            // State name
                                            + SHorizontalBox::Slot()
                                                .AutoWidth()
                                                .VAlign(VAlign_Center)
                                                [
                                                    SNew(STextBlock)
                                                        .Text(FText::FromString(StateName))
                                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                                        .ColorAndOpacity_Lambda([StateNodePtr = _StateNode]()
                                                        {
                                                            auto Color = FCkSmDebuggerStyle::Color_Sm_TextPrimary;
                                                            if (StateNodePtr
                                                                && StateNodePtr->Get_IsSubSmNode()
                                                                && NOT StateNodePtr->Get_IsParentStateActive())
                                                            { Color.A *= 0.35f; }
                                                            return FSlateColor(Color);
                                                        })
                                                ]

                                            // Right icon: exit breakpoint (Style 22/23 only)
                                            + SHorizontalBox::Slot()
                                                .AutoWidth()
                                                .VAlign(VAlign_Center)
                                                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                                                [
                                                    SNew(SBox)
                                                        .WidthOverride(8.0f)
                                                        .HeightOverride(8.0f)
                                                        .Visibility(bUseInlineBreakpoints ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)
                                                        [
                                                            SNew(SBorder)
                                                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                                                .BorderBackgroundColor_Lambda([StateNodePtr = _StateNode, BpRed, BpHollow]()
                                                                {
                                                                    return (StateNodePtr && StateNodePtr->Get_HasExitBreakpoint())
                                                                        ? BpRed : BpHollow;
                                                                })
                                                                .RenderTransformPivot(FVector2D(0.5, 0.5))
                                                                .RenderTransform(bUseDiamondShape
                                                                    ? FSlateRenderTransform(FQuat2D(PI / 4.0))
                                                                    : FSlateRenderTransform())
                                                        ]
                                                ]
                                        ]

                                    // Task rows (shown when node has tasks)
                                    + SVerticalBox::Slot()
                                        .AutoHeight()
                                        [
                                            CreateTaskRows()
                                        ]
                                ]
                        ]
                ]
        ];

    CreatePinWidgets();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    CreatePinWidgets()
    -> void
{
    // Only create a widget for the output pin — it fills the entire node via the
    // overlay. The connection policy resolves wire geometry to the node boundary
    // (not pin position), so the input pin needs no widget.
    if (NOT GraphNode) { return; }

    for (auto* Pin : GraphNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && NOT Pin->bHidden)
        {
            auto PinWidget = SNew(SGraphPin, Pin);
            PinWidget->SetIsEditable(false);
            // HitTestInvisible: pin exists for connection-drawing geometry but
            // does not intercept mouse events — prevents UE's built-in PIN ACTIONS
            // context menu from appearing on right-click.
            PinWidget->SetVisibility(EVisibility::HitTestInvisible);
            AddPin(PinWidget);
            break;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
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

    OutputPins.Add(PinToAdd);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    GetBorderBackgroundColor() const
    -> FSlateColor
{
    if (NOT _StateNode)
    { return FLinearColor::Transparent; }

    auto Color = _StateNode->Get_IsCurrentState()
        ? FCkSmDebuggerStyle::Color_Sm_ActiveStateBody
        : FCkSmDebuggerStyle::Color_Sm_InactiveStateBody;

    // Fade sub-SM states when their parent state is not active
    if (_StateNode->Get_IsSubSmNode() && NOT _StateNode->Get_IsParentStateActive())
    {
        constexpr auto InactiveFade = 0.35f;
        Color.A *= InactiveFade;
    }

    return Color;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    GetNodeInfoPopups(
        FNodeInfoContext* InContext,
        TArray<FGraphInformationPopupInfo>& OutPopups) const
    -> void
{
    if (NOT _StateNode)
    { return; }

    if (_StateNode->Get_IsCurrentState())
    {
        auto DwellStr = FString::Printf(TEXT("Active for %.2f secs"), _StateNode->Get_DwellTimeSeconds());
        OutPopups.Emplace(nullptr, FCkSmDebuggerStyle::Color_Sm_ActiveStateBorder, DwellStr);
    }
}

// --------------------------------------------------------------------------------------------------------------------

// Helper: draw a filled circle at a position
static auto DrawFilledCircle(
    FSlateWindowElementList& OutElements, int32 Layer,
    const FGeometry& NodeGeom, FVector2f Pos, float Size, const FLinearColor& Color) -> void
{
    static auto Brush = FSlateRoundedBoxBrush(FLinearColor::White, 999.0f);
    auto G = NodeGeom.MakeChild(FVector2f(Size, Size), FSlateLayoutTransform(Pos));
    FSlateDrawElement::MakeBox(OutElements, Layer, G.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, Color);
}

// Helper: draw a hollow ring (filled outer, dark inner punch-out)
static auto DrawHollowCircle(
    FSlateWindowElementList& OutElements, int32 Layer,
    const FGeometry& NodeGeom, FVector2f Pos, float Size, float Ring,
    const FLinearColor& Color, const FLinearColor& BgColor) -> void
{
    DrawFilledCircle(OutElements, Layer, NodeGeom, Pos, Size, Color);
    auto Inner = Size - Ring * 2.0f;
    DrawFilledCircle(OutElements, Layer + 1, NodeGeom, FVector2f(Pos.X + Ring, Pos.Y + Ring), Inner, BgColor);
}

// Helper: draw a filled square at a position
static auto DrawFilledSquare(
    FSlateWindowElementList& OutElements, int32 Layer,
    const FGeometry& NodeGeom, FVector2f Pos, float Size, const FLinearColor& Color) -> void
{
    auto G = NodeGeom.MakeChild(FVector2f(Size, Size), FSlateLayoutTransform(Pos));
    FSlateDrawElement::MakeBox(OutElements, Layer, G.ToPaintGeometry(),
        FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Color);
}

// Helper: draw a hollow square
static auto DrawHollowSquare(
    FSlateWindowElementList& OutElements, int32 Layer,
    const FGeometry& NodeGeom, FVector2f Pos, float Size, float Ring,
    const FLinearColor& Color, const FLinearColor& BgColor) -> void
{
    DrawFilledSquare(OutElements, Layer, NodeGeom, Pos, Size, Color);
    auto Inner = Size - Ring * 2.0f;
    DrawFilledSquare(OutElements, Layer + 1, NodeGeom,
        FVector2f(Pos.X + Ring, Pos.Y + Ring), Inner, BgColor);
}

// Helper: draw a diamond (45-degree rotated square)
static auto DrawFilledDiamond(
    FSlateWindowElementList& OutElements, int32 Layer,
    const FGeometry& NodeGeom, FVector2f Center, float Size, const FLinearColor& Color) -> void
{
    auto Half = Size * 0.5f;
    auto Pos = FVector2f(Center.X - Half, Center.Y - Half);
    auto G = NodeGeom.MakeChild(FVector2f(Size, Size), FSlateLayoutTransform(Pos));
    FSlateDrawElement::MakeRotatedBox(
        OutElements, Layer, G.ToPaintGeometry(),
        FAppStyle::GetBrush(TEXT("WhiteBrush")),
        ESlateDrawEffect::None,
        PI / 4.0f,
        TOptional<FVector2D>(),
        FSlateDrawElement::RelativeToElement,
        Color);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
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
    auto Result = SGraphNode::OnPaint(
        Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (NOT _StateNode)
    { return Result; }

    // --- Highlight glow: colored rect drawn behind the node ---
    {
        auto bHighlighted = _StateNode->Get_IsScrubActiveState()
            || _StateNode->Get_IsScrubExitedState()
            || _StateNode->Get_IsPreviousState();

        if (bHighlighted)
        {
            auto HColor = _StateNode->Get_IsScrubActiveState()
                ? FCkSmDebuggerStyle::Color_Sm_ScrubActiveOutline
                : FCkSmDebuggerStyle::Color_Sm_PreviousStateOutline;

            constexpr auto Pad = 4.0f;
            auto GlowColor = FLinearColor(HColor.R, HColor.G, HColor.B, 0.25f);
            static auto GlowBrush = FSlateRoundedBoxBrush(FLinearColor::White, 6.0f);

            auto Size = AllottedGeometry.GetLocalSize();
            auto W = static_cast<float>(Size.X);
            auto H = static_cast<float>(Size.Y);

            auto GlowGeom = AllottedGeometry.MakeChild(
                FVector2f(W + Pad * 2, H + Pad * 2),
                FSlateLayoutTransform(FVector2f(-Pad, -Pad)));

            FSlateDrawElement::MakeBox(
                OutDrawElements, LayerId - 1,
                GlowGeom.ToPaintGeometry(),
                &GlowBrush, ESlateDrawEffect::None, GlowColor);
        }
    }

    auto bEntry = _StateNode->Get_HasEntryBreakpoint();
    auto bExit  = _StateNode->Get_HasExitBreakpoint();

    if (NOT bEntry && NOT bExit)
    { return Result; }

    auto NodeSize = AllottedGeometry.GetLocalSize();
    auto L = Result + 1;  // draw layer

    auto Red  = FCkSmDebuggerStyle::Color_Sm_Breakpoint;
    auto Bg   = FCkSmDebuggerStyle::Color_Sm_InactiveStateBody;
    auto Style = _StateNode->Get_BreakpointStyle();

    auto W = NodeSize.X;
    auto H = NodeSize.Y;
    auto CY = H * 0.5f;
    auto S = 10.0f;   // dot size
    auto O = 12.0f;   // hollow outer
    auto R = 2.0f;    // ring thickness
    auto G = 2.0f;    // gap between dots
    auto P = 4.0f;    // padding from node edge
    auto FadedRed = FLinearColor(Red.R, Red.G, Red.B, 0.4f);

    // =============================================================================================================
    // GROUP A: Circles — overlay corners (half-overlapping the node edge)
    // =============================================================================================================

    if (Style == 0) // A0: top-right, horizontal
    {
        auto X = W - S + P;
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, -S * 0.5f), S, Red); X -= S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, -O * 0.5f), O, R, Red, Bg); }
    }
    else if (Style == 1) // A1: top-left, horizontal
    {
        auto X = -P;
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, -S * 0.5f), S, Red); X += S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, -O * 0.5f), O, R, Red, Bg); }
    }
    else if (Style == 2) // A2: bottom-right, horizontal
    {
        auto X = W - S + P;
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, H - S * 0.5f), S, Red); X -= S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, H - O * 0.5f), O, R, Red, Bg); }
    }
    else if (Style == 3) // A3: bottom-left, horizontal
    {
        auto X = -P;
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, H - S * 0.5f), S, Red); X += S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, H - O * 0.5f), O, R, Red, Bg); }
    }

    // =============================================================================================================
    // GROUP B: Circles — outside edges, horizontally stacked
    // =============================================================================================================

    else if (Style == 4) // B0: right side, horizontal
    {
        auto X = W + P;
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, CY - S * 0.5f), S, Red); X += S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, CY - O * 0.5f), O, R, Red, Bg); }
    }
    else if (Style == 5) // B1: left side, horizontal
    {
        auto X = -(S + P);
        if (bExit)  { X -= O + G; DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X + O + G, CY - O * 0.5f), O, R, Red, Bg); }
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, CY - S * 0.5f), S, Red); }
        // reverse: entry leftmost, exit rightmost
    }
    else if (Style == 6) // B2: top, horizontal center
    {
        auto StartX = (W - (bEntry ? S : 0) - (bExit ? O : 0) - ((bEntry && bExit) ? G : 0)) * 0.5f;
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(StartX, -(S + P)), S, Red); StartX += S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(StartX, -(O + P)), O, R, Red, Bg); }
    }
    else if (Style == 7) // B3: bottom, horizontal center
    {
        auto StartX = (W - (bEntry ? S : 0) - (bExit ? O : 0) - ((bEntry && bExit) ? G : 0)) * 0.5f;
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(StartX, H + P), S, Red); StartX += S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(StartX, H + P), O, R, Red, Bg); }
    }

    // =============================================================================================================
    // GROUP C: Circles — outside edges, vertically stacked
    // =============================================================================================================

    else if (Style == 8) // C0: right side, vertical
    {
        auto X = W + P;
        auto Y = CY - ((bEntry && bExit) ? S + G * 0.5f : S * 0.5f);
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, Y), S, Red); Y += S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, Y), O, R, Red, Bg); }
    }
    else if (Style == 9) // C1: left side, vertical
    {
        auto X = -(FMath::Max(S, O) + P);
        auto Y = CY - ((bEntry && bExit) ? S + G * 0.5f : S * 0.5f);
        if (bEntry) { DrawFilledCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, Y), S, Red); Y += S + G; }
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, Y), O, R, Red, Bg); }
    }

    // =============================================================================================================
    // GROUP D: Bars / strips
    // =============================================================================================================

    else if (Style == 10) // D0: left-side vertical bar (solid=entry, dashed=exit)
    {
        if (bEntry)
        {
            auto Geom = AllottedGeometry.MakeChild(FVector2f(3.0f, H), FSlateLayoutTransform(FVector2f(-6.0f, 0.0f)));
            FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Red);
        }
        if (bExit)
        {
            auto BarX = bEntry ? -10.0f : -6.0f;
            auto SegH = (H - 4.0f) / 3.0f;
            for (auto i = 0; i < 3; ++i)
            {
                auto Geom = AllottedGeometry.MakeChild(FVector2f(3.0f, SegH - 2.0f),
                    FSlateLayoutTransform(FVector2f(BarX, 1.0f + i * SegH)));
                FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                    FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Red);
            }
        }
    }
    else if (Style == 11) // D1: right-side vertical bar
    {
        if (bEntry)
        {
            auto Geom = AllottedGeometry.MakeChild(FVector2f(3.0f, H), FSlateLayoutTransform(FVector2f(W + 3.0f, 0.0f)));
            FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Red);
        }
        if (bExit)
        {
            auto BarX = W + (bEntry ? 7.0f : 3.0f);
            auto SegH = (H - 4.0f) / 3.0f;
            for (auto i = 0; i < 3; ++i)
            {
                auto Geom = AllottedGeometry.MakeChild(FVector2f(3.0f, SegH - 2.0f),
                    FSlateLayoutTransform(FVector2f(BarX, 1.0f + i * SegH)));
                FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                    FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Red);
            }
        }
    }
    else if (Style == 12) // D2: top horizontal strip (entry=left half, exit=right half)
    {
        auto HalfW = W * 0.5f;
        if (bEntry)
        {
            auto Geom = AllottedGeometry.MakeChild(FVector2f(HalfW, 3.0f), FSlateLayoutTransform(FVector2f(0.0f, -5.0f)));
            FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Red);
        }
        if (bExit)
        {
            auto Geom = AllottedGeometry.MakeChild(FVector2f(HalfW, 3.0f), FSlateLayoutTransform(FVector2f(HalfW, -5.0f)));
            FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, FadedRed);
        }
    }
    else if (Style == 13) // D3: bottom horizontal strip
    {
        auto HalfW = W * 0.5f;
        if (bEntry)
        {
            auto Geom = AllottedGeometry.MakeChild(FVector2f(HalfW, 3.0f), FSlateLayoutTransform(FVector2f(0.0f, H + 2.0f)));
            FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Red);
        }
        if (bExit)
        {
            auto Geom = AllottedGeometry.MakeChild(FVector2f(HalfW, 3.0f), FSlateLayoutTransform(FVector2f(HalfW, H + 2.0f)));
            FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, FadedRed);
        }
    }

    // =============================================================================================================
    // GROUP E: Squares / diamonds
    // =============================================================================================================

    else if (Style == 14) // E0: squares, top-right overlay
    {
        auto X = W - S + P;
        if (bEntry) { DrawFilledSquare(OutDrawElements, L, AllottedGeometry, FVector2f(X, -S * 0.5f), S, Red); X -= S + G; }
        if (bExit)  { DrawHollowSquare(OutDrawElements, L, AllottedGeometry, FVector2f(X, -O * 0.5f), O, R, Red, Bg); }
    }
    else if (Style == 15) // E1: squares, right side horizontal
    {
        auto X = W + P;
        if (bEntry) { DrawFilledSquare(OutDrawElements, L, AllottedGeometry, FVector2f(X, CY - S * 0.5f), S, Red); X += S + G; }
        if (bExit)  { DrawHollowSquare(OutDrawElements, L, AllottedGeometry, FVector2f(X, CY - O * 0.5f), O, R, Red, Bg); }
    }
    else if (Style == 16) // E2: diamonds, top-right overlay
    {
        auto X = W - 2.0f;
        if (bEntry) { DrawFilledDiamond(OutDrawElements, L, AllottedGeometry, FVector2f(X, -2.0f), S, Red); X -= S + G + 2.0f; }
        if (bExit)  { DrawFilledDiamond(OutDrawElements, L, AllottedGeometry, FVector2f(X, -2.0f), S, FadedRed); }
    }
    else if (Style == 17) // E3: diamonds, right side horizontal
    {
        auto X = W + P + S * 0.5f;
        if (bEntry) { DrawFilledDiamond(OutDrawElements, L, AllottedGeometry, FVector2f(X, CY), S, Red); X += S + G; }
        if (bExit)  { DrawFilledDiamond(OutDrawElements, L, AllottedGeometry, FVector2f(X, CY), S, FadedRed); }
    }

    // =============================================================================================================
    // GROUP F: Mixed / creative
    // =============================================================================================================

    else if (Style == 18) // F0: left arrow markers (▶ entry, ◀ exit) using triangular squares
    {
        if (bEntry)
        {
            DrawFilledSquare(OutDrawElements, L, AllottedGeometry, FVector2f(-10.0f, CY - 4.0f), 8.0f, Red);
            DrawFilledSquare(OutDrawElements, L, AllottedGeometry, FVector2f(-6.0f, CY - 2.0f), 4.0f, Red);
        }
        if (bExit)
        {
            DrawFilledSquare(OutDrawElements, L, AllottedGeometry, FVector2f(W + 2.0f, CY - 4.0f), 8.0f, Red);
            DrawFilledSquare(OutDrawElements, L, AllottedGeometry, FVector2f(W + 2.0f, CY - 2.0f), 4.0f, Red);
        }
    }
    else if (Style == 19) // F1: underline (entry=solid, exit=dotted)
    {
        if (bEntry)
        {
            auto Geom = AllottedGeometry.MakeChild(FVector2f(W, 2.0f), FSlateLayoutTransform(FVector2f(0.0f, H + 3.0f)));
            FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Red);
        }
        if (bExit)
        {
            auto DotW = 6.0f;
            auto Y2 = bEntry ? H + 7.0f : H + 3.0f;
            for (auto DotX = 0.0f; DotX < W; DotX += DotW + 3.0f)
            {
                auto Geom = AllottedGeometry.MakeChild(FVector2f(DotW, 2.0f), FSlateLayoutTransform(FVector2f(DotX, Y2)));
                FSlateDrawElement::MakeBox(OutDrawElements, L, Geom.ToPaintGeometry(),
                    FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Red);
            }
        }
    }
    else if (Style == 20) // F2: top-right single icon — nested circles (entry inside exit ring)
    {
        auto X = W - 4.0f;
        auto Y2 = -6.0f;
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, Y2), 14.0f, R, Red, Bg); }
        if (bEntry) { DrawFilledCircle(OutDrawElements, L + 2, AllottedGeometry, FVector2f(X + 3.0f, Y2 + 3.0f), 8.0f, Red); }
    }
    else if (Style == 21) // F3: bottom-right single icon — nested circles
    {
        auto X = W - 4.0f;
        auto Y2 = H - 8.0f;
        if (bExit)  { DrawHollowCircle(OutDrawElements, L, AllottedGeometry, FVector2f(X, Y2), 14.0f, R, Red, Bg); }
        if (bEntry) { DrawFilledCircle(OutDrawElements, L + 2, AllottedGeometry, FVector2f(X + 3.0f, Y2 + 3.0f), 8.0f, Red); }
    }
    // Styles 22-23: inline breakpoint indicators — handled entirely in UpdateGraphNode, no OnPaint needed

    return L + 2;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    CreateTaskRows()
    -> TSharedRef<SWidget>
{
    if (NOT _StateNode || _StateNode->Get_Tasks().Num() == 0)
    { return SNew(SBox).HeightOverride(0.0f); }

    // Check expand flag on the owning graph
    if (auto* Graph = Cast<UCkSmDebugGraph>(_StateNode->GetGraph()))
    {
        if (NOT Graph->LayoutParams.bExpandTasks)
        { return SNew(SBox).HeightOverride(0.0f); }
    }

    auto TaskBox = SNew(SVerticalBox);

    // Separator line below header
    TaskBox->AddSlot()
        .AutoHeight()
        [
            SNew(SBox)
                .HeightOverride(1.0f)
                [
                    SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(FCkSmDebuggerStyle::Color_Sm_HeaderSeparator)
                ]
        ];

    for (auto TaskIdx = 0; TaskIdx < _StateNode->Get_Tasks().Num(); ++TaskIdx)
    {
        auto& Task = _StateNode->Get_Tasks()[TaskIdx];

        auto TaskNameDepth = 1;
        if (auto* TaskGraph = Cast<UCkSmDebugGraph>(_StateNode->GetGraph()))
        { TaskNameDepth = TaskGraph->LayoutParams.NameDepth; }
        auto ClassName = FCkSmLayoutParams::ComputeDisplayName(Task.ClassName, TaskNameDepth);

        TaskBox->AddSlot()
            .AutoHeight()
            .Padding(FCkSmDebuggerStyle::Sm_NodePadding, 2.0f)
            [
                SNew(SHorizontalBox)

                // Status dot — color updates every frame via delegate
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                    [
                        SNew(SBox)
                            .WidthOverride(FCkSmDebuggerStyle::Sm_StateIconSize)
                            .HeightOverride(FCkSmDebuggerStyle::Sm_StateIconSize)
                            [
                                SNew(SBorder)
                                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                    .BorderBackgroundColor_Lambda([StateNodePtr = _StateNode, TaskIdx, this]()
                                    {
                                        if (NOT StateNodePtr || TaskIdx >= StateNodePtr->Get_Tasks().Num())
                                        { return FCkSmDebuggerStyle::Color_Text_Muted; }

                                        auto Color = GetTaskResultBrushColor(StateNodePtr->Get_Tasks()[TaskIdx].LastResult);

                                        // Fade out when state is not active
                                        if (NOT StateNodePtr->Get_IsCurrentState())
                                        { Color.A *= 0.3f; }

                                        return Color;
                                    })
                            ]
                    ]

                // Task class name — also fades when inactive
                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(ClassName))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity_Lambda([StateNodePtr = _StateNode]()
                            {
                                auto Color = FCkSmDebuggerStyle::Color_Sm_TextSecondary;
                                if (StateNodePtr && NOT StateNodePtr->Get_IsCurrentState())
                                { Color.A *= 0.4f; }
                                return FSlateColor(Color);
                            })
                    ]
            ];
    }

    return TaskBox;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    GetTaskResultBrushColor(
        ECk_SmTaskResult InResult) const
    -> FLinearColor
{
    switch (InResult)
    {
    case ECk_SmTaskResult::Running:   return FCkSmDebuggerStyle::Color_Sm_TaskRunning;
    case ECk_SmTaskResult::Succeeded: return FCkSmDebuggerStyle::Color_Sm_TaskSucceeded;
    case ECk_SmTaskResult::Failed:    return FCkSmDebuggerStyle::Color_Sm_TaskFailed;
    default:                          return FCkSmDebuggerStyle::Color_Text_Muted;
    }
}

// --------------------------------------------------------------------------------------------------------------------
