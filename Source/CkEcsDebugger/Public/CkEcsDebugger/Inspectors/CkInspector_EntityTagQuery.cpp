#include "CkInspector_EntityTagQuery.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkEntityTag/Query/CkEntityTagQuery_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityTagQuery)

static const FLinearColor Color_OK    = FLinearColor(0.6f, 0.85f, 0.55f);
static const FLinearColor Color_Field = FLinearColor(0.5f, 0.7f, 0.45f);

// =====================================================================================================================

auto FCkInspector_EntityTagQuery::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Entity Tag Query"));
}

auto FCkInspector_EntityTagQuery::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return UCk_Utils_EntityTagQuery_UE::Has(Entity);
}

auto FCkInspector_EntityTagQuery::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildGrid(Entity);
}

auto FCkInspector_EntityTagQuery::Build_Inspector(const FCk_Handle& Entity, const FString& /*InFilter*/) -> TSharedRef<SWidget>
{
    return BuildGrid(Entity);
}

auto FCkInspector_EntityTagQuery::Tick(const FCk_Handle& /*Entity*/, float /*InDeltaTime*/) -> void
{
}

// =====================================================================================================================

auto FCkInspector_EntityTagQuery::BuildGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    auto Mutable = Entity;
    auto Query   = UCk_Utils_EntityTagQuery_UE::Cast(Mutable);

    if (ck::Is_NOT_Valid(Query))
    { return Builder.Build(Entity, FString()); }

    // Is Satisfied row
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Is Satisfied:")),
        [](const FCk_Handle& E)
        {
            auto Me = E;
            auto Q  = UCk_Utils_EntityTagQuery_UE::Cast(Me);
            return FText::FromString(UCk_Utils_EntityTagQuery_UE::Get_IsSatisfied(Q) ? TEXT("YES") : TEXT("no"));
        },
        [](const FCk_Handle& E)
        {
            auto Me = E;
            auto Q  = UCk_Utils_EntityTagQuery_UE::Cast(Me);
            return UCk_Utils_EntityTagQuery_UE::Get_IsSatisfied(Q) ? Color_OK : CkDebugStyle::Err();
        });

    const auto Reqs    = UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(Query);
    const auto Results = UCk_Utils_EntityTagQuery_UE::Get_CurrentResults(Query);

    Builder.AddRow(
        FText::FromString(FString::Printf(TEXT("Requirements (%d):"), Reqs.Num())),
        [N = Reqs.Num()](const FCk_Handle&) { return FText::FromString(FString::FromInt(N)); },
        Color_Field);

    for (int32 i = 0; i < Reqs.Num(); ++i)
    {
        const auto& R   = Reqs[i];
        const auto  Tag = R.Get_Tag();
        const auto  N   = (i < Results.Num()) ? Results[i].Get_Handles().Num() : 0;

        const auto ThresholdStr =
            R.Get_Mode() == ECk_EntityTagQuery_CountMode::Count
                ? *FString::FromInt(R.Get_Count())
                : (R.Get_Mode() == ECk_EntityTagQuery_CountMode::All ? TEXT("∞") : TEXT("1"));

        const auto Header = FString::Printf(TEXT("  %s [%s, %d/%s]"),
            *Tag.ToString(),
            *UEnum::GetValueAsString(R.Get_Mode()),
            N,
            ThresholdStr);

        Builder.AddRow(
            FText::FromString(Header),
            [](const FCk_Handle&) { return FText::GetEmpty(); },
            Color_Field);

        if (R.Get_MaxAllowedEnsure() > FCk_EntityTagQuery_Requirement::NoEnsure)
        {
            Builder.AddRow(
                FText::FromString(TEXT("    Ensure ≤:")),
                [Max = R.Get_MaxAllowedEnsure()](const FCk_Handle&) { return FText::FromString(FString::FromInt(Max)); },
                Color_Field);
        }

        if (i < Results.Num())
        {
            for (const auto& H : Results[i].Get_Handles())
            {
                Builder.AddRow(
                    FText::FromString(TEXT("    Match:")),
                    [Captured = H](const FCk_Handle&) { return FText::FromString(ck::Format_UE(TEXT("{}"), Captured)); },
                    Color_OK);
            }
        }
    }

    return Builder.Build(Entity, FString());
}
