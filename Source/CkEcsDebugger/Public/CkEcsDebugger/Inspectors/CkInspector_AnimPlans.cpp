#include "CkInspector_AnimPlans.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkAnimation/AnimPlan/CkAnimPlan_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_AnimPlans)

auto FCkInspector_AnimPlans::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Anim Plans"));
}

auto FCkInspector_AnimPlans::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_AnimPlan_UE::Has_Any(Entity);
}

auto FCkInspector_AnimPlans::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildAnimPlanGrid(Entity, FString());
}

auto FCkInspector_AnimPlans::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildAnimPlanGrid(Entity, InFilter);
}

auto FCkInspector_AnimPlans::BuildAnimPlanGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(MutableEntity, [&Builder, WeakSelectionModel](FCk_Handle_AnimPlan& InAnimPlan)
    {
        const auto Goal = UCk_Utils_AnimPlan_UE::Get_AnimGoal(InAnimPlan);
        const auto Cluster = UCk_Utils_AnimPlan_UE::Get_AnimCluster(InAnimPlan);
        const auto State = UCk_Utils_AnimPlan_UE::Get_AnimState(InAnimPlan);

        const auto GoalTag = Goal.Get_AnimGoal();
        const auto GoalName = GoalTag.IsValid() ? GoalTag.ToString() : TEXT("Unknown");

        const auto AnimPlanHandle = FCk_Handle(InAnimPlan);

        Builder.AddClickableRow(
            FText::FromString(GoalName),
            [GoalTag](const FCk_Handle& E)
            {
                auto MutableE = E;
                const auto Plan = UCk_Utils_AnimPlan_UE::TryGet_AnimPlan(MutableE, GoalTag);
                if (ck::Is_NOT_Valid(Plan)) { return FText::GetEmpty(); }

                const auto PlanCluster = UCk_Utils_AnimPlan_UE::Get_AnimCluster(Plan);
                const auto PlanState = UCk_Utils_AnimPlan_UE::Get_AnimState(Plan);

                const auto ClusterTag = PlanCluster.Get_AnimCluster();
                const auto StateTag = PlanState.Get_AnimState();

                auto ClusterStr = ClusterTag.IsValid() ? ClusterTag.ToString() : TEXT("-");
                auto StateStr = StateTag.IsValid() ? StateTag.ToString() : TEXT("-");

                return FText::FromString(ck::Format_UE(TEXT("{} | {}"), ClusterStr, StateStr));
            },
            CkStyle::Value_Tag(),
            [WeakSelectionModel, AnimPlanHandle]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(AnimPlanHandle))
                {
                    WeakSelectionModel->Set_SelectedEntities({ AnimPlanHandle });
                }
            });

        // ---- Cluster / state entries ----
        // Both requests are LocalOk: an anim plan drives local presentation and re-derives from the
        // owning gameplay state, so a client-side nudge is a legitimate experiment.
        const auto CapturedPlan = InAnimPlan;

        // Cluster is a ONE-tag request, so committing the entry IS the write — no button needed.
        Builder.AddTagEntryRow(
            FText::FromString(TEXT("  Cluster:")),
            TAttribute<FText>::CreateLambda([CapturedPlan]()
            {
                if (ck::Is_NOT_Valid(CapturedPlan)) { return FText::FromString(TEXT("--")); }
                const auto Tag = UCk_Utils_AnimPlan_UE::Get_AnimCluster(CapturedPlan).Get_AnimCluster();
                return Tag.IsValid() ? FText::FromName(Tag.GetTagName()) : FText::FromString(TEXT("(none)"));
            }),
            [CapturedPlan](FGameplayTag InTag)
            {
                auto Mutable = CapturedPlan;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_AnimPlan_UE::Request_UpdateAnimCluster(Mutable,
                    FCk_Request_AnimPlan_UpdateAnimCluster{InTag}, {});
            });

        // State is a TWO-tag request (cluster + state), and a half-filled pair is not a request worth
        // firing — so the two entries stage into row-owned pendings and an explicit button commits the
        // pair. The pendings die with the row; a rebuild reseeds them from the live plan.
        const auto PendingCluster = MakeShared<FGameplayTag>(
            ck::IsValid(CapturedPlan) ? UCk_Utils_AnimPlan_UE::Get_AnimCluster(CapturedPlan).Get_AnimCluster() : FGameplayTag{});
        const auto PendingState = MakeShared<FGameplayTag>(
            ck::IsValid(CapturedPlan) ? UCk_Utils_AnimPlan_UE::Get_AnimState(CapturedPlan).Get_AnimState() : FGameplayTag{});

        Builder.AddTagEntryRow(
            FText::FromString(TEXT("  State <- cluster:")),
            TAttribute<FText>::CreateLambda([PendingCluster]()
            {
                return PendingCluster->IsValid() ? FText::FromName(PendingCluster->GetTagName()) : FText::FromString(TEXT("(none)"));
            }),
            [PendingCluster](FGameplayTag InTag) { *PendingCluster = InTag; });

        Builder.AddTagEntryRow(
            FText::FromString(TEXT("  State <- state:")),
            TAttribute<FText>::CreateLambda([PendingState]()
            {
                return PendingState->IsValid() ? FText::FromName(PendingState->GetTagName()) : FText::FromString(TEXT("(none)"));
            }),
            [PendingState](FGameplayTag InTag) { *PendingState = InTag; });

        Builder.AddActionRow(
            FText::FromString(TEXT("  ")),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Apply State")),
                    FText::FromString(TEXT("Request_UpdateAnimState with the two staged tags above. Both must resolve to real gameplay tags.")),
                    [CapturedPlan, PendingCluster, PendingState]()
                    {
                        auto Mutable = CapturedPlan;
                        if (ck::Is_NOT_Valid(Mutable)) { return; }
                        if (NOT PendingCluster->IsValid() || NOT PendingState->IsValid()) { return; }

                        UCk_Utils_AnimPlan_UE::Request_UpdateAnimState(Mutable,
                            FCk_Request_AnimPlan_UpdateAnimState{*PendingCluster, *PendingState}, {});
                    }
                },
            });
    });

    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_AnimPlans::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
