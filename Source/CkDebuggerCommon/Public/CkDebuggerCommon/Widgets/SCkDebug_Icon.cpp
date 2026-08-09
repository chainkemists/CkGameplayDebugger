#include "SCkDebug_Icon.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

// ====================================================================================================================

namespace ck_debug_icon
{
    // The axis' Medium option — the size this widget shipped with, and the denominator that
    // turns IconSize into a RATIO. Every existing call site passes an explicit Size tuned to
    // its row (11 / 14 / 16), so an absolute axis value would flatten those distinctions;
    // scaling preserves the relative sizing the surfaces were laid out around and is exactly
    // 1.0 under Classic, which is what keeps the default rendering byte-identical.
    constexpr auto BaselineSize = 16.0f;

    auto Get_SizeScale() -> float
    {
        return ck::debug_axes::Get_IconSize(UCkDebuggerStyleSettings::Get_Selection()) / BaselineSize;
    }
}

// ====================================================================================================================

auto
    SCkDebug_Icon::
    Construct(const FArguments& InArgs)
    -> void
{
    // Tooltip lives on the compound widget so the whole glyph footprint is the
    // hover target — the wrapped SImage stays passive.
    SetToolTipText(InArgs._Meaning);

    const auto BaseSize = InArgs._Size.Get(FVector2D{ck_debug_icon::BaselineSize, ck_debug_icon::BaselineSize});

    ChildSlot
    [
        SNew(SBox)
            .WidthOverride_Lambda([BaseSize]() -> FOptionalSize
            {
                return FOptionalSize{static_cast<float>(BaseSize.X) * ck_debug_icon::Get_SizeScale()};
            })
            .HeightOverride_Lambda([BaseSize]() -> FOptionalSize
            {
                return FOptionalSize{static_cast<float>(BaseSize.Y) * ck_debug_icon::Get_SizeScale()};
            })
            [
                SNew(SImage)
                    .Image(InArgs._Brush)
                    .ColorAndOpacity(InArgs._ColorAndOpacity)
            ]
    ];
}

// ====================================================================================================================
