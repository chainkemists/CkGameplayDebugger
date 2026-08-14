#include "CkDebuggerCommon/Devices/SCkDebug_DeviceGamepad.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_device_gamepad
{
    // The design box the regions below are authored in; paint scales it to the allotted geometry.
    constexpr auto DesignWidth = 236.0f;
    constexpr auto DesignHeight = 150.0f;

    constexpr auto DesiredScale = 1.35f;

    struct FRegion
    {
        FKey Key;
        const TCHAR* Label = nullptr;
        float X = 0.0f;
        float Y = 0.0f;
        float W = 0.0f;
        float H = 0.0f;
    };

    // Authored once — the pad's own layout table, same doctrine as the keyboard's.
    inline auto Get_Regions() -> const TArray<FRegion>&
    {
        static const auto Regions = TArray<FRegion>{
            {EKeys::Gamepad_LeftTrigger, TEXT("LT"), 16.0f, 4.0f, 38.0f, 14.0f},
            {EKeys::Gamepad_RightTrigger, TEXT("RT"), 182.0f, 4.0f, 38.0f, 14.0f},
            {EKeys::Gamepad_LeftShoulder, TEXT("LB"), 16.0f, 22.0f, 38.0f, 12.0f},
            {EKeys::Gamepad_RightShoulder, TEXT("RB"), 182.0f, 22.0f, 38.0f, 12.0f},

            {EKeys::Gamepad_DPad_Up, TEXT("^"), 44.0f, 58.0f, 16.0f, 16.0f},
            {EKeys::Gamepad_DPad_Left, TEXT("<"), 26.0f, 74.0f, 16.0f, 16.0f},
            {EKeys::Gamepad_DPad_Right, TEXT(">"), 62.0f, 74.0f, 16.0f, 16.0f},
            {EKeys::Gamepad_DPad_Down, TEXT("v"), 44.0f, 90.0f, 16.0f, 16.0f},

            {EKeys::Gamepad_Special_Left, TEXT("SE"), 100.0f, 58.0f, 16.0f, 10.0f},
            {EKeys::Gamepad_Special_Right, TEXT("ST"), 120.0f, 58.0f, 16.0f, 10.0f},

            {EKeys::Gamepad_FaceButton_Top, TEXT("Y"), 176.0f, 54.0f, 18.0f, 18.0f},
            {EKeys::Gamepad_FaceButton_Left, TEXT("X"), 156.0f, 72.0f, 18.0f, 18.0f},
            {EKeys::Gamepad_FaceButton_Right, TEXT("B"), 196.0f, 72.0f, 18.0f, 18.0f},
            {EKeys::Gamepad_FaceButton_Bottom, TEXT("A"), 176.0f, 90.0f, 18.0f, 18.0f},

            {EKeys::Gamepad_LeftThumbstick, TEXT("L"), 76.0f, 100.0f, 28.0f, 28.0f},
            {EKeys::Gamepad_RightThumbstick, TEXT("R"), 132.0f, 100.0f, 28.0f, 28.0f},
        };

        return Regions;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceGamepad::
    Construct(
        const FArguments& InArgs)
    -> void
{
    DoConstruct_DeviceCommon(InArgs._Snapshot, InArgs._OnKeyClicked, InArgs._KeyTooltip);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceGamepad::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D{
        ck_debug_device_gamepad::DesignWidth * ck_debug_device_gamepad::DesiredScale,
        ck_debug_device_gamepad::DesignHeight * ck_debug_device_gamepad::DesiredScale};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceGamepad::
    Get_KeyAtPosition(
        const FGeometry& InGeometry,
        const FVector2D& InLocalPos) const
    -> FKey
{
    const auto LocalSize = InGeometry.GetLocalSize();
    const auto Scale = static_cast<float>(FMath::Min(
        LocalSize.X / ck_debug_device_gamepad::DesignWidth,
        LocalSize.Y / ck_debug_device_gamepad::DesignHeight));

    if (Scale <= 0.05f)
    { return FKey{}; }

    for (const auto& Region : ck_debug_device_gamepad::Get_Regions())
    {
        const auto RegionRect = FSlateRect{
            Region.X * Scale,
            Region.Y * Scale,
            (Region.X + Region.W) * Scale,
            (Region.Y + Region.H) * Scale};

        if (RegionRect.ContainsPoint(InLocalPos))
        { return Region.Key; }
    }

    return FKey{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceGamepad::
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
    const auto* Snapshot = _Snapshot.Get(nullptr);

    const auto LocalSize = InAllottedGeometry.GetLocalSize();
    const auto Scale = static_cast<float>(FMath::Min(
        LocalSize.X / ck_debug_device_gamepad::DesignWidth,
        LocalSize.Y / ck_debug_device_gamepad::DesignHeight));

    if (Scale <= 0.05f)
    { return InLayerId; }

    const auto Connected = Snapshot != nullptr && Snapshot->DeviceConnected;
    const auto DeviceAlpha = Connected ? 1.0f : ck::debug_devices::DisconnectedAlpha;

    const auto* Brush = FCkDebuggerStyle::Get().GetBrush(TEXT("CkDebugger.Badge.Rounded"));
    const auto* FillBrush = FAppStyle::GetBrush("WhiteBrush");

    const auto Font = CkStyle::RegularFont(CkStyle::FontSizeMicro());
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    const auto BaseLayer = InLayerId;
    const auto RimLayer = InLayerId + 1;
    const auto RegionLayer = InLayerId + 2;
    const auto FillLayer = InLayerId + 3;
    const auto FlashLayer = InLayerId + 4;
    const auto LabelLayer = InLayerId + 5;

    // The body shell behind the regions — not a key, just the silhouette that makes it read as a pad.
    {
        auto BodyTint = CkStyle::Bg1();
        BodyTint.A *= DeviceAlpha;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            BaseLayer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{228.0f * Scale, 106.0f * Scale},
                FSlateLayoutTransform{FVector2f{4.0f * Scale, 40.0f * Scale}}),
            Brush,
            ESlateDrawEffect::None,
            BodyTint);
    }

    for (const auto& Region : ck_debug_device_gamepad::Get_Regions())
    {
        const auto RegionPos = FVector2f{Region.X * Scale, Region.Y * Scale};
        const auto RegionSize = FVector2f{Region.W * Scale, Region.H * Scale};

        const auto* State = Snapshot != nullptr ? Snapshot->TryGet_Key(Region.Key) : nullptr;
        const auto IsMinted = State != nullptr && State->IsMinted;
        const auto IsActionable = State != nullptr && State->IsActionable;
        const auto IsRebound = State != nullptr && State->IsRebound;
        const auto IsHighlighted = State != nullptr && State->IsHighlighted;

        if (IsActionable || IsHighlighted)
        {
            const auto RimPx = FMath::Max(1.0f, (IsHighlighted ? 1.8f : 1.2f) * Scale);
            auto RimTint = IsHighlighted ? CkStyle::Text() : (IsRebound ? CkStyle::Warn() : CkStyle::Accent());
            RimTint.A *= (IsHighlighted ? 1.0f : 0.9f) * DeviceAlpha;

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                RimLayer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{RegionSize.X + RimPx * 2.0f, RegionSize.Y + RimPx * 2.0f},
                    FSlateLayoutTransform{FVector2f{RegionPos.X - RimPx, RegionPos.Y - RimPx}}),
                Brush,
                ESlateDrawEffect::None,
                RimTint);
        }

        auto BaseTint = IsMinted ? CkStyle::Bg3() : CkStyle::Bg2();
        BaseTint.A *= DeviceAlpha;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            RegionLayer,
            InAllottedGeometry.ToPaintGeometry(RegionSize, FSlateLayoutTransform{RegionPos}),
            Brush,
            ESlateDrawEffect::None,
            BaseTint);

        if (State != nullptr)
        {
            const auto FillFraction = ck::debug_devices::Get_FillFraction(*State);

            if (FillFraction > 0.0f)
            {
                const auto Inset = 2.0f * Scale;
                const auto FillHeight = (RegionSize.Y - Inset * 2.0f) * FillFraction;

                auto FillTint = FillFraction >= 1.0f ? CkStyle::Accent() : CkStyle::AccentDim();
                FillTint.A *= 0.85f * DeviceAlpha;

                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    FillLayer,
                    InAllottedGeometry.ToPaintGeometry(
                        FVector2f{RegionSize.X - Inset * 2.0f, FillHeight},
                        FSlateLayoutTransform{FVector2f{
                            RegionPos.X + Inset,
                            RegionPos.Y + RegionSize.Y - Inset - FillHeight}}),
                    FillBrush,
                    ESlateDrawEffect::None,
                    FillTint);
            }

            const auto FlashAlpha = ck::debug_devices::Get_FlashAlpha(*State, Snapshot->LiveFrame);

            if (FlashAlpha > 0.0f)
            {
                auto FlashTint = CkStyle::Ok();
                FlashTint.A = FlashAlpha * DeviceAlpha;

                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    FlashLayer,
                    InAllottedGeometry.ToPaintGeometry(RegionSize, FSlateLayoutTransform{RegionPos}),
                    Brush,
                    ESlateDrawEffect::None,
                    FlashTint);
            }
        }

        if (Region.Label != nullptr && Scale >= 0.8f)
        {
            auto LabelTint = IsRebound ? CkStyle::Warn() : (IsMinted ? CkStyle::Text() : CkStyle::TextDim());
            LabelTint.A *= DeviceAlpha;

            const auto LabelSize = FontMeasure->Measure(Region.Label, Font);
            const auto LabelPos = FVector2f{
                RegionPos.X + (RegionSize.X - static_cast<float>(LabelSize.X)) * 0.5f,
                RegionPos.Y + (RegionSize.Y - static_cast<float>(LabelSize.Y)) * 0.5f};

            FSlateDrawElement::MakeText(
                OutDrawElements,
                LabelLayer,
                InAllottedGeometry.ToPaintGeometry(RegionSize, FSlateLayoutTransform{LabelPos}),
                Region.Label,
                Font,
                ESlateDrawEffect::None,
                LabelTint);
        }
    }

    return LabelLayer;
}

// --------------------------------------------------------------------------------------------------------------------
