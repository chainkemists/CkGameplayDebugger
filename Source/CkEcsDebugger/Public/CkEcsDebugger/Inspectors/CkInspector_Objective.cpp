#include "CkInspector_Objective.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkObjective/Objective/CkObjective_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Objective)

static const FLinearColor Color_MetaData = FLinearColor(0.85f, 0.75f, 0.55f);

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
        CkDebugStyle::Status_Active());

    // Display Name
    const auto DisplayName = UCk_Utils_Objective_UE::Get_DisplayName(ObjectiveHandle);
    if (NOT DisplayName.IsEmpty())
    {
        Builder.AddRow(
            FText::FromString(TEXT("Display:")),
            [DisplayName](const FCk_Handle& E) { return DisplayName; },
            CkDebugStyle::Text());
    }

    // Description
    const auto Description = UCk_Utils_Objective_UE::Get_Description(ObjectiveHandle);
    if (NOT Description.IsEmpty())
    {
        Builder.AddRow(
            FText::FromString(TEXT("Description:")),
            [Description](const FCk_Handle& E) { return Description; },
            CkDebugStyle::TextDim());
    }

    // Status (live-updating via TAttribute)
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Status:")),
        [CapturedObjective](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedObjective)) { return FText::FromString(TEXT("--")); }
            const auto Status = UCk_Utils_Objective_UE::Get_Status(CapturedObjective);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
        },
        [CapturedObjective](const FCk_Handle& E) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedObjective)) { return CkDebugStyle::None(); }
            return FCkDebuggerStyle::Get_ObjectiveStatusColor(UCk_Utils_Objective_UE::Get_Status(CapturedObjective));
        });

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_Objective::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // Status updates are handled by TAttribute lambdas — no tick logic needed
}
