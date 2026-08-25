#include "CkInputHudOverlay/Widgets/SCkInputHud_Root.h"

#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkInputHudOverlay/Model/CkInputHud_Model.h"
#include "CkInputHudOverlay/Style/CkInputHud_RenderStyle.h"
#include "CkInputHudOverlay/Widgets/SCkInputHud_Ribbon.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_root
{
    // How long the panel takes to fade out once the model runs dry. Long enough that a pause between two inputs
    // does not read as the HUD flickering, short enough that an idle screen clears.
    constexpr auto FadeOutSeconds = 1.6f;

    auto
        Get_Alignment(
            int32                 InCorner,
            EHorizontalAlignment& OutHorizontal,
            EVerticalAlignment&   OutVertical,
            FVector2D&            OutPivot)
        -> void
    {
        switch (InCorner)
        {
            case 0:  OutHorizontal = HAlign_Left;  OutVertical = VAlign_Top;    OutPivot = FVector2D{0.0, 0.0}; break;
            case 2:  OutHorizontal = HAlign_Left;  OutVertical = VAlign_Bottom; OutPivot = FVector2D{0.0, 1.0}; break;
            case 3:  OutHorizontal = HAlign_Right; OutVertical = VAlign_Bottom; OutPivot = FVector2D{1.0, 1.0}; break;
            case 1:
            default: OutHorizontal = HAlign_Right; OutVertical = VAlign_Top;    OutPivot = FVector2D{1.0, 0.0}; break;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Root::
    Construct(
        const FArguments& InArgs)
    -> void
{
    using namespace ck_input_hud_root;

    _Model  = InArgs._Model;
    _Corner = InArgs._Corner;
    _Scale  = InArgs._Scale;
    _Mode   = InArgs._Mode;
    _Opacity = InArgs._Opacity;
    _AnchorOffset = InArgs._AnchorOffset;

    const auto RenderStyle = ck::input_hud::Get_ActiveRenderStyle();

    const auto WeakModel = _Model;

    auto PanelFill = SNew(SBorder)
        .BorderImage(ck::input_hud::Resolve_Brush(RenderStyle.PanelBrushShape))
        .BorderBackgroundColor(RenderStyle.Palette.Panel.CopyWithNewOpacity(RenderStyle.PanelOpacity))
        .Padding(FMargin{RenderStyle.PanelPaddingX, RenderStyle.PanelPaddingY})
        [
            SNew(SVerticalBox)

            // ---- Ribbon ----
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(_Ribbon, SCkInputHud_Ribbon)
                    .Model(_Model)
            ]

            // ---- Stick numeric readout ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin{0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f})
            [
                SNew(STextBlock)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                .Visibility_Lambda([this]() -> EVisibility
                {
                    return Get_ShowSticks() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                })
                .Text_Lambda([WeakModel]() -> FText
                {
                    const auto Model = WeakModel.Pin();
                    if (NOT Model.IsValid())
                    { return FText::GetEmpty(); }

                    const auto& Left  = Model->Get_LeftStick();
                    const auto& Right = Model->Get_RightStick();

                    return FText::FromString(ck::Format_UE(
                        TEXT("L {:.2f},{:.2f}   R {:.2f},{:.2f}"), Left.X, Left.Y, Right.X, Right.Y));
                })
            ]

            // ---- Layer stack ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin{0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f})
            [
                SNew(SHorizontalBox)
                .Visibility_Lambda([WeakModel]() -> EVisibility
                {
                    const auto Model = WeakModel.Pin();
                    return Model.IsValid() && NOT Model->Get_LayerPrimary().IsEmpty()
                        ? EVisibility::HitTestInvisible
                        : EVisibility::Collapsed;
                })

                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor{CkStyle::Info()})
                    .Text_Lambda([WeakModel]() -> FText
                    {
                        const auto Model = WeakModel.Pin();
                        return Model.IsValid()
                            ? FText::FromString(Model->Get_LayerPrimary())
                            : FText::GetEmpty();
                    })
                ]

                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    .Text_Lambda([WeakModel]() -> FText
                    {
                        const auto Model = WeakModel.Pin();
                        if (NOT Model.IsValid() || Model->Get_LayerRemainder().IsEmpty())
                        { return FText::GetEmpty(); }

                        return FText::FromString(ck::Format_UE(
                            TEXT(" · {}"), Model->Get_LayerRemainder()));
                    })
                ]
            ]
        ];

    auto Panel = SNew(SBorder)
        // Outline brushes carry their own authored white ring. A filled outer shell lets the user tint the ring;
        // the nested panel fill below covers its center and leaves exactly PanelOutlineWidth visible.
        .BorderImage(ck::input_hud::Resolve_Brush(RenderStyle.PanelBrushShape))
        .BorderBackgroundColor(RenderStyle.Palette.ContainerOutline.CopyWithNewOpacity(
            RenderStyle.KeyBorderOpacity))
        .Padding(FMargin{RenderStyle.PanelOutlineWidth})
        [
            PanelFill
        ];

    _Panel = Panel;
    _PanelFill = PanelFill;
    _Panel->SetRenderOpacity(_PanelOpacity);

    ChildSlot
    .HAlign(HAlign_Fill)
    .VAlign(VAlign_Fill)
    [
        SAssignNew(_AnchorBox, SBox)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding(FMargin{_AnchorOffset.Get().X, _AnchorOffset.Get().Y})
        [
            Panel
        ]
    ];

    SetVisibility(TAttribute<EVisibility>::CreateLambda([]() -> EVisibility
    {
        return ck::diagnostic_visibility::Is_HiddenForStreamerMode()
            ? EVisibility::Collapsed
            : EVisibility::HitTestInvisible;
    }));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Root::
    Tick(
        const FGeometry& InAllottedGeometry,
        const double     InCurrentTime,
        const float      InDeltaTime)
    -> void
{
    using namespace ck_input_hud_root;

    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    const auto Corner = _Corner.Get();
    const auto Scale  = ck::input_hud::Get_ValidOverlayScale(_Scale.Get());
    const auto RenderStyle = ck::input_hud::Get_ActiveRenderStyle();

    auto Horizontal = HAlign_Right;
    auto Vertical   = VAlign_Top;
    auto Pivot      = FVector2D{1.0, 0.0};
    Get_Alignment(Corner, Horizontal, Vertical, Pivot);

    if (Corner != _AppliedCorner && _AnchorBox.IsValid() && _Panel.IsValid())
    {
        _AnchorBox->SetHAlign(Horizontal);
        _AnchorBox->SetVAlign(Vertical);
        _Panel->SetRenderTransformPivot(Pivot);
        _AppliedCorner = Corner;
    }

    // Inset from the anchored corner. SBox padding on an aligned child already means "distance from the aligned
    // edge", so one FMargin covers all four corners and switching corners MIRRORS the overlay instead of throwing
    // it off screen -- which is why the setting is a positive inset rather than a signed screen-space delta.
    const auto AnchorOffset = _AnchorOffset.Get();
    if (NOT AnchorOffset.Equals(_AppliedAnchorOffset) && _AnchorBox.IsValid())
    {
        _AnchorBox->SetPadding(FMargin{AnchorOffset.X, AnchorOffset.Y});
        _AppliedAnchorOffset = AnchorOffset;
    }

    if (NOT FMath::IsNearlyEqual(Scale, _AppliedScale) && _Panel.IsValid())
    {
        _Panel->SetRenderTransform(TOptional<FSlateRenderTransform>{FSlateRenderTransform{Scale}});
        _AppliedScale = Scale;
    }

    const auto PanelFillTint = RenderStyle.Palette.Panel.CopyWithNewOpacity(RenderStyle.PanelOpacity);
    const auto PanelOutlineTint = RenderStyle.Palette.ContainerOutline.CopyWithNewOpacity(
        RenderStyle.KeyBorderOpacity);
    const auto PanelPadding = FVector2f{RenderStyle.PanelPaddingX, RenderStyle.PanelPaddingY};
    const auto SettingsRevision = UCk_InputHud_UserSettings::Get_Revision();
    if (_Panel.IsValid() && _PanelFill.IsValid() && (RenderStyle.PanelBrushShape != _AppliedPanelBrushShape ||
        NOT PanelFillTint.Equals(_AppliedPanelFillTint) ||
        NOT PanelOutlineTint.Equals(_AppliedPanelOutlineTint) ||
        NOT PanelPadding.Equals(_AppliedPanelPadding)))
    {
        _Panel->SetBorderImage(ck::input_hud::Resolve_Brush(RenderStyle.PanelBrushShape));
        _Panel->SetBorderBackgroundColor(PanelOutlineTint);
        _Panel->SetPadding(FMargin{RenderStyle.PanelOutlineWidth});
        _PanelFill->SetBorderImage(ck::input_hud::Resolve_Brush(RenderStyle.PanelBrushShape));
        _PanelFill->SetBorderBackgroundColor(PanelFillTint);
        _PanelFill->SetPadding(FMargin{RenderStyle.PanelPaddingX, RenderStyle.PanelPaddingY});
        _AppliedPanelBrushShape = RenderStyle.PanelBrushShape;
        _AppliedPanelFillTint   = PanelFillTint;
        _AppliedPanelOutlineTint = PanelOutlineTint;
        _AppliedPanelPadding    = PanelPadding;
    }

    if (SettingsRevision != _AppliedSettingsRevision)
    {
        _AppliedSettingsRevision = SettingsRevision;
        if (_Ribbon.IsValid())
        { _Ribbon->Invalidate(EInvalidateWidgetReason::Layout); }
        Invalidate(EInvalidateWidgetReason::Layout);
    }

    const auto Model = _Model.Pin();
    const auto HasContent = Model.IsValid() && NOT Model->Get_Events().IsEmpty();

    if (HasContent && _Ribbon.IsValid())
    {
        // Event colors, history fade, release easing, and the press pop are all time-derived. Explicit paint
        // invalidation keeps PIE and the retained Style Lab sample on the same animation clock.
        _Ribbon->Invalidate(EInvalidateWidgetReason::Paint);

        // A live hold changes the bar width and duration text, which can change the desired width as it grows.
        if (Model->Get_HeldNum() > 0)
        { _Ribbon->Invalidate(EInvalidateWidgetReason::Layout); }
    }

    // Snapping back rather than easing in: the frame an event lands is the frame it must be readable on.
    const auto Target = HasContent
        ? 1.0f
        : FMath::Max(0.0f, _PanelOpacity - InDeltaTime / FadeOutSeconds);

    _PanelOpacity = Target;

    // One RenderOpacity for the whole strip. Slate blends it down into the panel fill, the readouts, AND the
    // ribbon's draw elements, so lowering the user's Overall opacity reveals the game behind the overlay instead
    // of blending the keys into a panel that stays put.
    const auto Applied = ck::input_hud::Get_ComposedOverlayOpacity(
        Target, _Opacity.Get(), RenderStyle.OverallOpacity);
    if (NOT FMath::IsNearlyEqual(Applied, _AppliedOpacityProduct) && _Panel.IsValid())
    {
        _AppliedOpacityProduct = Applied;
        _Panel->SetRenderOpacity(Applied);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Root::
    Get_ShowSticks() const
    -> bool
{
    // Mode 1 is keyboard-only by contract; only mode 2 follows the device.
    if (_Mode.Get() != 2 || UCk_InputHud_UserSettings::Get_MetadataMode() != ECk_InputHud_MetadataMode::Full)
    { return false; }

    const auto Model = _Model.Pin();
    if (NOT Model.IsValid())
    { return false; }

    return Model->Get_ActiveInputType() == ECommonInputType::Gamepad;
}

// --------------------------------------------------------------------------------------------------------------------
