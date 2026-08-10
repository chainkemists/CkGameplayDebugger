#include "CkInspector_EntityInfo.h"

#include "CkCore/Enums/CkEnums.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

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
    const auto CapturedEntity = Entity;

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    return Builder
        .AddRow(
            FText::FromString(TEXT("Name:")),
            [](const FCk_Handle& E) { return FText::FromString(UCk_Utils_Handle_UE::Get_DebugName(E).ToString()); })
        // Editable debug name. LocalOk: Set_DebugName is an immediate, purely-descriptive mutator with
        // no authority gate — the "Name:" row above reads the new value on the next paint.
        // ECk_Override::Override is passed explicitly: a debugger rename that silently declined to
        // overwrite an existing name would be indistinguishable from a broken button.
        //
        // NO destroy button here: Request_DestroyEntity is destructive and there is no confirmation
        // affordance in this vocabulary (design doc "Deliberately excluded").
        .AddNameEntryRow(
            FText::FromString(TEXT("Set Name:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity)) { return FText::GetEmpty(); }
                return FText::FromName(UCk_Utils_Handle_UE::Get_DebugName(CapturedEntity));
            }),
            [CapturedEntity](FName InDebugName)
            {
                auto MutableEntity = CapturedEntity;
                if (ck::Is_NOT_Valid(MutableEntity) || InDebugName.IsNone()) { return; }

                UCk_Utils_Handle_UE::Set_DebugName(MutableEntity, InDebugName, ECk_Override::Override);
            })
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