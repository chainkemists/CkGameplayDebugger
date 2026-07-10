#include "CkInspector_Network.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Net/CkNet_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Network)

auto FCkInspector_Network::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Network"));
}

auto FCkInspector_Network::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_Network::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return FCkInspectorWidgetBuilder()
        .AddRow(
            FText::FromString(TEXT("NetMode:")),
            [](const FCk_Handle& E) { return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Net_UE::Get_EntityNetMode(E))); },
            CkStyle::Network())
        .AddRow(
            FText::FromString(TEXT("NetRole:")),
            [](const FCk_Handle& E) { return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Net_UE::Get_EntityNetRole(E))); },
            CkStyle::Network())
        .Build(Entity);
}

auto FCkInspector_Network::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed for network info
}