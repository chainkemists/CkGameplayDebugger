#include "CkIntentDebugger/Window/SCkIntentDebugger_OctantDial.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_octant_dial
{
    constexpr auto NumOctants = 8;
    constexpr auto SpokeThickness = 1.0f;
    constexpr auto LitThickness = 3.0f;
    constexpr auto DotHalfExtent = 3.0f;
    constexpr auto Margin = 10.0f;

    // Screen space grows downward while the axis pair's +Y is up, so every point is flipped once, here.
    auto
        To_Local(
            const FVector2D& InAxisSpace,
            const FVector2D& InCentre,
            float InRadius)
        -> FVector2D
    {
        return FVector2D{InCentre.X + (InAxisSpace.X * InRadius), InCentre.Y - (InAxisSpace.Y * InRadius)};
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_OctantDial::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _DesiredSize = InArgs._DesiredSize;
    _Octant = InArgs._Octant;
    _AxisValue = InArgs._AxisValue;
    _NeutralRadius = InArgs._NeutralRadius;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_OctantDial::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D{_DesiredSize, _DesiredSize};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_OctantDial::
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const
    -> int32
{
    const auto Size = InAllottedGeometry.GetLocalSize();
    const auto Centre = FVector2D{Size.X * 0.5, Size.Y * 0.5};
    const auto Radius = FMath::Max(
        4.0f, static_cast<float>(FMath::Min(Size.X, Size.Y) * 0.5) - ck_intent_debugger_octant_dial::Margin);

    const auto PaintGeometry = InAllottedGeometry.ToPaintGeometry(
        FVector2f{Size}, FSlateLayoutTransform{});

    const auto SpokeColor = CkStyle::Border();
    const auto LitColor = CkStyle::Accent();
    const auto DotColor = CkStyle::TextStrong();
    const auto NeutralColor = CkStyle::TextMute();

    const auto Lit = _Octant.Get(ECk_Intent_Octant::Neutral);

    for (auto Index = 0; Index < ck_intent_debugger_octant_dial::NumOctants; ++Index)
    {
        const auto Octant = static_cast<ECk_Intent_Octant>(Index + 1);
        const auto Direction = ck::intent_debugger::Get_OctantUnitVector(Octant);
        const auto IsLit = Octant == Lit;

        auto Points = TArray<FVector2D>{};
        Points.Add(Centre);
        Points.Add(ck_intent_debugger_octant_dial::To_Local(Direction, Centre, Radius));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            InLayerId,
            PaintGeometry,
            Points,
            ESlateDrawEffect::None,
            IsLit ? LitColor : SpokeColor,
            true,
            IsLit ? ck_intent_debugger_octant_dial::LitThickness : ck_intent_debugger_octant_dial::SpokeThickness);
    }

    // The neutral band: below this magnitude the row reads Neutral no matter where the stick points.
    const auto NeutralRadius = FMath::Clamp(_NeutralRadius.Get(0.0f), 0.0f, 1.0f);
    if (NeutralRadius > 0.0f)
    {
        auto Ring = TArray<FVector2D>{};
        constexpr auto RingSegments = 24;

        for (auto Index = 0; Index <= RingSegments; ++Index)
        {
            const auto Radians = (2.0 * PI * static_cast<double>(Index)) / static_cast<double>(RingSegments);
            const auto Point = FVector2D{FMath::Cos(Radians), FMath::Sin(Radians)} * NeutralRadius;

            Ring.Add(ck_intent_debugger_octant_dial::To_Local(Point, Centre, Radius));
        }

        FSlateDrawElement::MakeLines(
            OutDrawElements, InLayerId, PaintGeometry, Ring, ESlateDrawEffect::None, NeutralColor, true,
            ck_intent_debugger_octant_dial::SpokeThickness);
    }

    const auto Axis = _AxisValue.Get(FVector2D::ZeroVector);
    const auto DotCentre = ck_intent_debugger_octant_dial::To_Local(
        FVector2D{FMath::Clamp(Axis.X, -1.0, 1.0), FMath::Clamp(Axis.Y, -1.0, 1.0)}, Centre, Radius);

    const auto DotSize = FVector2f{
        ck_intent_debugger_octant_dial::DotHalfExtent * 2.0f, ck_intent_debugger_octant_dial::DotHalfExtent * 2.0f};

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        InLayerId + 1,
        InAllottedGeometry.ToPaintGeometry(
            DotSize,
            FSlateLayoutTransform{FVector2f{
                static_cast<float>(DotCentre.X) - ck_intent_debugger_octant_dial::DotHalfExtent,
                static_cast<float>(DotCentre.Y) - ck_intent_debugger_octant_dial::DotHalfExtent}}),
        FAppStyle::GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        DotColor);

    return InLayerId + 2;
}

// --------------------------------------------------------------------------------------------------------------------
