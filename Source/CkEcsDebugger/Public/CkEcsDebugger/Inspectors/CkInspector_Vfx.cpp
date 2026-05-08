#include "CkInspector_Vfx.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkVfx/Cue/CkVfxCue_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Vfx)

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
            { return CkDebugStyle::None(); }
            const auto* Component = CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_NiagaraComponent().Get();
            return ck::IsValid(Component, ck::IsValid_Policy_NullptrOnly{})
                ? CkDebugStyle::Value_Bool_True()
                : CkDebugStyle::Value_Bool_False();
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
        CkDebugStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Duration:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Duration = CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_EffectDuration();
            return FText::FromString(ck::Format_UE(TEXT("{}"), Duration));
        },
        CkDebugStyle::Value_Numeric());

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Finished:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Finished = CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_HasFiredFinished();
            return FText::FromString(Finished ? TEXT("Yes") : TEXT("No"));
        },
        [CapturedEntity](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_VfxCue_Current>())
            { return CkDebugStyle::None(); }
            const auto Finished = CapturedEntity.Get<ck::FFragment_VfxCue_Current>().Get_HasFiredFinished();
            return Finished ? CkDebugStyle::Value_Bool_True() : CkDebugStyle::Value_Bool_False();
        });

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Vfx::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
