#include "SCkDebug_Card.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_GlowWrap.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

// ====================================================================================================================

auto
    SCkDebug_Card::
    Construct(const FArguments& InArgs)
    -> void
{
    const auto Stripe   = InArgs._StripeColor;
    const auto Glow     = InArgs._GlowColor;
    const auto Selected = InArgs._Selected;
    const auto GlowExtent = InArgs._GlowExtent.IsSet()
        ? InArgs._GlowExtent
        : TAttribute<float>::CreateStatic(&ck::debug_axes::Get_CardOuterExtent);

    ChildSlot
    [
        SNew(SCkDebug_GlowWrap)
        .Extent(GlowExtent)
        .GlowColor(Glow)
        [
            SNew(SBorder)
            .BorderImage_Static(&ck::debug_axes::Get_CardBrush)
            .BorderBackgroundColor_Lambda([Selected]
            {
                if (Selected.Get(false))
                {
                    return FSlateColor{CkStyle::OverlayOf(CkStyle::Accent(), CkStyle::AlphaStrong())};
                }
                return FSlateColor{CkStyle::Border()};
            })
            .Padding_Lambda([]{ return FMargin{CkStyle::RingWidth()}; })
            [
                // The card body is a depth-2 surface: Layered keeps today's Bg2 fill, Flat pulls it
                // up to the shared tier while the corner axis selects the card shape.
                SNew(SBorder)
                .BorderImage_Static(&ck::debug_axes::Get_CardSurfaceBrush)
                .BorderBackgroundColor_Lambda([]
                {
                    return FSlateColor{ck::debug_axes::Get_SurfaceTint(2)};
                })
                .Padding(FMargin{0.0f})
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SBox)
                        .WidthOverride(3.0f)
                        .Visibility_Lambda([Stripe]
                        {
                            return Stripe.Get(FLinearColor::Transparent).A > KINDA_SMALL_NUMBER
                                ? EVisibility::SelfHitTestInvisible
                                : EVisibility::Collapsed;
                        })
                        [
                            SNew(SImage)
                            .Image(CkStyle::GetFilledBrush())
                            .ColorAndOpacity_Lambda([Stripe]
                            {
                                return FSlateColor{Stripe.Get(FLinearColor::Transparent)};
                            })
                        ]
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(InArgs._BodyPadding)
                    [
                        InArgs._Content.Widget
                    ]
                ]
            ]
        ]
    ];
}

// ====================================================================================================================
