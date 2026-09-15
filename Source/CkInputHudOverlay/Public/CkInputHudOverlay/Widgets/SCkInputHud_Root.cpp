#include "CkInputHudOverlay/Widgets/SCkInputHud_Root.h"

#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkInputHudOverlay/Model/CkInputHud_Model.h"
#include "CkInputHudOverlay/Style/CkInputHud_RenderStyle.h"
#include "CkInputHudOverlay/Widgets/SCkInputHud_Ribbon.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/SNullWidget.h"
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
    constexpr auto AuthoredPollIntervalSeconds = 0.5;

    auto ToCssColor(const FLinearColor& InColor) -> FString
    {
        return TEXT("#") + InColor.ToFColorSRGB().ToHex();
    }

    auto ToCssPixels(float InValue) -> FString
    {
        return FString::SanitizeFloat(InValue) + TEXT("px");
    }

    auto Get_PanelRadius(ECk_InputHud_BrushShape InShape) -> float
    {
        switch (InShape)
        {
            case ECk_InputHud_BrushShape::Square:       return 0.0f;
            case ECk_InputHud_BrushShape::Rounded:      return 6.0f;
            case ECk_InputHud_BrushShape::RoundedLarge: return 8.0f;
            case ECk_InputHud_BrushShape::Pill:         return 99.0f;
            default:                                    return 8.0f;
        }
    }

    auto Get_AuthoredStyleTokens(const FCk_InputHud_RenderStyle& InStyle) -> FCkUiView::FTokens
    {
        return
        {
            {TEXT("--input-hud-panel-fill"), ToCssColor(
                InStyle.Palette.Panel.CopyWithNewOpacity(InStyle.PanelOpacity))},
            {TEXT("--input-hud-panel-outline"), ToCssColor(
                InStyle.Palette.ContainerOutline.CopyWithNewOpacity(InStyle.KeyBorderOpacity))},
            {TEXT("--input-hud-padding-x"), ToCssPixels(InStyle.PanelPaddingX)},
            {TEXT("--input-hud-padding-y"), ToCssPixels(InStyle.PanelPaddingY)},
            {TEXT("--input-hud-outline-width"), ToCssPixels(InStyle.PanelOutlineWidth)},
            {TEXT("--input-hud-outline-radius"), ToCssPixels(Get_PanelRadius(InStyle.PanelBrushShape))},
            {TEXT("--input-hud-small-gap"), ToCssPixels(FCkDebuggerStyle::Padding_Small)},
            {TEXT("--input-hud-micro-font-size"), FString::FromInt(CkStyle::FontSizeMicro()) + TEXT("px")},
            {TEXT("--input-hud-text-dim"), ToCssColor(CkStyle::TextDim())},
            {TEXT("--input-hud-text-info"), ToCssColor(CkStyle::Info())},
            {TEXT("--input-hud-text-mute"), ToCssColor(CkStyle::TextMute())},
        };
    }

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
#if WITH_DEV_AUTOMATION_TESTS
    _AuthoredMarkupPath = InArgs._AuthoredMarkupPathOverride;
    _AuthoredStylesheetPath = InArgs._AuthoredStylesheetPathOverride;
#endif

    SAssignNew(_Ribbon, SCkInputHud_Ribbon)
        .Model(_Model);

    ChildSlot
    .HAlign(HAlign_Fill)
    .VAlign(VAlign_Fill)
    [
        SAssignNew(_AnchorBox, SBox)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding(FMargin{_AnchorOffset.Get().X, _AnchorOffset.Get().Y})
        [
            SAssignNew(_PresentationHost, SBox)
        ]
    ];

    _PresentationHost->SetContent(DoCreate_NativePresentation());
    _PresentationHost->SetRenderOpacity(_PanelOpacity);

    SetVisibility(TAttribute<EVisibility>::CreateLambda([]() -> EVisibility
    {
        return ck::diagnostic_visibility::Is_HiddenForStreamerMode()
            ? EVisibility::Collapsed
            : EVisibility::HitTestInvisible;
    }));

    DoBuild_AuthoredPresentation();
}

