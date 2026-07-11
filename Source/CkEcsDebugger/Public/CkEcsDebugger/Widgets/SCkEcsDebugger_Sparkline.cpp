#include "SCkEcsDebugger_Sparkline.h"

#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkEcsDebugger_Sparkline::Construct(const FArguments& InArgs) -> void
{
    Samples = InArgs._Samples;
    Color = InArgs._Color;
    DesiredSize = InArgs._DesiredSize;

    // The sample ring mutates behind Slate's back — repaint every frame.
    ForceVolatile(true);
}

auto SCkEcsDebugger_Sparkline::ComputeDesiredSize(float) const -> FVector2D
{
    return DesiredSize;
}

auto SCkEcsDebugger_Sparkline::OnPaint(
    const FPaintArgs& InArgs,
    const FGeometry& InAllottedGeometry,
    const FSlateRect& InCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 InLayerId,
    const FWidgetStyle& InWidgetStyle,
    bool InParentEnabled) const -> int32
{
    if (NOT Samples.IsValid() || Samples->Num() < 2)
    { return InLayerId; }

    const auto& Values = *Samples;
    const auto Size = InAllottedGeometry.GetLocalSize();

    auto MinValue = Values[0];
    auto MaxValue = Values[0];
    for (const auto& Value : Values)
    {
        MinValue = FMath::Min(MinValue, Value);
        MaxValue = FMath::Max(MaxValue, Value);
    }
    const auto Range = FMath::Max(MaxValue - MinValue, KINDA_SMALL_NUMBER);

    auto Points = TArray<FVector2D>{};
    Points.Reserve(Values.Num());
    for (auto Index = 0; Index < Values.Num(); ++Index)
    {
        const auto X = (Values.Num() > 1 ? static_cast<float>(Index) / (Values.Num() - 1) : 0.0f) * Size.X;
        const auto Normalized = (Values[Index] - MinValue) / Range;
        const auto Y = (1.0f - Normalized) * (Size.Y - 2.0f) + 1.0f;
        Points.Emplace(X, Y);
    }

    constexpr auto Antialias = true;
    FSlateDrawElement::MakeLines(
        OutDrawElements,
        InLayerId,
        InAllottedGeometry.ToPaintGeometry(),
        Points,
        ESlateDrawEffect::None,
        Color,
        Antialias,
        1.5f);

    return InLayerId + 1;
}
