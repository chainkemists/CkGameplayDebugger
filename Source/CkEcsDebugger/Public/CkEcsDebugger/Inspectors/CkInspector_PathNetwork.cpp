#include "CkInspector_PathNetwork.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_PathNetwork)

// =====================================================================================================================

auto FCkInspector_PathNetwork::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Path Network"));
}

auto FCkInspector_PathNetwork::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_PathNetwork_UE::Has(Entity);
}

auto FCkInspector_PathNetwork::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto NetworkHandle = UCk_Utils_PathNetwork_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(NetworkHandle))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedNetwork = NetworkHandle;

    // An unbuilt network routes nothing, so "No" warns rather than reading as a neutral mode.
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Built:")),
        TAttribute<FText>::CreateLambda([CapturedNetwork]()
        {
            if (ck::Is_NOT_Valid(CapturedNetwork)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(UCk_Utils_PathNetwork_UE::Get_IsBuilt(CapturedNetwork) ? TEXT("Yes") : TEXT("No"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedNetwork]()
        {
            if (ck::Is_NOT_Valid(CapturedNetwork)) { return ECk_Tone::Neutral; }
            return UCk_Utils_PathNetwork_UE::Get_IsBuilt(CapturedNetwork) ? ECk_Tone::Ok : ECk_Tone::Warn;
        }));

    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Nodes:")),
        TAttribute<int32>::CreateLambda([CapturedNetwork]()
        {
            if (ck::Is_NOT_Valid(CapturedNetwork)) { return 0; }
            return UCk_Utils_PathNetwork_UE::Get_NumNodes(CapturedNetwork);
        }),
        ECk_Tone::Info);

    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Edges:")),
        TAttribute<int32>::CreateLambda([CapturedNetwork]()
        {
            if (ck::Is_NOT_Valid(CapturedNetwork)) { return 0; }
            return UCk_Utils_PathNetwork_UE::Get_NumEdges(CapturedNetwork);
        }),
        ECk_Tone::Info);

    Builder.AddRow(
        FText::FromString(TEXT("Build Epoch:")),
        [CapturedNetwork](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedNetwork)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_PathNetwork_UE::Get_BuildEpoch(CapturedNetwork)));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}
