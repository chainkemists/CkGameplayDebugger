#include "SCkDebug_GlowWrap.h"

#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"

// ====================================================================================================================

auto
    SCkDebug_GlowWrap::
    Construct(const FArguments& InArgs)
    -> void
{
    const auto GlowBrush = InArgs._Tight
        ? FCkDebuggerCommonStyle::Get_GlowTightBrush()
        : FCkDebuggerCommonStyle::Get_GlowSoftBrush();

    auto GlowColor = InArgs._GlowColor;
    const auto GlowOpacity = InArgs._GlowOpacity;

    const auto ResolveColor = [GlowColor, GlowOpacity]() -> FSlateColor
    {
        auto Color = GlowColor.Get(FLinearColor::Transparent);
        Color.A *= GlowOpacity;
        return FSlateColor{Color};
    };

    const auto ResolveVisibility = [GlowColor]() -> EVisibility
    {
        return GlowColor.Get(FLinearColor::Transparent).A > KINDA_SMALL_NUMBER
            ? EVisibility::HitTestInvisible
            : EVisibility::Collapsed;
    };

    ChildSlot
    [
        SNew(SOverlay)

        + SOverlay::Slot()
        [
            SNew(SImage)
            .Image(GlowBrush)
            .ColorAndOpacity_Lambda(ResolveColor)
            .Visibility_Lambda(ResolveVisibility)
        ]

        + SOverlay::Slot()
        .Padding(InArgs._Extent)
        [
            InArgs._Content.Widget
        ]
    ];
}

// ====================================================================================================================
