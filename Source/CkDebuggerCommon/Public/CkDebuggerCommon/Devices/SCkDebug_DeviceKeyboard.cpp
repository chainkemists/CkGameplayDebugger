#include "CkDebuggerCommon/Devices/SCkDebug_DeviceKeyboard.h"

#include "CkDebuggerCommon/Devices/CkDebug_KeyboardLayout.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_device_keyboard
{
    // Nominal pixels per key unit at desired size; paint re-derives the real unit from the allotted geometry.
    constexpr auto UnitPx = 30.0f;

    // The gap between caps, as a fraction of a unit — the physical board's bezel line.
    constexpr auto CapGapFraction = 0.08f;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceKeyboard::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Snapshot = InArgs._Snapshot;
    SetCanTick(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceKeyboard::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    const auto Extent = ck::debug_devices::Get_KeyboardLayout_ExtentUnits();
    return Extent * ck_debug_device_keyboard::UnitPx;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceKeyboard::
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
    const auto& Layout = ck::debug_devices::Get_KeyboardLayout_AnsiFull();
    const auto Extent = ck::debug_devices::Get_KeyboardLayout_ExtentUnits();

    const auto LocalSize = InAllottedGeometry.GetLocalSize();
    const auto Unit = static_cast<float>(FMath::Min(
        LocalSize.X / Extent.X,
        LocalSize.Y / Extent.Y));

    if (Unit <= 1.0f)
    { return InLayerId; }

    const auto Gap = Unit * ck_debug_device_keyboard::CapGapFraction;

    const auto Connected = Snapshot != nullptr && Snapshot->DeviceConnected;
    const auto BoardAlpha = Connected ? 1.0f : ck::debug_devices::DisconnectedAlpha;

    const auto* CapBrush = FCkDebuggerStyle::Get().GetBrush(TEXT("CkDebugger.Badge.Rounded"));
    const auto* FillBrush = FAppStyle::GetBrush("WhiteBrush");

    const auto CapFont = CkStyle::RegularFont(CkStyle::FontSizeMicro());
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    const auto RimLayer = InLayerId;
    const auto BaseLayer = InLayerId + 1;
    const auto FillLayer = InLayerId + 2;
    const auto FlashLayer = InLayerId + 3;
    const auto LabelLayer = InLayerId + 4;

    for (const auto& Def : Layout)
    {
        const auto CapPos = FVector2f{Def.X * Unit, Def.Y * Unit};
        const auto CapSize = FVector2f{Def.W * Unit - Gap, Def.H * Unit - Gap};

        const auto* State = Snapshot != nullptr ? Snapshot->TryGet_Key(Def.Key) : nullptr;
        const auto IsLightable = Def.Key.IsValid();
        const auto IsMinted = State != nullptr && State->IsMinted;
        const auto IsActionable = State != nullptr && State->IsActionable;

        if (IsActionable)
        {
            // A rounded rim: the same badge brush drawn one step larger UNDER the cap, so the outline keeps the
            // cap's corner radius without a dedicated border brush.
            const auto RimPx = FMath::Max(1.0f, Gap * 0.5f);
            auto RimTint = CkStyle::Accent();
            RimTint.A *= 0.9f * BoardAlpha;

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                RimLayer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{CapSize.X + RimPx * 2.0f, CapSize.Y + RimPx * 2.0f},
                    FSlateLayoutTransform{FVector2f{CapPos.X - RimPx, CapPos.Y - RimPx}}),
                CapBrush,
                ESlateDrawEffect::None,
                RimTint);
        }

        auto BaseTint = IsMinted ? CkStyle::Bg3() : CkStyle::Bg2();
        BaseTint.A *= IsLightable ? BoardAlpha : BoardAlpha * 0.6f;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            BaseLayer,
            InAllottedGeometry.ToPaintGeometry(CapSize, FSlateLayoutTransform{CapPos}),
            CapBrush,
            ESlateDrawEffect::None,
            BaseTint);

        if (State != nullptr)
        {
            const auto FillFraction = ck::debug_devices::Get_FillFraction(*State);

            if (FillFraction > 0.0f)
            {
                // Bottom-anchored: the cap fills upward toward the verdict, and a full cap IS the hold.
                const auto Inset = Gap * 0.75f;
                const auto FillHeight = (CapSize.Y - Inset * 2.0f) * FillFraction;
                const auto FillPos = FVector2f{
                    CapPos.X + Inset,
                    CapPos.Y + CapSize.Y - Inset - FillHeight};

                auto FillTint = FillFraction >= 1.0f ? CkStyle::Accent() : CkStyle::AccentDim();
                FillTint.A *= 0.85f * BoardAlpha;

                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    FillLayer,
                    InAllottedGeometry.ToPaintGeometry(
                        FVector2f{CapSize.X - Inset * 2.0f, FillHeight},
                        FSlateLayoutTransform{FillPos}),
                    FillBrush,
                    ESlateDrawEffect::None,
                    FillTint);
            }

            const auto FlashAlpha = ck::debug_devices::Get_FlashAlpha(*State, Snapshot->LiveFrame);

            if (FlashAlpha > 0.0f)
            {
                auto FlashTint = CkStyle::Ok();
                FlashTint.A = FlashAlpha * BoardAlpha;

                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    FlashLayer,
                    InAllottedGeometry.ToPaintGeometry(CapSize, FSlateLayoutTransform{CapPos}),
                    CapBrush,
                    ESlateDrawEffect::None,
                    FlashTint);
            }
        }

        if (Unit >= 14.0f)
        {
            auto LabelTint = IsMinted ? CkStyle::Text() : (IsLightable ? CkStyle::TextDim() : CkStyle::TextMute());
            LabelTint.A *= BoardAlpha;

            const auto LabelSize = FontMeasure->Measure(Def.Label, CapFont);
            const auto LabelPos = FVector2f{
                CapPos.X + FMath::Max(Gap, (CapSize.X - static_cast<float>(LabelSize.X)) * 0.5f),
                CapPos.Y + (CapSize.Y - static_cast<float>(LabelSize.Y)) * 0.5f};

            FSlateDrawElement::MakeText(
                OutDrawElements,
                LabelLayer,
                InAllottedGeometry.ToPaintGeometry(CapSize, FSlateLayoutTransform{LabelPos}),
                Def.Label,
                CapFont,
                ESlateDrawEffect::None,
                LabelTint);
        }
    }

    return LabelLayer;
}

// --------------------------------------------------------------------------------------------------------------------
