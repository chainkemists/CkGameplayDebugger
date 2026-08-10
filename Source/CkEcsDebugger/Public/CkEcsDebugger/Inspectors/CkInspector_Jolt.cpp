#include "CkInspector_Jolt.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

// Jolt feature Utils expose the clean accessor surface; the JoltBody fragment (pulled transitively
// via the Utils header) is read directly for the raw JPH::BodyID, mirroring CkInspector_Physics's
// direct-fragment reads.
#include "CkJolt/Body/CkJoltBody_Utils.h"
#include "CkJolt/Body/CkJoltBody_Fragment.h"
#include "CkJolt/Character/CkJoltCharacter_Utils.h"
#include "CkJolt/StaticWorld/CkJoltStaticActor_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Jolt)

// =====================================================================================================================

namespace ck_inspector_jolt
{
    // Motion type is a simulation CLASS, not a health state: Static is the inert default, Kinematic
    // is externally driven, Dynamic is the one the solver actually integrates.
    static auto Get_MotionTone(ECk_MotionType InMotionType) -> ECk_Tone
    {
        switch (InMotionType)
        {
            case ECk_MotionType::Kinematic: return ECk_Tone::Info;
            case ECk_MotionType::Dynamic:   return ECk_Tone::Accent;
            default:                        return ECk_Tone::Neutral;
        }
    }

    static auto Get_SleepTone(ECk_Jolt_SleepState InSleepState) -> ECk_Tone
    {
        return InSleepState == ECk_Jolt_SleepState::Awake ? ECk_Tone::Ok : ECk_Tone::Neutral;
    }

    // NotSupported means the character will fall through what it is touching — an error, not a warning.
    static auto Get_GroundTone(ECk_JoltCharacter_GroundState InGroundState) -> ECk_Tone
    {
        switch (InGroundState)
        {
            case ECk_JoltCharacter_GroundState::OnGround:      return ECk_Tone::Ok;
            case ECk_JoltCharacter_GroundState::OnSteepSlope:  return ECk_Tone::Warn;
            case ECk_JoltCharacter_GroundState::InAir:         return ECk_Tone::Info;
            case ECk_JoltCharacter_GroundState::NotSupported:  return ECk_Tone::Err;
            default:                                           return ECk_Tone::Neutral;
        }
    }

    // Three fixed-precision components in X/Y/Z order — same 3 decimals FVector::ToString printed —
    // so the row's axis coloring lines up with the Transform inspector.
    static auto Make_AxisComponents(
        TFunction<FVector()> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InProjector, Axis]()
            {
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), InProjector()[Axis]));
            }));
        }

        return Components;
    }
}

// =====================================================================================================================

auto FCkInspector_Jolt::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Jolt"));
}

auto FCkInspector_Jolt::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return UCk_Utils_JoltBody_UE::Has(Entity)
        || UCk_Utils_JoltCharacter_UE::Has(Entity)
        || UCk_Utils_JoltStaticActor_UE::Has(Entity);
}

// =====================================================================================================================

