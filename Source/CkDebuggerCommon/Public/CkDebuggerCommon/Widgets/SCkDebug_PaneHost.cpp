#include "SCkDebug_PaneHost.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_GlowWrap.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

// ====================================================================================================================

namespace ck_debug_pane_host
{
    auto Is_Cards() -> bool
    { return ck::debug_axes::Get_CardOuterExtent() > 0.0f; }

    auto Get_OuterBrush() -> const FSlateBrush*
    { return Is_Cards() ? ck::debug_axes::Get_CardBrush() : CkStyle::GetFilledBrush(); }

    auto Get_OuterTint() -> FSlateColor
    { return FSlateColor{Is_Cards() ? CkStyle::Border() : ck::debug_axes::Get_SurfaceTint(2)}; }

    auto Get_RingPadding() -> FMargin
    { return Is_Cards() ? FMargin{CkStyle::RingWidth()} : FMargin{0.0f}; }

    auto Get_SurfaceBrush() -> const FSlateBrush*
    { return Is_Cards() ? ck::debug_axes::Get_CardSurfaceBrush() : CkStyle::GetFilledBrush(); }

    auto Get_ContentPadding(const ECkDebugPaneContent InContentMode) -> FMargin
    {
        if (Is_Cards() && InContentMode == ECkDebugPaneContent::OpaqueRenderer)
        { return FMargin{CkStyle::SpaceS}; }

        return FMargin{0.0f};
    }
}

// ====================================================================================================================

auto
    SCkDebug_PaneHost::
    Construct(const FArguments& InArgs)
    -> void
{
    const auto ContentMode = InArgs._ContentMode;

    ChildSlot
    [
        SNew(SCkDebug_GlowWrap)
        .Extent_Static(&ck::debug_axes::Get_CardOuterExtent)
        .GlowColor(FLinearColor::Transparent)
        [
            SNew(SBorder)
            .BorderImage_Static(&ck_debug_pane_host::Get_OuterBrush)
            .BorderBackgroundColor_Static(&ck_debug_pane_host::Get_OuterTint)
            .Padding_Static(&ck_debug_pane_host::Get_RingPadding)
            [
                SNew(SBorder)
                .BorderImage_Static(&ck_debug_pane_host::Get_SurfaceBrush)
                .BorderBackgroundColor_Lambda([]
                {
                    return FSlateColor{ck::debug_axes::Get_SurfaceTint(2)};
                })
                .Padding(FMargin{0.0f})
                [
                    SNew(SBox)
                    .Padding_Lambda([ContentMode]
                    {
                        return ck_debug_pane_host::Get_ContentPadding(ContentMode);
                    })
                    [
                        InArgs._Content.Widget
                    ]
                ]
            ]
        ]
    ];
}

// ====================================================================================================================
