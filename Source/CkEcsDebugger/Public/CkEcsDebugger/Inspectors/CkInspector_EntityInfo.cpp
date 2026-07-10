#include "CkInspector_EntityInfo.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityInfo)

auto FCkInspector_EntityInfo::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Entity Info"));
}

auto FCkInspector_EntityInfo::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_EntityInfo::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return FCkInspectorWidgetBuilder()
        .AddRow(
            FText::FromString(TEXT("Name:")),
            [](const FCk_Handle& E) { return FText::FromString(UCk_Utils_Handle_UE::Get_DebugName(E).ToString()); })
        .AddWidgetRow(
            FText::FromString(TEXT("ID:")),
            SNew(SCkDebug_EntityRef)
                .Entity(Entity))
        .AddConditionalRow(
            FText::FromString(TEXT("Actor:")),
            [](const FCk_Handle& E)
            {
                if (UCk_Utils_OwningActor_UE::Has(E))
                { return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_OwningActor_UE::Get_EntityOwningActor(E))); }
                return FText::FromString(TEXT("None"));
            },
            [](const FCk_Handle& E)
            {
                return UCk_Utils_OwningActor_UE::Has(E) ? CkStyle::Text() : CkStyle::None();
            })
        .Build(Entity);
}

auto FCkInspector_EntityInfo::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed for entity info
}