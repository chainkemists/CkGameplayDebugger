#include "CkInspector_Vfx.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Time/CkTime_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkVfx/Cue/CkVfxCue_Fragment.h"
#include "CkVfx/Cue/CkVfxCue_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Vfx)

// =====================================================================================================================

namespace ck_inspector_vfx
{
    // Seconds since the cue was activated, read the same way the lifetime-monitor processor does
    // (world time minus the recorded start). Returns 0 when the entity or its world is gone.
    static auto Get_ElapsedSeconds(
        const FCk_Handle& InEntity)
        -> float
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_VfxCue_Current>())
        { return 0.0f; }

        const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InEntity);
        if (ck::Is_NOT_Valid(World))
        { return 0.0f; }

        const auto TimeResult = UCk_Utils_Time_UE::Get_WorldTime(FCk_Utils_Time_GetWorldTime_Params{World});
        const auto StartTime  = InEntity.Get<ck::FFragment_VfxCue_Current>().Get_EffectStartTime();

        return FMath::Max(0.0f,
            static_cast<float>((TimeResult.Get_WorldTime().Get_Time() - StartTime).Get_Seconds()));
    }

    static auto Get_DurationSeconds(
        const FCk_Handle& InEntity)
        -> float
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_VfxCue_Current>())
        { return 0.0f; }

        return static_cast<float>(InEntity.Get<ck::FFragment_VfxCue_Current>().Get_EffectDuration().Get_Seconds());
    }
}

// =====================================================================================================================

auto FCkInspector_Vfx::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("VFX"));
}

auto FCkInspector_Vfx::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && Entity.Has<ck::FFragment_VfxCue_Current>();
}

// =====================================================================================================================

auto FCkInspector_Vfx::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    if (NOT Entity.Has<ck::FFragment_VfxCue_Current>())
    { return Builder.Build(Entity); }

    Builder.AddHeader(FText::FromString(TEXT("VFX Cue")));

    const auto CapturedEntity = Entity;

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Component:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto* Component = CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_NiagaraComponent().Get();
            return FText::FromString(ck::IsValid(Component, ck::IsValid_Policy_NullptrOnly{})
                ? TEXT("Valid")
                : TEXT("None"));
        },
        [CapturedEntity](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
            { return CkStyle::None(); }
            const auto* Component = CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_NiagaraComponent().Get();
            return ck::IsValid(Component, ck::IsValid_Policy_NullptrOnly{})
                ? CkStyle::Value_Bool_True()
                : CkStyle::Value_Bool_False();
        });

    Builder.AddRow(
        FText::FromString(TEXT("Start Time:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto StartTime = CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_EffectStartTime();
            return FText::FromString(ck::Format_UE(TEXT("{}"), StartTime));
        },
        CkStyle::Value_Numeric());

    // A negative duration is the Infinite mode — there is no bounded quantity to meter. The mode is
    // fixed at setup, so choosing the row shape once at compose time is safe.
    const auto HasFiniteDuration = ck_inspector_vfx::Get_DurationSeconds(Entity) > 0.0f;

    if (HasFiniteDuration)
    {
        Builder.AddMeterRow(
            FText::FromString(TEXT("Elapsed / Duration:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            {
                const auto Duration = ck_inspector_vfx::Get_DurationSeconds(CapturedEntity);
                if (Duration <= 0.0f) { return 0.0f; }
                return ck_inspector_vfx::Get_ElapsedSeconds(CapturedEntity) / Duration;
            }),
            ECk_Tone::Accent,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{:.2f} / {:.2f}s"),
                    ck_inspector_vfx::Get_ElapsedSeconds(CapturedEntity),
                    ck_inspector_vfx::Get_DurationSeconds(CapturedEntity)));
            }));
    }
    else
    {
        Builder.AddRow(
            FText::FromString(TEXT("Duration:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Duration = CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_EffectDuration();
                return FText::FromString(Duration.Get_Seconds() < 0.0
                    ? FString(TEXT("Infinite"))
                    : ck::Format_UE(TEXT("{}"), Duration));
            },
            CkStyle::Value_Numeric());
    }

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
            { return FText::FromString(TEXT("--")); }
            if (CapturedEntity.Has<ck::FTag_VfxCue_IsPlaying>())
            { return FText::FromString(TEXT("Playing")); }
            return FText::FromString(CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_HasFiredFinished()
                ? TEXT("Finished")
                : TEXT("Idle"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
            { return ECk_Tone::Neutral; }
            if (CapturedEntity.Has<ck::FTag_VfxCue_IsPlaying>())
            { return ECk_Tone::Ok; }
            return CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_HasFiredFinished()
                ? ECk_Tone::Info
                : ECk_Tone::Neutral;
        }));

    // ---- Controls ----
    //
    // CosmeticOnly: a Niagara cue has nothing to spawn on a dedicated server, so both verbs grey out
    // there with the reason in their tooltip rather than enqueueing a request nobody will ever see.
    // Public Utils only, typed handle captured BY VALUE and re-validated on fire; the State pill above
    // stays the display, one frame behind the request as usual.
    auto MutableEntity = Entity;

    if (const auto CapturedCue = UCk_Utils_VfxCue_UE::Cast(MutableEntity);
        ck::IsValid(CapturedCue))
    {
        Builder.AddActionRow(
            FText::FromString(TEXT("Playback:")),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Play")),
                    FText::FromString(TEXT("Request_Play — (re)activates the cue's Niagara component")),
                    [CapturedCue]
                    {
                        auto Cue = CapturedCue;
                        if (ck::Is_NOT_Valid(Cue))
                        { return; }

                        UCk_Utils_VfxCue_UE::Request_Play(Cue, FCk_Request_VfxCue_Play{}, {});
                    },
                    ECk_DebugRequest_Requirement::CosmeticOnly
                },
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Stop")),
                    FText::FromString(TEXT("Request_Stop — deactivates the cue")),
                    [CapturedCue]
                    {
                        auto Cue = CapturedCue;
                        if (ck::Is_NOT_Valid(Cue))
                        { return; }

                        UCk_Utils_VfxCue_UE::Request_Stop(Cue, {});
                    },
                    ECk_DebugRequest_Requirement::CosmeticOnly
                }
            });
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Vfx::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
