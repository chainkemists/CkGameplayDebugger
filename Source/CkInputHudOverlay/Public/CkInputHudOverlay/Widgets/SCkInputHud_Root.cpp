#include "CkInputHudOverlay/Widgets/SCkInputHud_Root.h"

#include "CkInputHudOverlay/Model/CkInputHud_Model.h"
#include "CkInputHudOverlay/Widgets/SCkInputHud_Ribbon.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_root
{
    constexpr auto PanelPadding = 8.0f;

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

    const auto WeakModel = _Model;

    auto Panel = SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get_SquareBrush())
        .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::BgRoot(), 0.75f))
        .Padding(FMargin{PanelPadding})
        [
            SNew(SVerticalBox)

            // ---- Ribbon ----
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SCkInputHud_Ribbon)
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

                    return FText::FromString(FString::Printf(
                        TEXT("L %.2f,%.2f   R %.2f,%.2f"), Left.X, Left.Y, Right.X, Right.Y));
                })
            ]

            // ---- Layer stack ----
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin{0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f})
            [
                SNew(STextBlock)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor{CkStyle::Info()})
                .Text_Lambda([WeakModel]() -> FText
                {
                    const auto Model = WeakModel.Pin();
                    if (NOT Model.IsValid())
                    { return FText::GetEmpty(); }

                    return FText::FromString(Model->Get_LayerLine());
                })
            ]
        ];

    _Panel = Panel;
    _Panel->SetRenderOpacity(_PanelOpacity);

    ChildSlot
    .HAlign(HAlign_Fill)
    .VAlign(VAlign_Fill)
    [
        SAssignNew(_AnchorBox, SBox)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding(FMargin{PanelPadding})
        [
            Panel
        ]
    ];

    SetVisibility(EVisibility::HitTestInvisible);
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
    const auto Scale  = FMath::Clamp(_Scale.Get(), 0.25f, 4.0f);

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

    if (NOT FMath::IsNearlyEqual(Scale, _AppliedScale) && _Panel.IsValid())
    {
        _Panel->SetRenderTransform(TOptional<FSlateRenderTransform>{FSlateRenderTransform{Scale}});
        _AppliedScale = Scale;
    }

    const auto Model = _Model.Pin();
    const auto HasContent = Model.IsValid() && NOT Model->Get_Events().IsEmpty();

    // Snapping back rather than easing in: the frame an event lands is the frame it must be readable on.
    const auto Target = HasContent
        ? 1.0f
        : FMath::Max(0.0f, _PanelOpacity - InDeltaTime / FadeOutSeconds);

    _PanelOpacity = Target;

    const auto Applied = Target * FMath::Clamp(_Opacity.Get(), 0.15f, 1.0f);
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
    if (_Mode.Get() != 2)
    { return false; }

    const auto Model = _Model.Pin();
    if (NOT Model.IsValid())
    { return false; }

    return Model->Get_ActiveInputType() == ECommonInputType::Gamepad;
}

// --------------------------------------------------------------------------------------------------------------------
