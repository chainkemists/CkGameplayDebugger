#include "CkInspector_ObjectiveOwner.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkObjective/Objective/CkObjective_Utils.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ObjectiveOwner)

// =====================================================================================================================

auto FCkInspector_ObjectiveOwner::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Objectives"));
}

auto FCkInspector_ObjectiveOwner::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_ObjectiveOwner_UE::Has(Entity);
}

auto FCkInspector_ObjectiveOwner::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildOwnerGrid(Entity, FString());
}

auto FCkInspector_ObjectiveOwner::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildOwnerGrid(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_ObjectiveOwner::BuildOwnerGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    const auto OwnerHandle = UCk_Utils_ObjectiveOwner_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(OwnerHandle))
    {
        return Builder.Build(Entity, InFilter);
    }

    const auto Objectives = UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(OwnerHandle);
    _CachedObjectiveCount = Objectives.Num();

    // Count header
    Builder.AddRow(
        FText::FromString(TEXT("Count:")),
        [Count = Objectives.Num()](const FCk_Handle& E)
        {
            return FText::FromString(ck::Format_UE(TEXT("{}"), Count));
        },
        CkStyle::TextDim());

    // Each objective as a clickable row with live status
    for (const auto& ObjectiveHandle : Objectives)
    {
        if (ck::Is_NOT_Valid(ObjectiveHandle)) { continue; }

        const auto Name = UCk_Utils_Objective_UE::Get_Name(ObjectiveHandle);
        const auto CapturedObjective = ObjectiveHandle;
        const auto ObjectiveAsEntity = FCk_Handle(ObjectiveHandle);

        Builder.AddClickableRow(
            FText::FromString(Name.ToString()),
            [CapturedObjective](const FCk_Handle& E) -> FText
            {
                if (ck::Is_NOT_Valid(CapturedObjective)) { return FText::FromString(TEXT("--")); }
                const auto Status = UCk_Utils_Objective_UE::Get_Status(CapturedObjective);
                return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
            },
            [CapturedObjective](const FCk_Handle& E) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedObjective)) { return CkStyle::None(); }
                return FCkDebuggerStyle::Get_ObjectiveStatusColor(UCk_Utils_Objective_UE::Get_Status(CapturedObjective));
            },
            [WeakSelectionModel, ObjectiveAsEntity]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(ObjectiveAsEntity))
                {
                    WeakSelectionModel->Set_SelectedEntities({ ObjectiveAsEntity });
                }
            });
    }

    return Builder.Build(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_ObjectiveOwner::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity)) { return; }

    auto MutableEntity = Entity;
    const auto OwnerHandle = UCk_Utils_ObjectiveOwner_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(OwnerHandle)) { return; }

    const auto Objectives = UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(OwnerHandle);

    if (_CachedObjectiveCount != Objectives.Num())
    {
        _CachedObjectiveCount = Objectives.Num();
        RequestRebuild();
    }
}
