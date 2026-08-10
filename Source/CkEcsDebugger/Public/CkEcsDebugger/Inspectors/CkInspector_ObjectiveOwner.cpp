#include "CkInspector_ObjectiveOwner.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkObjective/Objective/CkObjective_Utils.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ObjectiveOwner)

// =====================================================================================================================

namespace ck_inspector_objective_owner
{
    // Counted over the objective set captured when the rows were composed — Tick rebuilds the inspector whenever that
    // set changes size, so the meter never outlives the handles it reads. Each read re-validates anyway.
    auto Get_CompletedCount(
        const TArray<FCk_Handle_Objective>& InObjectives)
        -> int32
    {
        auto Count = 0;

        for (const auto& Objective : InObjectives)
        {
            if (ck::Is_NOT_Valid(Objective))
            { continue; }

            if (UCk_Utils_Objective_UE::Get_Status(Objective) == ECk_ObjectiveStatus::Completed)
            { ++Count; }
        }

        return Count;
    }

    // Objectives carry no per-objective progress value, so the row is a pill, not a meter — the
    // status IS the whole payload.
    auto Get_StatusTone(
        FCk_Handle_Objective InObjective)
        -> ECk_Tone
    {
        if (ck::Is_NOT_Valid(InObjective))
        { return ECk_Tone::Neutral; }

        switch (UCk_Utils_Objective_UE::Get_Status(InObjective))
        {
            case ECk_ObjectiveStatus::NotStarted: return ECk_Tone::Neutral;
            case ECk_ObjectiveStatus::Active:     return ECk_Tone::Info;
            case ECk_ObjectiveStatus::Completed:  return ECk_Tone::Ok;
            case ECk_ObjectiveStatus::Failed:     return ECk_Tone::Err;
        }

        return ECk_Tone::Neutral;
    }
}

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
    Builder.SetEditGuard(Get_EditGuard());

    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    const auto OwnerHandle = UCk_Utils_ObjectiveOwner_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(OwnerHandle))
    {
        return Builder.Build(Entity, InFilter);
    }

    const auto Objectives = UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(OwnerHandle);
    _CachedObjectiveCount = Objectives.Num();

    // Completed / total — the count the plain header used to show is the meter's denominator.
    Builder.AddMeterRow(
        FText::FromString(TEXT("Progress:")),
        TAttribute<float>::CreateLambda([Objectives]()
        {
            if (Objectives.IsEmpty())
            { return 0.0f; }

            return static_cast<float>(ck_inspector_objective_owner::Get_CompletedCount(Objectives))
                 / static_cast<float>(Objectives.Num());
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([Objectives]()
        {
            return FText::FromString(ck::Format_UE(TEXT("{} / {}"),
                ck_inspector_objective_owner::Get_CompletedCount(Objectives),
                Objectives.Num()));
        }));

    // Each objective as a clickable row with live status
    for (const auto& ObjectiveHandle : Objectives)
    {
        if (ck::Is_NOT_Valid(ObjectiveHandle)) { continue; }

        const auto Name = UCk_Utils_Objective_UE::Get_Name(ObjectiveHandle);
        const auto CapturedObjective = ObjectiveHandle;
        const auto ObjectiveAsEntity = FCk_Handle(ObjectiveHandle);

        Builder.AddStatusPillRow(
            FText::FromString(Name.ToString()),
            TAttribute<FText>::CreateLambda([CapturedObjective]()
            {
                if (ck::Is_NOT_Valid(CapturedObjective)) { return FText::FromString(TEXT("--")); }
                const auto Status = UCk_Utils_Objective_UE::Get_Status(CapturedObjective);
                return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedObjective]()
            {
                return ck_inspector_objective_owner::Get_StatusTone(CapturedObjective);
            }),
            [WeakSelectionModel, ObjectiveAsEntity]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(ObjectiveAsEntity))
                {
                    WeakSelectionModel->Set_SelectedEntities({ ObjectiveAsEntity });
                }
            });

        // Remove rides the SAME loop as the status pill above — no second enumeration, and Tick's
        // count check already rebuilds the whole inspector when the objective set changes size.
        // LocalOk: the ObjectiveOwner request processor carries no authority gate.
        const auto CapturedOwner = OwnerHandle;

        Builder.AddActionRow(
            FText::FromString(Name.ToString()),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Remove")),
                    FText::FromString(ck::Format_UE(TEXT("UCk_Utils_ObjectiveOwner_UE::Request_RemoveObjective({})"),
                        Name.ToString())),
                    [CapturedOwner, CapturedObjective]()
                    {
                        auto MutableOwner = CapturedOwner;
                        if (ck::Is_NOT_Valid(MutableOwner) || ck::Is_NOT_Valid(CapturedObjective)) { return; }

                        UCk_Utils_ObjectiveOwner_UE::Request_RemoveObjective(MutableOwner,
                            FCk_Request_ObjectiveOwner_RemoveObjective{CapturedObjective}, {});
                    },
                    ECk_DebugRequest_Requirement::LocalOk
                },
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
