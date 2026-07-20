#include "SCkDebug_Icon.h"

#include "CkCore/Macros/CkMacros.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

// ====================================================================================================================

auto
    SCkDebug_Icon::
    Construct(const FArguments& InArgs)
    -> void
{
    // Tooltip lives on the compound widget so the whole glyph footprint is the
    // hover target — the wrapped SImage stays passive.
    SetToolTipText(InArgs._Meaning);

    const auto Size = InArgs._Size.Get(FVector2D{16.0f, 16.0f});

    ChildSlot
    [
        SNew(SBox)
            .WidthOverride(Size.X)
            .HeightOverride(Size.Y)
            [
                SNew(SImage)
                    .Image(InArgs._Brush)
                    .ColorAndOpacity(InArgs._ColorAndOpacity)
            ]
    ];
}

// ====================================================================================================================