SCkInputHud_Root::~SCkInputHud_Root()
{
    Release_AuthoredPresentation();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Root::
    DoCreate_NativePresentation()
    -> TSharedRef<SWidget>
{
    const auto RenderStyle = ck::input_hud::Get_ActiveRenderStyle();
    const TWeakPtr<SCkInputHud_Root> WeakRoot = SharedThis(this);

    auto PanelFill = SNew(SBorder)
        .BorderImage(ck::input_hud::Resolve_Brush(RenderStyle.PanelBrushShape))
        .BorderBackgroundColor(RenderStyle.Palette.Panel.CopyWithNewOpacity(RenderStyle.PanelOpacity))
        .Padding(FMargin{RenderStyle.PanelPaddingX, RenderStyle.PanelPaddingY})
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                _Ribbon.ToSharedRef()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin{0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f})
            [
                SNew(STextBlock)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                .Visibility_Lambda([WeakRoot]() -> EVisibility
                {
                    const auto Root = WeakRoot.Pin();
                    return Root.IsValid() && Root->Get_ShowSticks()
                        ? EVisibility::HitTestInvisible
                        : EVisibility::Collapsed;
                })
                .Text_Lambda([WeakRoot]() -> FText
                {
                    const auto Root = WeakRoot.Pin();
                    return Root.IsValid() ? Root->Get_SticksText() : FText::GetEmpty();
                })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin{0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f})
            [
                SNew(SHorizontalBox)
                .Visibility_Lambda([WeakRoot]() -> EVisibility
                {
                    const auto Root = WeakRoot.Pin();
                    return Root.IsValid() && Root->Get_ShowLayer()
                        ? EVisibility::HitTestInvisible
                        : EVisibility::Collapsed;
                })

                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor{CkStyle::Info()})
                    .Text_Lambda([WeakRoot]() -> FText
                    {
                        const auto Root = WeakRoot.Pin();
                        return Root.IsValid() ? Root->Get_LayerPrimaryText() : FText::GetEmpty();
                    })
                ]

                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    .Text_Lambda([WeakRoot]() -> FText
                    {
                        const auto Root = WeakRoot.Pin();
                        return Root.IsValid() ? Root->Get_LayerRemainderText() : FText::GetEmpty();
                    })
                ]
            ]
        ];

    auto Panel = SNew(SBorder)
        .BorderImage(ck::input_hud::Resolve_Brush(RenderStyle.PanelBrushShape))
        .BorderBackgroundColor(RenderStyle.Palette.ContainerOutline.CopyWithNewOpacity(
            RenderStyle.KeyBorderOpacity))
        .Padding(FMargin{RenderStyle.PanelOutlineWidth})
        [
            PanelFill
        ];

    _NativePanel = Panel;
    _NativePanelFill = PanelFill;

    return Panel;
}

auto
    SCkInputHud_Root::
    DoBuild_AuthoredPresentation()
    -> void
{
    if (_AuthoredPresentationReleased || NOT _PresentationHost.IsValid() || NOT _Ribbon.IsValid())
    { return; }

    if (_AuthoredMarkupPath.IsEmpty() || _AuthoredStylesheetPath.IsEmpty())
    {
        const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
        if (NOT Plugin.IsValid())
        {
            _AuthoredFailure = TEXT("Unable to locate the CkDebugger plugin for Input HUD authored resources.");
            return;
        }

        const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
        _AuthoredMarkupPath = FPaths::Combine(Directory, TEXT("InputHudOverlay.ui.html"));
        _AuthoredStylesheetPath = FPaths::Combine(Directory, TEXT("InputHudOverlay.ui.css"));
    }

    const TWeakPtr<SCkInputHud_Root> WeakRoot = SharedThis(this);
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("input-hud-sticks"), TAttribute<FText>::CreateLambda([WeakRoot]() -> FText
    {
        const auto Root = WeakRoot.Pin();
        return Root.IsValid() ? Root->Get_SticksText() : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("input-hud-layer-primary"), TAttribute<FText>::CreateLambda([WeakRoot]() -> FText
    {
        const auto Root = WeakRoot.Pin();
        return Root.IsValid() ? Root->Get_LayerPrimaryText() : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("input-hud-layer-remainder"), TAttribute<FText>::CreateLambda([WeakRoot]() -> FText
    {
        const auto Root = WeakRoot.Pin();
        return Root.IsValid() ? Root->Get_LayerRemainderText() : FText::GetEmpty();
    }));
    Data.Visibility.Add(TEXT("input-hud-sticks-visible"), TAttribute<bool>::CreateLambda([WeakRoot]() -> bool
    {
        const auto Root = WeakRoot.Pin();
        return Root.IsValid() && Root->Get_ShowSticks();
    }));
    Data.Visibility.Add(TEXT("input-hud-layer-visible"), TAttribute<bool>::CreateLambda([WeakRoot]() -> bool
    {
        const auto Root = WeakRoot.Pin();
        return Root.IsValid() && Root->Get_ShowLayer();
    }));
    Data.CanDispatchEvents = TAttribute<bool>(false);

    _PresentationHost->SetContent(SNullWidget::NullWidget);
    _NativePanel.Reset();
    _NativePanelFill.Reset();

    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("input-hud-ribbon"), _Ribbon.ToSharedRef());
    const auto Tokens = ck_input_hud_root::Get_AuthoredStyleTokens(ck::input_hud::Get_ActiveRenderStyle());
    const TSharedRef<FCkUiView> View = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, Tokens, CkStyle::RegularFont(CkStyle::FontSizeMicro()), MoveTemp(Data));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));

    View->SetFiles(_AuthoredMarkupPath, _AuthoredStylesheetPath);
    _AuthoredView = View;
    View->PollFiles(Tokens);

    _UsingNativeFallback = NOT View->GetLastResult().Succeeded;
    _AuthoredFailure = _UsingNativeFallback
        ? FString::Join(View->GetLastResult().Errors, TEXT("\n"))
        : FString{};
    _PresentationHost->SetContent(_UsingNativeFallback ? DoCreate_NativePresentation() : Main);
}

