#include "CkInspector_Relationships.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkRelationship/Team/CkTeam_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
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
    const auto ContextOwner = UCk_Utils_ContextOwner_UE::Has(Entity)
        ? UCk_Utils_ContextOwner_UE::Get_ContextOwner(Entity)
        : FCk_Handle{};

    const auto LifetimeOwner = Entity.Has<ck::FFragment_LifetimeOwner>()
        ? FCk_Handle{Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity()}
        : FCk_Handle{};

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
                { return CkStyle::Relationship(); }
                return CkStyle::Err();
            })
        .AddWidgetRow(
            FText::FromString(TEXT("Context Owner:")),
            SNew(SCkDebug_EntityRef)
                .Entity(ContextOwner)
                .ShowName(true))
        .AddWidgetRow(
            FText::FromString(TEXT("Lifetime Owner:")),
            SNew(SCkDebug_EntityRef)
                .Entity(LifetimeOwner)
                .ShowName(true))
        .Build(Entity);
}

auto FCkInspector_Relationships::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed for relationships
}