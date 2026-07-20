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
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Jolt)

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

        Builder.AddRow(
            FText::FromString(TEXT("Motion Type:")),
            [CapturedBody](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltBody_UE::Get_MotionType(CapturedBody)));
            },
            CkStyle::Value_Tag());

        Builder.AddRow(
            FText::FromString(TEXT("Sleep State:")),
            [CapturedBody](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltBody_UE::Get_SleepState(CapturedBody)));
            },
            CkStyle::Value_Tag());

        Builder.AddRow(
            FText::FromString(TEXT("Body Added:")),
            [CapturedBody](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(UCk_Utils_JoltBody_UE::Get_IsBodyAdded(CapturedBody) ? TEXT("Yes") : TEXT("No"));
            },
            CkStyle::Value_Tag());

        Builder.AddRow(
            FText::FromString(TEXT("Linear Velocity:")),
            [CapturedBody](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(UCk_Utils_JoltBody_UE::Get_LinearVelocity(CapturedBody).ToString());
            },
            CkStyle::Value_Math());
    }

    // ---- JoltCharacter ----
    if (UCk_Utils_JoltCharacter_UE::Has(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Jolt Character")));

        auto       MutableEntity     = Entity;
        const auto CapturedCharacter = UCk_Utils_JoltCharacter_UE::CastChecked(MutableEntity);

        Builder.AddRow(
            FText::FromString(TEXT("Ground State:")),
            [CapturedCharacter](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltCharacter_UE::Get_GroundState(CapturedCharacter)));
            },
            CkStyle::Value_Tag());

        Builder.AddRow(
            FText::FromString(TEXT("Ground Normal:")),
            [CapturedCharacter](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(UCk_Utils_JoltCharacter_UE::Get_GroundNormal(CapturedCharacter).ToString());
            },
            CkStyle::Value_Math());

        Builder.AddRow(
            FText::FromString(TEXT("Ground Velocity:")),
            [CapturedCharacter](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(UCk_Utils_JoltCharacter_UE::Get_GroundVelocity(CapturedCharacter).ToString());
            },
            CkStyle::Value_Math());
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

        Builder.AddRow(
            FText::FromString(TEXT("Num Bodies:")),
            [CapturedStaticActor](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedStaticActor)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltStaticActor_UE::Get_NumBodies(CapturedStaticActor)));
            },
            CkStyle::Value_Numeric());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Jolt::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