auto FCkInspector_Jolt::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- JoltBody ----
    if (UCk_Utils_JoltBody_UE::Has(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Jolt Body")));

        auto        MutableEntity = Entity;
        const auto  CapturedBody  = UCk_Utils_JoltBody_UE::CastChecked(MutableEntity);
        const auto  CapturedEntity = Entity;

        Builder.AddRow(
            FText::FromString(TEXT("Body Id:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_JoltBody_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Raw = CapturedEntity.Get<ck::FFragment_JoltBody_Current>().Get_BodyId().GetIndexAndSequenceNumber();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Raw));
            },
            CkStyle::Value_Numeric());

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Motion Type:")),
            TAttribute<FText>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltBody_UE::Get_MotionType(CapturedBody)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return ECk_Tone::Neutral; }
                return ck_inspector_jolt::Get_MotionTone(UCk_Utils_JoltBody_UE::Get_MotionType(CapturedBody));
            }));

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Sleep State:")),
            TAttribute<FText>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltBody_UE::Get_SleepState(CapturedBody)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return ECk_Tone::Neutral; }
                return ck_inspector_jolt::Get_SleepTone(UCk_Utils_JoltBody_UE::Get_SleepState(CapturedBody));
            }));

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Body Added:")),
            TAttribute<FText>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(UCk_Utils_JoltBody_UE::Get_IsBodyAdded(CapturedBody) ? TEXT("Yes") : TEXT("No"));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return ECk_Tone::Neutral; }
                // A body that never made it into the Jolt world simulates nothing — that is a defect, not a mode.
                return UCk_Utils_JoltBody_UE::Get_IsBodyAdded(CapturedBody) ? ECk_Tone::Ok : ECk_Tone::Warn;
            }));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Linear Velocity:")),
            ck_inspector_jolt::Make_AxisComponents([CapturedBody]() -> FVector
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FVector::ZeroVector; }
                return UCk_Utils_JoltBody_UE::Get_LinearVelocity(CapturedBody);
            }));

        Builder.AddSparklineRow(
            FText::FromString(TEXT("Linear Speed:")),
            TAttribute<float>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return 0.0f; }
                return static_cast<float>(UCk_Utils_JoltBody_UE::Get_LinearVelocity(CapturedBody).Size());
            }),
            ECk_Tone::Accent,
            TAttribute<FText>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{:.2f}"), UCk_Utils_JoltBody_UE::Get_LinearVelocity(CapturedBody).Size()));
            }));
    }

    // ---- JoltCharacter ----
    if (UCk_Utils_JoltCharacter_UE::Has(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Jolt Character")));

        auto       MutableEntity     = Entity;
        const auto CapturedCharacter = UCk_Utils_JoltCharacter_UE::CastChecked(MutableEntity);

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Ground State:")),
            TAttribute<FText>::CreateLambda([CapturedCharacter]()
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltCharacter_UE::Get_GroundState(CapturedCharacter)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedCharacter]()
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return ECk_Tone::Neutral; }
                return ck_inspector_jolt::Get_GroundTone(UCk_Utils_JoltCharacter_UE::Get_GroundState(CapturedCharacter));
            }));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Ground Normal:")),
            ck_inspector_jolt::Make_AxisComponents([CapturedCharacter]() -> FVector
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FVector::ZeroVector; }
                return UCk_Utils_JoltCharacter_UE::Get_GroundNormal(CapturedCharacter);
            }));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Ground Velocity:")),
            ck_inspector_jolt::Make_AxisComponents([CapturedCharacter]() -> FVector
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FVector::ZeroVector; }
                return UCk_Utils_JoltCharacter_UE::Get_GroundVelocity(CapturedCharacter);
            }));
    }

    // ---- JoltStaticActor ----
    if (UCk_Utils_JoltStaticActor_UE::Has(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Jolt Static Actor")));

        auto       MutableEntity       = Entity;
        const auto CapturedStaticActor = UCk_Utils_JoltStaticActor_UE::CastChecked(MutableEntity);

        Builder.AddRow(
            FText::FromString(TEXT("Source Actor:")),
            [CapturedStaticActor](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedStaticActor)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(UCk_Utils_JoltStaticActor_UE::Get_SourceActorName(CapturedStaticActor).ToString());
            },
            CkStyle::Value_Object());

        Builder.AddCountBadgeRow(
            FText::FromString(TEXT("Num Bodies:")),
            TAttribute<int32>::CreateLambda([CapturedStaticActor]()
            {
                if (ck::Is_NOT_Valid(CapturedStaticActor)) { return 0; }
                return UCk_Utils_JoltStaticActor_UE::Get_NumBodies(CapturedStaticActor);
            }),
            ECk_Tone::Info);
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Jolt::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
