#include "CkInspector_Aggro.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAggro/CkAggroOwner_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Aggro)

// =====================================================================================================================

auto FCkInspector_Aggro::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Aggro"));
}

auto FCkInspector_Aggro::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Aggro_Current,
        ck::FFragment_AggroOwner_Current>();
}

// =====================================================================================================================

auto FCkInspector_Aggro::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Aggro entry (this entity is a threat tracked by an owner) ----
    if (Entity.Has<ck::FFragment_Aggro_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Aggro Entry")));

        const auto CapturedEntity = Entity;
        Builder.AddRow(
            FText::FromString(TEXT("Score:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Aggro_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Score = CapturedEntity.Get<ck::FFragment_Aggro_Current>().Get_Score();
                return FText::FromString(FString::Printf(TEXT("%.3f"), Score));
            },
            CkStyle::Value_Numeric());
    }

    // ---- Aggro owner (this entity holds the threat table) ----
    if (Entity.Has<ck::FFragment_AggroOwner_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Aggro Owner")));

        const auto CapturedEntity = Entity;
        Builder.AddRow(
            FText::FromString(TEXT("Best Aggro:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AggroOwner_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Best = CapturedEntity.Get<ck::FFragment_AggroOwner_Current>().Get_BestAggro();
                return FText::FromString(ck::IsValid(Best)
                    ? ck::Format_UE(TEXT("[{}]"), Best)
                    : FString(TEXT("(None)")));
            },
            CkStyle::Value_Handle());

        if (Entity.Has<ck::FFragment_AggroOwner_NewBestAggro>())
        {
            Builder.AddConditionalRow(
                FText::FromString(TEXT("New Best Aggro:")),
                [CapturedEntity](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AggroOwner_NewBestAggro>())
                    { return FText::FromString(TEXT("--")); }
                    const auto Best = CapturedEntity.Get<ck::FFragment_AggroOwner_NewBestAggro>().Get_BestAggro();
                    return FText::FromString(ck::IsValid(Best)
                        ? ck::Format_UE(TEXT("[{}]"), Best)
                        : FString(TEXT("(None)")));
                },
                [CapturedEntity](const FCk_Handle&) -> FLinearColor
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AggroOwner_NewBestAggro>())
                    { return CkStyle::None(); }
                    const auto Best = CapturedEntity.Get<ck::FFragment_AggroOwner_NewBestAggro>().Get_BestAggro();
                    return ck::IsValid(Best) ? CkStyle::Status_Active() : CkStyle::TextMute();
                });
        }
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Aggro::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
