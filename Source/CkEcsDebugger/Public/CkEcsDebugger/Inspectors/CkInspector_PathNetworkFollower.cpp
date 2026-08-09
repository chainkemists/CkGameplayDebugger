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

namespace ck_inspector_pathnetworkfollower
{
    static auto Get_RouteStatusTone(ECk_PathNetwork_RouteStatus InStatus) -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_PathNetwork_RouteStatus::Pending: return ECk_Tone::Info;
            case ECk_PathNetwork_RouteStatus::Ready:   return ECk_Tone::Ok;
            case ECk_PathNetwork_RouteStatus::Failed:  return ECk_Tone::Err;
            default:                                   return ECk_Tone::Neutral;
        }
    }

    // BudgetExceeded retries next frame, so it warns; every other reason is terminal for the request.
    static auto Get_FailReasonTone(ECk_PathNetwork_RouteFailReason InReason) -> ECk_Tone
    {
        switch (InReason)
        {
            case ECk_PathNetwork_RouteFailReason::None:           return ECk_Tone::Neutral;
            case ECk_PathNetwork_RouteFailReason::BudgetExceeded: return ECk_Tone::Warn;
            default:                                              return ECk_Tone::Err;
        }
    }

    static auto Get_GoalLocation(const FCk_Handle_PathNetworkFollower& InFollower) -> FVector
    {
        if (ck::Is_NOT_Valid(InFollower))
        { return FVector::ZeroVector; }

        return UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(InFollower).Get_GoalLocation();
    }

    // Three fixed-precision components in X/Y/Z order so the goal row's axis coloring lines up with
    // the Transform inspector.
    static auto Make_GoalComponents(
        const FCk_Handle_PathNetworkFollower& InFollower)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InFollower, Axis]()
            {
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Get_GoalLocation(InFollower)[Axis]));
            }));
        }

        return Components;
    }
}

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

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Route Status:")),
        TAttribute<FText>::CreateLambda([CapturedFollower]()
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return FText::FromString(TEXT("--")); }
            const auto Status = UCk_Utils_PathNetworkFollower_UE::Get_RouteStatus(CapturedFollower);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedFollower]()
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return ECk_Tone::Neutral; }
            return ck_inspector_pathnetworkfollower::Get_RouteStatusTone(
                UCk_Utils_PathNetworkFollower_UE::Get_RouteStatus(CapturedFollower));
        }));

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Fail Reason:")),
        TAttribute<FText>::CreateLambda([CapturedFollower]()
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return FText::FromString(TEXT("--")); }
            const auto Result = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Result.Get_FailReason()));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedFollower]()
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return ECk_Tone::Neutral; }
            return ck_inspector_pathnetworkfollower::Get_FailReasonTone(
                UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower).Get_FailReason());
        }));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Goal:")),
        ck_inspector_pathnetworkfollower::Make_GoalComponents(CapturedFollower));

    // ---- Corridor shape ----

    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Legs:")),
        TAttribute<int32>::CreateLambda([CapturedFollower]()
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return 0; }
            return UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower).Get_Legs().Num();
        }),
        ECk_Tone::Info);

    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Waypoints:")),
        TAttribute<int32>::CreateLambda([CapturedFollower]()
        {
            if (ck::Is_NOT_Valid(CapturedFollower)) { return 0; }
            return UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(CapturedFollower).Get_CompiledWaypoints().Num();
        }),
        ECk_Tone::Info);

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
