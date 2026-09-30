#include "CkInspector_RotateTowards.h"

#include "CkRotateTowards/CkRotateTowards_Utils.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/World.h"
#include "Widgets/Input/SCheckBox.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_RotateTowards)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_inspector_rotate_towards
{
    auto
        Get_RotateTowards(const FCk_Handle& InEntity)
        -> FCk_Handle_RotateTowards
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_RotateTowards_UE::Has(InEntity))
        {
            return {};
        }
        auto MutableEntity = InEntity;
        return UCk_Utils_RotateTowards_UE::Cast(MutableEntity);
    }

    auto
        Format_Rotator(const FRotator& InRotator)
        -> FString
    {
        return ck::Format_UE(TEXT("P {:.1f} Y {:.1f} R {:.1f}"), InRotator.Pitch, InRotator.Yaw, InRotator.Roll);
    }

    auto
        Format_Axis(const FCk_RotateTowards_Axis& InAxis)
        -> FString
    {
        return InAxis.Get_Mode() == ECk_RotateTowards_AxisMode::Locked
            ? FString{TEXT("Locked")} : ck::Format_UE(TEXT("Free {}/s"), InAxis.Get_TurnRateDegPerSec());
    }

    auto
        Format_Ranges(const FCk_RotateTowards_RangeClamp& InRangeClamp)
        -> FString
    {
        auto Ranges = TArray<FString>{};
        const auto AddRange = [&Ranges](const FString& InAxisName, const FCk_RotateTowards_AxisRange& InRange)
        {
            if (InRange.Get_Enabled() == ECk_EnableDisable::Enable)
            {
                Ranges.Add(ck::Format_UE(TEXT("{} [{}, {}]"), InAxisName, InRange.Get_RangeDeg().Get_Min(), InRange.Get_RangeDeg().Get_Max()));
            }
        };
        AddRange(TEXT("P"), InRangeClamp.Get_Pitch());
        AddRange(TEXT("Y"), InRangeClamp.Get_Yaw());
        AddRange(TEXT("R"), InRangeClamp.Get_Roll());
        return FString::Join(Ranges, TEXT(" | "));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_RotateTowards::Get_ComponentName() const
    -> FText
{
    return FText::FromString(TEXT("Rotate Towards"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_RotateTowards::CanInspect(const FCk_Handle& InEntity) const
    -> bool
{
    return ck::IsValid(InEntity) && UCk_Utils_RotateTowards_UE::Has(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_RotateTowards::Build_NativeBody(const FCk_Handle& InEntity)
    -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    const auto RotateTowards = ck_inspector_rotate_towards::Get_RotateTowards(InEntity);
    if (ck::Is_NOT_Valid(RotateTowards))
    {
        return Builder.Build(InEntity, FString{});
    }
    Builder.AddWidgetRow(FText::FromString(TEXT("Target:")), SNew(SCkDebug_EntityRef).Entity_Lambda([InEntity]() -> FCk_Handle
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InEntity);
        return ck::IsValid(Current) ? FCk_Handle{UCk_Utils_RotateTowards_UE::Get_Target(Current)} : FCk_Handle{};
    }));
    Builder.AddStatusPillRow(FText::FromString(TEXT("Enabled:")), TAttribute<FText>::CreateLambda([InEntity]()
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InEntity);
        return FText::FromString(ck::IsValid(Current) && UCk_Utils_RotateTowards_UE::Get_IsEnabled(Current) ? TEXT("Enabled") : TEXT("Disabled"));
    }), TAttribute<ECk_Tone>::CreateLambda([InEntity]()
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InEntity);
        return ck::IsValid(Current) && UCk_Utils_RotateTowards_UE::Get_IsEnabled(Current) ? ECk_Tone::Accent : ECk_Tone::Neutral;
    }));
    Builder.AddStatusPillRow(FText::FromString(TEXT("State:")), TAttribute<FText>::CreateLambda([InEntity]()
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InEntity);
        if (ck::Is_NOT_Valid(Current) || NOT UCk_Utils_RotateTowards_UE::Get_HasTarget(Current))
        {
            return FText::FromString(TEXT("No target"));
        }
        return FText::FromString(UCk_Utils_RotateTowards_UE::Get_IsAtTarget(Current) ? TEXT("At target") : TEXT("Turning"));
    }), TAttribute<ECk_Tone>::CreateLambda([InEntity]()
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InEntity);
        if (ck::Is_NOT_Valid(Current) || NOT UCk_Utils_RotateTowards_UE::Get_HasTarget(Current))
        {
            return ECk_Tone::Neutral;
        }
        return UCk_Utils_RotateTowards_UE::Get_IsAtTarget(Current) ? ECk_Tone::Accent : ECk_Tone::Warn;
    }));
    Builder.AddRow(FText::FromString(TEXT("Mode:")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InCurrent);
        return FText::FromString(ck::IsValid(Current)
            ? ck::Format_UE(TEXT("{}"), UCk_Utils_RotateTowards_UE::Get_Tunables(Current).Get_Mode()) : TEXT("--"));
    }, CkStyle::Value_Enum());
    Builder.AddRow(FText::FromString(TEXT("Axes (P/Y/R):")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InCurrent);
        if (ck::Is_NOT_Valid(Current))
        {
            return FText::FromString(TEXT("--"));
        }
        const auto Tunables = UCk_Utils_RotateTowards_UE::Get_Tunables(Current);
        return FText::FromString(ck::Format_UE(TEXT("{} | {} | {}"),
            ck_inspector_rotate_towards::Format_Axis(Tunables.Get_Pitch()),
            ck_inspector_rotate_towards::Format_Axis(Tunables.Get_Yaw()),
            ck_inspector_rotate_towards::Format_Axis(Tunables.Get_Roll())));
    }, CkStyle::Value_Numeric());
    Builder.AddRow(FText::FromString(TEXT("Tolerance:")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InCurrent);
        return FText::FromString(ck::IsValid(Current)
            ? ck::Format_UE(TEXT("{:.2f} deg"), UCk_Utils_RotateTowards_UE::Get_Tunables(Current).Get_ReachedToleranceDeg()) : TEXT("--"));
    }, CkStyle::Value_Numeric());
    Builder.AddRow(FText::FromString(TEXT("Desired:")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InCurrent);
        return FText::FromString(ck::IsValid(Current)
            ? ck_inspector_rotate_towards::Format_Rotator(UCk_Utils_RotateTowards_UE::Get_DesiredRotation(Current)) : TEXT("--"));
    }, CkStyle::Value_Numeric());
    Builder.AddRow(FText::FromString(TEXT("Remaining:")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InCurrent);
        return FText::FromString(ck::IsValid(Current)
            ? ck_inspector_rotate_towards::Format_Rotator(UCk_Utils_RotateTowards_UE::Get_RemainingRotation(Current)) : TEXT("--"));
    }, CkStyle::Value_Numeric());
    const auto HasRangeClamp = UCk_Utils_RotateTowards_UE::Get_HasRangeClamp(RotateTowards);
    if (HasRangeClamp)
    {
        Builder.AddWidgetRow(FText::FromString(TEXT("Rest ref:")), SNew(SCkDebug_EntityRef).Entity_Lambda([InEntity]() -> FCk_Handle
        {
            const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InEntity);
            return ck::IsValid(Current) && UCk_Utils_RotateTowards_UE::Get_HasRangeClamp(Current)
                ? FCk_Handle{UCk_Utils_RotateTowards_UE::Get_RangeClamp(Current).Get_RestReferencePoint()} : FCk_Handle{};
        }));
        Builder.AddRow(FText::FromString(TEXT("Ranges:")), [](const FCk_Handle& InCurrent)
        {
            const auto Current = ck_inspector_rotate_towards::Get_RotateTowards(InCurrent);
            return FText::FromString(ck::IsValid(Current) && UCk_Utils_RotateTowards_UE::Get_HasRangeClamp(Current)
                ? ck_inspector_rotate_towards::Format_Ranges(UCk_Utils_RotateTowards_UE::Get_RangeClamp(Current)) : TEXT("--"));
        }, CkStyle::Value_Numeric());
    }
    else
    {
        Builder.AddRow(FText::FromString(TEXT("Range clamp:")), [](const FCk_Handle&)
        {
            return FText::FromString(TEXT("None"));
        });
    }
    const auto Draw = _Draw;
    Builder.AddWidgetRow(FText::FromString(TEXT("Draw:")), SNew(SCheckBox)
        .IsChecked_Lambda([Draw]() { return *Draw ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([Draw](ECheckBoxState InState) { *Draw = InState == ECheckBoxState::Checked; }));
    if (NOT FCkInspector_RowCaptureScope::Is_Active())
    {
        _EntityId = static_cast<uint32>(InEntity.Get_Entity().Get_ID());
        _HadRangeClamp = HasRangeClamp;
    }
    return Builder.Build(InEntity, FString{});
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_RotateTowards::Tick(const FCk_Handle& InEntity, float InDeltaTime)
    -> void
{
    const auto RotateTowards = ck_inspector_rotate_towards::Get_RotateTowards(InEntity);
    if (ck::Is_NOT_Valid(RotateTowards))
    {
        return;
    }
    const auto EntityId = static_cast<uint32>(InEntity.Get_Entity().Get_ID());
    const auto HasRangeClamp = UCk_Utils_RotateTowards_UE::Get_HasRangeClamp(RotateTowards);
    if (NOT _EntityId.IsSet() || _EntityId.GetValue() != EntityId || HasRangeClamp != _HadRangeClamp)
    {
        _EntityId = EntityId;
        _HadRangeClamp = HasRangeClamp;
        RequestRebuild();
    }
    if (NOT *_Draw)
    {
        return;
    }
    auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InEntity);
    if (ck::Is_NOT_Valid(World))
    {
        return;
    }
    auto MutableEntity = InEntity;
    const auto Self = UCk_Utils_Transform_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(Self))
    {
        return;
    }
    const auto FeatureColor = Get_FeatureColor().GetValue();
    const auto Location = UCk_Utils_Transform_UE::Get_EntityCurrentLocation(Self);
    const auto Forward = UCk_Utils_Transform_UE::Get_EntityCurrentRotation(Self).Vector();
    UCk_Utils_DebugDraw_UE::DrawDebugArrow(World, Location, Location + Forward * 60.0, 10.0f, FeatureColor, 0.0f, 2.0f);

    const auto Target = UCk_Utils_RotateTowards_UE::Get_Target(RotateTowards);
    if (UCk_Utils_RotateTowards_UE::Get_HasTarget(RotateTowards) && ck::IsValid(Target))
    {
        UCk_Utils_DebugDraw_UE::DrawDebugLine(World, Location, UCk_Utils_Transform_UE::Get_EntityCurrentLocation(Target), FeatureColor, 0.0f, 1.0f);
        UCk_Utils_DebugDraw_UE::DrawDebugArrow(World, Location,
            Location + UCk_Utils_RotateTowards_UE::Get_DesiredRotation(RotateTowards).Vector() * 60.0, 10.0f, FLinearColor::White, 0.0f, 1.0f);
    }
    if (NOT HasRangeClamp)
    {
        return;
    }
    const auto RangeClamp = UCk_Utils_RotateTowards_UE::Get_RangeClamp(RotateTowards);
    const auto& RestReferencePoint = RangeClamp.Get_RestReferencePoint();
    if (ck::Is_NOT_Valid(RestReferencePoint))
    {
        return;
    }
    const auto RestDirection = (UCk_Utils_Transform_UE::Get_EntityCurrentLocation(RestReferencePoint) - Location).GetSafeNormal();
    if (RestDirection.IsNearlyZero())
    {
        return;
    }
    UCk_Utils_DebugDraw_UE::DrawDebugLine(World, Location, Location + RestDirection * 80.0, FLinearColor::Blue, 0.0f, 1.0f);
    const auto& YawRange = RangeClamp.Get_Yaw();
    if (YawRange.Get_Enabled() != ECk_EnableDisable::Enable)
    {
        return;
    }
    const auto RestYaw = RestDirection.Rotation().Yaw;
    const auto MinEdge = FRotator{0.0, RestYaw + YawRange.Get_RangeDeg().Get_Min(), 0.0}.Vector();
    const auto MaxEdge = FRotator{0.0, RestYaw + YawRange.Get_RangeDeg().Get_Max(), 0.0}.Vector();
    UCk_Utils_DebugDraw_UE::DrawDebugLine(World, Location, Location + MinEdge * 80.0, FLinearColor{0.0f, 1.0f, 1.0f}, 0.0f, 1.0f);
    UCk_Utils_DebugDraw_UE::DrawDebugLine(World, Location, Location + MaxEdge * 80.0, FLinearColor{0.0f, 1.0f, 1.0f}, 0.0f, 1.0f);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_RotateTowards::OnDeactivated()
    -> void
{
    *_Draw = false;
    _Draw = MakeShared<bool>(false);
    _EntityId.Reset();
    _HadRangeClamp = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_RotateTowards::Build_Inspector(const FCk_Handle& InEntity)
    -> TSharedRef<SWidget>
{
    return Build_NativeBody(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------
