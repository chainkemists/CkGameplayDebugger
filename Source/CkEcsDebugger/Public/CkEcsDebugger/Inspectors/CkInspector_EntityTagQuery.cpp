#include "CkInspector_EntityTagQuery.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkEntityTag/Query/CkEntityTagQuery_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityTagQuery)

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

    // Is Satisfied — the headline of this inspector, so it gets the pill. Live via attributes: the
    // entity is captured by value and re-Cast per read, matching the previous row's behaviour.
    const auto CapturedEntity = Entity;

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Is Satisfied:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            auto Me = CapturedEntity;
            if (ck::Is_NOT_Valid(Me)) { return FText::FromString(TEXT("--")); }
            auto Q = UCk_Utils_EntityTagQuery_UE::Cast(Me);
            return FText::FromString(UCk_Utils_EntityTagQuery_UE::Get_IsSatisfied(Q) ? TEXT("Yes") : TEXT("No"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            auto Me = CapturedEntity;
            if (ck::Is_NOT_Valid(Me)) { return ECk_Tone::Neutral; }
            auto Q = UCk_Utils_EntityTagQuery_UE::Cast(Me);
            return UCk_Utils_EntityTagQuery_UE::Get_IsSatisfied(Q) ? ECk_Tone::Ok : ECk_Tone::Err;
        }));

    const auto Reqs    = UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(Query);
    const auto Results = UCk_Utils_EntityTagQuery_UE::Get_CurrentResults(Query);

    Builder.AddRow(
        FText::FromString(FString::Printf(TEXT("Requirements (%d):"), Reqs.Num())),
        [N = Reqs.Num()](const FCk_Handle&) { return FText::FromString(FString::FromInt(N)); },
        CkStyle::Value_Numeric());

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
            CkStyle::TextDim());

        if (R.Get_MaxAllowedEnsure() > FCk_EntityTagQuery_Requirement::NoEnsure)
        {
            Builder.AddRow(
                FText::FromString(TEXT("    Ensure ≤:")),
                [Max = R.Get_MaxAllowedEnsure()](const FCk_Handle&) { return FText::FromString(FString::FromInt(Max)); },
                CkStyle::Value_Numeric());
        }

        // Matches were one formatted-handle text row each — dead text for the one thing you always
        // want to do with a match, which is go look at it. One badge box of SCkDebug_EntityRef pills
        // instead (house policy: never render an FCk_Handle as plain text).
        if (i < Results.Num())
        {
            const auto& Handles = Results[i].Get_Handles();
            if (NOT Handles.IsEmpty())
            {
                Builder.AddWidgetRow(
                    FText::FromString(TEXT("    Matches:")),
                    FCkInspectorWidgetBuilder::MakeBadgeBox(Handles));
            }
        }
    }

    return Builder.Build(Entity, FString());
}
