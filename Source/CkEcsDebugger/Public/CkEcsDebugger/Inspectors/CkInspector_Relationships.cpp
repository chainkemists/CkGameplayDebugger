#include "CkInspector_Relationships.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkRelationship/Team/CkTeam_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Relationships)

auto FCkInspector_Relationships::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Relationships"));
}

auto FCkInspector_Relationships::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_Relationships::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto WeakSelectionModel = SelectionModel;

    return FCkInspectorWidgetBuilder()
        .AddConditionalRow(
            FText::FromString(TEXT("Team:")),
            [](const FCk_Handle& E)
            {
                if (const auto TeamEntity = UCk_Utils_Team_UE::Cast(E); ck::IsValid(TeamEntity))
                { return FText::FromString(ck::Format_UE(TEXT("{} (Starts from ZERO)"), UCk_Utils_Team_UE::Get_ID(TeamEntity))); }
                return FText::FromString(TEXT("Unknown"));
            },
            [](const FCk_Handle& E)
            {
                if (const auto TeamEntity = UCk_Utils_Team_UE::Cast(E); ck::IsValid(TeamEntity))
                { return FCkDebuggerStyle::Color_Relationship; }
                return FCkDebuggerStyle::Color_Error;
            })
        .AddClickableRow(
            FText::FromString(TEXT("Context Owner:")),
            [](const FCk_Handle& E)
            {
                if (UCk_Utils_ContextOwner_UE::Has(E))
                {
                    const auto ContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(E);
                    return FText::FromString(ck::Format_UE(TEXT("{} | {}"), UCk_Utils_Handle_UE::Get_DebugName(ContextOwner), ContextOwner));
                }
                return FText::FromString(TEXT("None"));
            },
            FCkDebuggerStyle::Color_Reference,
            [WeakSelectionModel, Entity]()
            {
                if (NOT WeakSelectionModel.IsValid() || NOT UCk_Utils_ContextOwner_UE::Has(Entity))
                { return; }
                const auto ContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(Entity);
                if (ck::IsValid(ContextOwner))
                {
                    WeakSelectionModel->Set_SelectedEntities({ ContextOwner });
                }
            })
        .AddClickableRow(
            FText::FromString(TEXT("Lifetime Owner:")),
            [](const FCk_Handle& E)
            {
                if (E.Has<ck::FFragment_LifetimeOwner>())
                {
                    const auto& LifetimeOwner = E.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
                    return FText::FromString(ck::Format_UE(TEXT("{} | {}"), UCk_Utils_Handle_UE::Get_DebugName(LifetimeOwner), LifetimeOwner));
                }
                return FText::FromString(TEXT("None"));
            },
            FCkDebuggerStyle::Color_Reference,
            [WeakSelectionModel, Entity]()
            {
                if (NOT WeakSelectionModel.IsValid() || NOT Entity.Has<ck::FFragment_LifetimeOwner>())
                { return; }
                const auto& LifetimeOwner = Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
                if (ck::IsValid(LifetimeOwner))
                {
                    WeakSelectionModel->Set_SelectedEntities({ LifetimeOwner });
                }
            })
        .Build(Entity);
}

auto FCkInspector_Relationships::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed for relationships
}