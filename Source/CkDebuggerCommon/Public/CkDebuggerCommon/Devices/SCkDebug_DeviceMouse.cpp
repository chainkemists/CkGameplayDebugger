#include "CkDebuggerCommon/Devices/SCkDebug_DeviceMouse.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_device_mouse
{
    // The design box the regions below are authored in; paint scales it to the allotted geometry.
    constexpr auto DesignWidth = 100.0f;
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

    // Authored once — the mouse's own layout table, same doctrine as the keyboard's.
    inline auto Get_Regions() -> const TArray<FRegion>&
    {
        static const auto Regions = TArray<FRegion>{
            {EKeys::LeftMouseButton, TEXT("L"), 8.0f, 8.0f, 36.0f, 52.0f},
            {EKeys::RightMouseButton, TEXT("R"), 56.0f, 8.0f, 36.0f, 52.0f},
            {EKeys::MouseScrollUp, nullptr, 44.5f, 9.0f, 11.0f, 8.0f},
            {EKeys::MiddleMouseButton, nullptr, 44.5f, 19.0f, 11.0f, 26.0f},
            {EKeys::MouseScrollDown, nullptr, 44.5f, 47.0f, 11.0f, 8.0f},
            {EKeys::ThumbMouseButton, TEXT("4"), 1.0f, 74.0f, 9.0f, 24.0f},
            {EKeys::ThumbMouseButton2, TEXT("5"), 1.0f, 102.0f, 9.0f, 18.0f},
        };

        return Regions;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceMouse::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Snapshot = InArgs._Snapshot;
    SetCanTick(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceMouse::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D{
        ck_debug_device_mouse::DesignWidth * ck_debug_device_mouse::DesiredScale,
        ck_debug_device_mouse::DesignHeight * ck_debug_device_mouse::DesiredScale};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceMouse::
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
        LocalSize.X / ck_debug_device_mouse::DesignWidth,
        LocalSize.Y / ck_debug_device_mouse::DesignHeight));

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

    // The body shell behind the regions — not a key, just the silhouette that makes it read as a mouse.
    {
        auto BodyTint = CkStyle::Bg1();
        BodyTint.A *= DeviceAlpha;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            BaseLayer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{92.0f * Scale, 140.0f * Scale},
                FSlateLayoutTransform{FVector2f{4.0f * Scale, 5.0f * Scale}}),
            Brush,
            ESlateDrawEffect::None,
            BodyTint);
    }

    for (const auto& Region : ck_debug_device_mouse::Get_Regions())
    {
        const auto RegionPos = FVector2f{Region.X * Scale, Region.Y * Scale};
        const auto RegionSize = FVector2f{Region.W * Scale, Region.H * Scale};

        const auto* State = Snapshot != nullptr ? Snapshot->TryGet_Key(Region.Key) : nullptr;
        const auto IsMinted = State != nullptr && State->IsMinted;
        const auto IsActionable = State != nullptr && State->IsActionable;

        if (IsActionable)
        {
            const auto RimPx = FMath::Max(1.0f, 1.2f * Scale);
            auto RimTint = CkStyle::Accent();
            RimTint.A *= 0.9f * DeviceAlpha;

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
            auto LabelTint = IsMinted ? CkStyle::Text() : CkStyle::TextDim();
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