auto
    SCkInputHud_Root::
    DoPoll_AuthoredPresentation(
        double InCurrentTime)
    -> void
{
    using namespace ck_input_hud_root;

    if (_AuthoredPresentationReleased || NOT _AuthoredView.IsValid() || NOT _PresentationHost.IsValid() ||
        InCurrentTime < _NextAuthoredPollSeconds)
    { return; }

    _NextAuthoredPollSeconds = InCurrentTime + AuthoredPollIntervalSeconds;
    const auto Tokens = Get_AuthoredStyleTokens(ck::input_hud::Get_ActiveRenderStyle());
    const auto ContentChanged = _AuthoredView->PollFiles(Tokens);

    if (NOT _UsingNativeFallback)
    {
        _AuthoredFailure = _AuthoredView->GetLastResult().Succeeded
            ? FString{}
            : FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n"));
        return;
    }

    if (NOT ContentChanged)
    { return; }

    _PresentationHost->SetContent(SNullWidget::NullWidget);
    _NativePanel.Reset();
    _NativePanelFill.Reset();
    _AuthoredView->SetFiles(_AuthoredMarkupPath, _AuthoredStylesheetPath);
    _AuthoredView->PollFiles(Tokens);

    _UsingNativeFallback = NOT _AuthoredView->GetLastResult().Succeeded;
    _AuthoredFailure = _UsingNativeFallback
        ? FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n"))
        : FString{};
    _PresentationHost->SetContent(_UsingNativeFallback
        ? DoCreate_NativePresentation()
        : _AuthoredView->GetRegion(TEXT("main")));
}

auto
    SCkInputHud_Root::
    Release_AuthoredPresentation()
    -> void
{
    if (_AuthoredPresentationReleased)
    { return; }

    _AuthoredPresentationReleased = true;

    if (_PresentationHost.IsValid())
    { _PresentationHost->SetContent(SNullWidget::NullWidget); }

    _AuthoredView.Reset();
    _NativePanel.Reset();
    _NativePanelFill.Reset();
    _Ribbon.Reset();
}

