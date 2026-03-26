#include "CkInspector_Objective.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkObjective/Objective/CkObjective_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Objective)

static const FLinearColor Color_NotStarted = FLinearColor(0.5f, 0.5f, 0.5f);
static const FLinearColor Color_Active     = FLinearColor(0.55f, 0.78f, 0.95f);
static const FLinearColor Color_Completed  = FLinearColor(0.6f, 0.85f, 0.55f);
static const FLinearColor Color_Failed     = FLinearColor(0.95f, 0.35f, 0.3f);
static const FLinearColor Color_MetaData   = FLinearColor(0.85f, 0.75f, 0.55f);

// =====================================================================================================================

static auto GetStatusColor(ECk_ObjectiveStatus InStatus) -> FLinearColor
{
    switch (InStatus)
    {
        case ECk_ObjectiveStatus::NotStarted: return Color_NotStarted;
        case ECk_ObjectiveStatus::Active:     return Color_Active;
        case ECk_ObjectiveStatus::Completed:  return Color_Completed;
        case ECk_ObjectiveStatus::Failed:     return Color_Failed;
        default:                              return FCkDebuggerStyle::Color_Text_Primary;
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
        Color_Active);

    // Display Name
    const auto DisplayName = UCk_Utils_Objective_UE::Get_DisplayName(ObjectiveHandle);
    if (NOT DisplayName.IsEmpty())
    {
        Builder.AddRow(
            FText::FromString(TEXT("Display:")),
            [DisplayName](const FCk_Handle& E) { return DisplayName; },
            FCkDebuggerStyle::Color_Text_Primary);
    }

    // Description
    const auto Description = UCk_Utils_Objective_UE::Get_Description(ObjectiveHandle);
    if (NOT Description.IsEmpty())
    {
        Builder.AddRow(
            FText::FromString(TEXT("Description:")),
            [Description](const FCk_Handle& E) { return Description; },
            FCkDebuggerStyle::Color_Text_Secondary);
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
            if (ck::Is_NOT_Valid(CapturedObjective)) { return FCkDebuggerStyle::Color_None; }
            return GetStatusColor(UCk_Utils_Objective_UE::Get_Status(CapturedObjective));
        });

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_Objective::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // Status updates are handled by TAttribute lambdas — no tick logic needed
}
