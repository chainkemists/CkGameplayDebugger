#include "CkInspector_Objective.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkObjective/Objective/CkObjective_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Objective)

// =====================================================================================================================

namespace ck_inspector_objective
{
    auto Get_StatusTone(
        ECk_ObjectiveStatus InStatus)
        -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_ObjectiveStatus::NotStarted: return ECk_Tone::Neutral;
            case ECk_ObjectiveStatus::Active:     return ECk_Tone::Accent;
            case ECk_ObjectiveStatus::Completed:  return ECk_Tone::Ok;
            case ECk_ObjectiveStatus::Failed:     return ECk_Tone::Err;
            default:                              return ECk_Tone::Neutral;
        }
    }
}

// =====================================================================================================================

auto FCkInspector_Objective::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Objective"));
}

auto FCkInspector_Objective::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Objective_UE::Has(Entity);
}

auto FCkInspector_Objective::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildObjectiveGrid(Entity);
}

// =====================================================================================================================

auto FCkInspector_Objective::BuildObjectiveGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto ObjectiveHandle = UCk_Utils_Objective_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(ObjectiveHandle))
    {
        return Builder.Build(Entity, FString());
    }

    const auto CapturedObjective = ObjectiveHandle;

    // Name (GameplayTag)
    const auto Name = UCk_Utils_Objective_UE::Get_Name(ObjectiveHandle);
    Builder.AddRow(
        FText::FromString(TEXT("Name:")),
        [Name](const FCk_Handle& E) { return FText::FromString(Name.ToString()); },
        CkStyle::Status_Active());

    // Display Name
    const auto DisplayName = UCk_Utils_Objective_UE::Get_DisplayName(ObjectiveHandle);
    if (NOT DisplayName.IsEmpty())
    {
        Builder.AddRow(
            FText::FromString(TEXT("Display:")),
            [DisplayName](const FCk_Handle& E) { return DisplayName; },
            CkStyle::Text());
    }

    // Description
    const auto Description = UCk_Utils_Objective_UE::Get_Description(ObjectiveHandle);
    if (NOT Description.IsEmpty())
    {
        Builder.AddRow(
            FText::FromString(TEXT("Description:")),
            [Description](const FCk_Handle& E) { return Description; },
            CkStyle::TextDim());
    }

    // Status (live-updating via TAttribute)
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Status:")),
        TAttribute<FText>::CreateLambda([CapturedObjective]()
        {
            if (ck::Is_NOT_Valid(CapturedObjective)) { return FText::FromString(TEXT("--")); }
            const auto Status = UCk_Utils_Objective_UE::Get_Status(CapturedObjective);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedObjective]()
        {
            if (ck::Is_NOT_Valid(CapturedObjective)) { return ECk_Tone::Neutral; }
            return ck_inspector_objective::Get_StatusTone(UCk_Utils_Objective_UE::Get_Status(CapturedObjective));
        }));

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_Objective::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // Status updates are handled by TAttribute lambdas — no tick logic needed
}
