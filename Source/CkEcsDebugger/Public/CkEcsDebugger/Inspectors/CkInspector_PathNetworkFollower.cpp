#include "CkInspector_PathNetworkFollower.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_PathNetworkFollower)

// =====================================================================================================================

auto FCkInspector_PathNetworkFollower::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Path Network Follower"));
}

auto FCkInspector_PathNetworkFollower::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_PathNetworkFollower_UE::Has(Entity);
}

auto FCkInspector_PathNetworkFollower::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto FollowerHandle = UCk_Utils_PathNetworkFollower_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(FollowerHandle))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedFollower = FollowerHandle;

    // ---- Route state ----

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Route Status:")),
        [CapturedFollower](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return FText::FromString(TEXT("--")); }
            const auto Status = UCk_Utils_PathNetworkFollower_UE::Get_RouteStatus(CapturedFollower);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
        },
        [CapturedFollower](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return CkStyle::None(); }
            return UCk_Utils_PathNetworkFollower_UE::Get_RouteStatus(CapturedFollower) == ECk_PathNetwork_RouteStatus::Ready
                ? CkStyle::Status_Active()
                : CkStyle::Value_Enum();
        });

    Builder.AddRow(
        FText::FromString(TEXT("Fail Reason:")),
        [CapturedFollower](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return FText::FromString(TEXT("--")); }
            const auto Result = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Result.Get_FailReason()));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("Goal:")),
        [CapturedFollower](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return FText::FromString(TEXT("--")); }
            const auto Result = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Result.Get_GoalLocation()));
        },
        CkStyle::Value_Numeric());

    // ---- Corridor shape ----

    Builder.AddRow(
        FText::FromString(TEXT("Legs:")),
        [CapturedFollower](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return FText::FromString(TEXT("--")); }
            const auto Result = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Result.Get_Legs().Num()));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Waypoints:")),
        [CapturedFollower](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return FText::FromString(TEXT("--")); }
            const auto Result = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Result.Get_CompiledWaypoints().Num()));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Total Cost:")),
        [CapturedFollower](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return FText::FromString(TEXT("--")); }
            const auto Result = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Result.Get_TotalCost()));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}