auto
    SCkInputHud_Root::
    Get_AuthoredFailure() const
    -> FString
{
    return _AuthoredFailure;
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

    if (_AuthoredPresentationReleased)
    { return; }

    const auto Corner = _Corner.Get();
    const auto Scale  = ck::input_hud::Get_ValidOverlayScale(_Scale.Get());
    const auto RenderStyle = ck::input_hud::Get_ActiveRenderStyle();

    auto Horizontal = HAlign_Right;
    auto Vertical   = VAlign_Top;
    auto Pivot      = FVector2D{1.0, 0.0};
    Get_Alignment(Corner, Horizontal, Vertical, Pivot);

    if (Corner != _AppliedCorner && _AnchorBox.IsValid() && _PresentationHost.IsValid())
    {
        _AnchorBox->SetHAlign(Horizontal);
        _AnchorBox->SetVAlign(Vertical);
        _PresentationHost->SetRenderTransformPivot(Pivot);
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

    if (NOT FMath::IsNearlyEqual(Scale, _AppliedScale) && _PresentationHost.IsValid())
    {
        _PresentationHost->SetRenderTransform(TOptional<FSlateRenderTransform>{FSlateRenderTransform{Scale}});
        _AppliedScale = Scale;
    }

    const auto PanelFillTint = RenderStyle.Palette.Panel.CopyWithNewOpacity(RenderStyle.PanelOpacity);
    const auto PanelOutlineTint = RenderStyle.Palette.ContainerOutline.CopyWithNewOpacity(
        RenderStyle.KeyBorderOpacity);
    const auto PanelPadding = FVector2f{RenderStyle.PanelPaddingX, RenderStyle.PanelPaddingY};
    const auto SettingsRevision = UCk_InputHud_UserSettings::Get_Revision();
    if (_NativePanel.IsValid() && _NativePanelFill.IsValid() &&
        (RenderStyle.PanelBrushShape != _AppliedPanelBrushShape ||
        NOT PanelFillTint.Equals(_AppliedPanelFillTint) ||
        NOT PanelOutlineTint.Equals(_AppliedPanelOutlineTint) ||
        NOT PanelPadding.Equals(_AppliedPanelPadding)))
    {
        _NativePanel->SetBorderImage(ck::input_hud::Resolve_Brush(RenderStyle.PanelBrushShape));
        _NativePanel->SetBorderBackgroundColor(PanelOutlineTint);
        _NativePanel->SetPadding(FMargin{RenderStyle.PanelOutlineWidth});
        _NativePanelFill->SetBorderImage(ck::input_hud::Resolve_Brush(RenderStyle.PanelBrushShape));
        _NativePanelFill->SetBorderBackgroundColor(PanelFillTint);
        _NativePanelFill->SetPadding(FMargin{RenderStyle.PanelPaddingX, RenderStyle.PanelPaddingY});
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
        _NextAuthoredPollSeconds = 0.0;
    }

    DoPoll_AuthoredPresentation(InCurrentTime);

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
    if (NOT FMath::IsNearlyEqual(Applied, _AppliedOpacityProduct) && _PresentationHost.IsValid())
    {
        _AppliedOpacityProduct = Applied;
        _PresentationHost->SetRenderOpacity(Applied);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Root::
    Get_ShowSticks() const
    -> bool
{
    // Mode 1 is keyboard-only by contract; only mode 2 follows the device.
    if (_AuthoredPresentationReleased || _Mode.Get() != 2 ||
        UCk_InputHud_UserSettings::Get_MetadataMode() != ECk_InputHud_MetadataMode::Full)
    { return false; }

    const auto Model = _Model.Pin();
    if (NOT Model.IsValid())
    { return false; }

    return Model->Get_ActiveInputType() == ECommonInputType::Gamepad;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Root::
    Get_SticksText() const
    -> FText
{
    if (_AuthoredPresentationReleased)
    { return FText::GetEmpty(); }

    const auto Model = _Model.Pin();
    if (NOT Model.IsValid())
    { return FText::GetEmpty(); }

    const auto& Left  = Model->Get_LeftStick();
    const auto& Right = Model->Get_RightStick();

    return FText::FromString(ck::Format_UE(
        TEXT("L {:.2f},{:.2f}   R {:.2f},{:.2f}"), Left.X, Left.Y, Right.X, Right.Y));
}

auto
    SCkInputHud_Root::
    Get_ShowLayer() const
    -> bool
{
    if (_AuthoredPresentationReleased)
    { return false; }

    const auto Model = _Model.Pin();
    return Model.IsValid() && NOT Model->Get_LayerPrimary().IsEmpty();
}

auto
    SCkInputHud_Root::
    Get_LayerPrimaryText() const
    -> FText
{
    if (_AuthoredPresentationReleased)
    { return FText::GetEmpty(); }

    const auto Model = _Model.Pin();
    return Model.IsValid()
        ? FText::FromString(Model->Get_LayerPrimary())
        : FText::GetEmpty();
}

auto
    SCkInputHud_Root::
    Get_LayerRemainderText() const
    -> FText
{
    if (_AuthoredPresentationReleased)
    { return FText::GetEmpty(); }

    const auto Model = _Model.Pin();
    if (NOT Model.IsValid() || Model->Get_LayerRemainder().IsEmpty())
    { return FText::GetEmpty(); }

    return FText::FromString(ck::Format_UE(TEXT(" · {}"), Model->Get_LayerRemainder()));
}

// --------------------------------------------------------------------------------------------------------------------
