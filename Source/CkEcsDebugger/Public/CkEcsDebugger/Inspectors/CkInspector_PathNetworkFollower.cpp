#include "CkInspector_PathNetworkFollower.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkPathNetwork/Network/CkPathNetwork_Fragment.h"
#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_PathNetworkFollower)
namespace ck_inspector_pathnetworkfollower
{
    enum class EValue : uint8
    {
        RouteStatus,
        FailReason,
        GoalX,
        GoalY,
        GoalZ,
        Legs,
        Waypoints,
        TotalCost,
    };

    auto TryGetFollower(
        const FCk_Handle& InEntity,
        FCk_Handle_PathNetworkFollower& OutFollower)
        -> bool
    {
        if (NOT ck::IsValid(InEntity) || NOT UCk_Utils_PathNetworkFollower_UE::Has(InEntity))
        {
            return false;
        }

        auto MutableEntity = InEntity;
        const auto Follower = UCk_Utils_PathNetworkFollower_UE::Cast(MutableEntity);
        if (ck::Is_NOT_Valid(Follower) || NOT Follower.Has<ck::FFragment_PathNetworkFollower_Params>() ||
            NOT Follower.Has<ck::FFragment_PathNetworkFollower_Corridor>())
        {
            return false;
        }

        OutFollower = Follower;
        return true;
    }

    auto TryGetResult(
        const FCk_Handle& InEntity,
        FCk_PathNetwork_RouteResult& OutResult)
        -> bool
    {
        auto Follower = FCk_Handle_PathNetworkFollower{};
        if (NOT TryGetFollower(InEntity, Follower))
        {
            return false;
        }

        OutResult = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(Follower);
        return true;
    }

    auto GetRouteTone(const ECk_PathNetwork_RouteStatus InStatus) -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_PathNetwork_RouteStatus::Pending: return ECk_Tone::Info;
            case ECk_PathNetwork_RouteStatus::Ready:   return ECk_Tone::Ok;
            case ECk_PathNetwork_RouteStatus::Failed:  return ECk_Tone::Err;
            default:                                   return ECk_Tone::Neutral;
        }
    }

    // BudgetExceeded retries next frame, so it warns; every other non-empty reason is terminal.
    auto GetFailReasonTone(const ECk_PathNetwork_RouteFailReason InReason) -> ECk_Tone
    {
        if (InReason == ECk_PathNetwork_RouteFailReason::BudgetExceeded)
        {
            return ECk_Tone::Warn;
        }
        return InReason == ECk_PathNetwork_RouteFailReason::None ? ECk_Tone::Neutral : ECk_Tone::Err;
    }

    auto GetText(
        const FCk_Handle& InEntity,
        const EValue InValue)
        -> FString
    {
        auto Result = FCk_PathNetwork_RouteResult{};
        if (NOT TryGetResult(InEntity, Result))
        {
            return TEXT("--");
        }

        switch (InValue)
        {
            case EValue::RouteStatus: return ck::Format_UE(TEXT("{}"), Result.Get_Status());
            case EValue::FailReason:  return ck::Format_UE(TEXT("{}"), Result.Get_FailReason());
            case EValue::GoalX:       return ck::Format_UE(TEXT("{:.3f}"), Result.Get_GoalLocation().X);
            case EValue::GoalY:       return ck::Format_UE(TEXT("{:.3f}"), Result.Get_GoalLocation().Y);
            case EValue::GoalZ:       return ck::Format_UE(TEXT("{:.3f}"), Result.Get_GoalLocation().Z);
            case EValue::Legs:        return ck::Format_UE(TEXT("{}"), Result.Get_Legs().Num());
            case EValue::Waypoints:   return ck::Format_UE(TEXT("{}"), Result.Get_CompiledWaypoints().Num());
            case EValue::TotalCost:   return ck::Format_UE(TEXT("{}"), Result.Get_TotalCost());
        }

        return TEXT("--");
    }

    auto GetRouteTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        auto Result = FCk_PathNetwork_RouteResult{};
        return TryGetResult(InEntity, Result) ? GetRouteTone(Result.Get_Status()) : ECk_Tone::Neutral;
    }

    auto GetFailReasonTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        auto Result = FCk_PathNetwork_RouteResult{};
        return TryGetResult(InEntity, Result) ? GetFailReasonTone(Result.Get_FailReason()) : ECk_Tone::Neutral;
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

auto SCkInspector_PathNetworkFollowerAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _RouteStatusDiffMarked = InArgs._RouteStatusDiffMarked;
    _FailReasonDiffMarked = InArgs._FailReasonDiffMarked;
    _GoalDiffMarked = InArgs._GoalDiffMarked;
    _LegsDiffMarked = InArgs._LegsDiffMarked;
    _WaypointsDiffMarked = InArgs._WaypointsDiffMarked;
    _TotalCostDiffMarked = InArgs._TotalCostDiffMarked;
    _FeatureDiffMarked = InArgs._FeatureDiffMarked;

    const TSharedRef<SBox> RootBox = SNew(SBox);
    ChildSlot[RootBox];
    if (Build_AuthoredView())
    {
        RootBox->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    RootBox->SetContent(SNullWidget::NullWidget);
}
SCkInspector_PathNetworkFollowerAuthored::~SCkInspector_PathNetworkFollowerAuthored()
{
    Release();
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_IsAvailable() const -> bool
{
    auto Follower = FCk_Handle_PathNetworkFollower{};
    return _Active && ck_inspector_pathnetworkfollower::TryGetFollower(_Entity, Follower);
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_RouteStatusText() const -> FString
{
    return _Active ? ck_inspector_pathnetworkfollower::GetText(
                         _Entity, ck_inspector_pathnetworkfollower::EValue::RouteStatus)
                   : FString{};
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_FailReasonText() const -> FString
{
    return _Active ? ck_inspector_pathnetworkfollower::GetText(
                         _Entity, ck_inspector_pathnetworkfollower::EValue::FailReason)
                   : FString{};
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_GoalXText() const -> FString
{
    return _Active ? ck_inspector_pathnetworkfollower::GetText(
                         _Entity, ck_inspector_pathnetworkfollower::EValue::GoalX)
                   : FString{};
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_GoalYText() const -> FString
{
    return _Active ? ck_inspector_pathnetworkfollower::GetText(
                         _Entity, ck_inspector_pathnetworkfollower::EValue::GoalY)
                   : FString{};
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_GoalZText() const -> FString
{
    return _Active ? ck_inspector_pathnetworkfollower::GetText(
                         _Entity, ck_inspector_pathnetworkfollower::EValue::GoalZ)
                   : FString{};
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_LegsText() const -> FString
{
    return _Active ? ck_inspector_pathnetworkfollower::GetText(
                         _Entity, ck_inspector_pathnetworkfollower::EValue::Legs)
                   : FString{};
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_WaypointsText() const -> FString
{
    return _Active ? ck_inspector_pathnetworkfollower::GetText(
                         _Entity, ck_inspector_pathnetworkfollower::EValue::Waypoints)
                   : FString{};
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_TotalCostText() const -> FString
{
    return _Active ? ck_inspector_pathnetworkfollower::GetText(
                         _Entity, ck_inspector_pathnetworkfollower::EValue::TotalCost)
                   : FString{};
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_RouteStatusForeground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneColor(ck_inspector_pathnetworkfollower::GetRouteTone(_Entity))
                   : FLinearColor::Transparent;
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_RouteStatusBackground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneDimColor(ck_inspector_pathnetworkfollower::GetRouteTone(_Entity))
                   : FLinearColor::Transparent;
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_FailReasonForeground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneColor(ck_inspector_pathnetworkfollower::GetFailReasonTone(_Entity))
                   : FLinearColor::Transparent;
}
auto SCkInspector_PathNetworkFollowerAuthored::Get_FailReasonBackground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneDimColor(ck_inspector_pathnetworkfollower::GetFailReasonTone(_Entity))
                   : FLinearColor::Transparent;
}

auto SCkInspector_PathNetworkFollowerAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid())
        {
            Errors.Add(TEXT("Debugger UI registry is unavailable."));
        }
        if (NOT Plugin.IsValid())
        {
            Errors.Add(TEXT("CkDebugger plugin is unavailable."));
        }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_PathNetworkFollowerAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText =
        [&Data, WeakWidget](
            const FString& Name,
            FString (SCkInspector_PathNetworkFollowerAuthored::* Getter)() const)
    {
        Data.Text.Add(Name, TAttribute<FText>::CreateLambda([WeakWidget, Getter]()
        {
            const TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*Getter)())
                : FText::GetEmpty();
        }));
    };
    BindText(TEXT("path-network-follower-route-status"),
             &SCkInspector_PathNetworkFollowerAuthored::Get_RouteStatusText);
    BindText(TEXT("path-network-follower-fail-reason"), &SCkInspector_PathNetworkFollowerAuthored::Get_FailReasonText);
    BindText(TEXT("path-network-follower-goal-x"), &SCkInspector_PathNetworkFollowerAuthored::Get_GoalXText);
    BindText(TEXT("path-network-follower-goal-y"), &SCkInspector_PathNetworkFollowerAuthored::Get_GoalYText);
    BindText(TEXT("path-network-follower-goal-z"), &SCkInspector_PathNetworkFollowerAuthored::Get_GoalZText);
    BindText(TEXT("path-network-follower-legs"), &SCkInspector_PathNetworkFollowerAuthored::Get_LegsText);
    BindText(TEXT("path-network-follower-waypoints"), &SCkInspector_PathNetworkFollowerAuthored::Get_WaypointsText);
    BindText(TEXT("path-network-follower-total-cost"), &SCkInspector_PathNetworkFollowerAuthored::Get_TotalCostText);
    Data.Text.Add(TEXT("path-network-follower-remove-label"), FText::FromString(TEXT("Remove")));
    Data.Text.Add(TEXT("path-network-follower-remove-tooltip"),
                  FText::FromString(TEXT("Remove the PathNetworkFollower feature from this entity.")));
    Data.Text.Add(TEXT("path-network-follower-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable()
            ? FText::GetEmpty()
            : FText::FromString(TEXT("Path Network Follower is unavailable."));
    }));
    Data.Visibility.Add(TEXT("path-network-follower-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    const auto BindColor =
        [&Data, WeakWidget](
            const FString& Name,
            FLinearColor (SCkInspector_PathNetworkFollowerAuthored::* Getter)() const)
    {
        Data.Color.Add(Name, TAttribute<FLinearColor>::CreateLambda([WeakWidget, Getter]()
        {
            const TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? (Widget.Get()->*Getter)()
                : FLinearColor::Transparent;
        }));
    };
    BindColor(TEXT("path-network-follower-route-status-foreground"),
              &SCkInspector_PathNetworkFollowerAuthored::Get_RouteStatusForeground);
    BindColor(TEXT("path-network-follower-route-status-background"),
              &SCkInspector_PathNetworkFollowerAuthored::Get_RouteStatusBackground);
    BindColor(TEXT("path-network-follower-fail-reason-foreground"),
              &SCkInspector_PathNetworkFollowerAuthored::Get_FailReasonForeground);
    BindColor(TEXT("path-network-follower-fail-reason-background"),
              &SCkInspector_PathNetworkFollowerAuthored::Get_FailReasonBackground);
    const auto BindDiff = [&Data, WeakWidget](
        const FString& Name,
        const bool SCkInspector_PathNetworkFollowerAuthored::* Member)
    {
        Data.Color.Add(Name, TAttribute<FLinearColor>::CreateLambda([WeakWidget, Member]()
        {
            const TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_pathnetworkfollower::DiffColor(Widget.Get()->*Member)
                : FLinearColor::Transparent;
        }));
    };
    BindDiff(TEXT("path-network-follower-route-status-diff-color"),
             &SCkInspector_PathNetworkFollowerAuthored::_RouteStatusDiffMarked);
    BindDiff(TEXT("path-network-follower-fail-reason-diff-color"),
             &SCkInspector_PathNetworkFollowerAuthored::_FailReasonDiffMarked);
    BindDiff(TEXT("path-network-follower-goal-diff-color"), &SCkInspector_PathNetworkFollowerAuthored::_GoalDiffMarked);
    BindDiff(TEXT("path-network-follower-legs-diff-color"), &SCkInspector_PathNetworkFollowerAuthored::_LegsDiffMarked);
    BindDiff(TEXT("path-network-follower-waypoints-diff-color"),
             &SCkInspector_PathNetworkFollowerAuthored::_WaypointsDiffMarked);
    BindDiff(TEXT("path-network-follower-total-cost-diff-color"),
             &SCkInspector_PathNetworkFollowerAuthored::_TotalCostDiffMarked);
    BindDiff(TEXT("path-network-follower-feature-diff-color"),
             &SCkInspector_PathNetworkFollowerAuthored::_FeatureDiffMarked);
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("path-network-follower-remove"), FSimpleDelegate::CreateLambda([WeakWidget]()
    {
        if (const TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
        {
            Widget->Remove();
        }
    }));
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorPathNetworkFollower.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorPathNetworkFollower.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}
auto SCkInspector_PathNetworkFollowerAuthored::Remove() -> void
{
    auto Follower = FCk_Handle_PathNetworkFollower{};
    if (_Active && ck_inspector_pathnetworkfollower::TryGetFollower(_Entity, Follower))
    {
        UCk_Utils_PathNetworkFollower_UE::Remove(Follower);
    }
}
auto SCkInspector_PathNetworkFollowerAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (_Active && _View.IsValid())
    {
        _View->PollFiles();
        if (NOT _View->GetLastResult().Succeeded)
        {
            _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n"));
        }
        else
        {
            _LoadError.Reset();
        }
    }
}
auto SCkInspector_PathNetworkFollowerAuthored::Release() -> void
{
    if (NOT _Active)
    {
        return;
    }

    _Active = false;
    _Entity = {};
    _View.Reset();
    _Mounted = false;
}
FCkInspector_PathNetworkFollower::~FCkInspector_PathNetworkFollower()
{
    OnDeactivated();
}
auto FCkInspector_PathNetworkFollower::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Path Network Follower"));
}
auto FCkInspector_PathNetworkFollower::CanInspect(const FCk_Handle& InEntity) const -> bool
{
    auto Follower = FCk_Handle_PathNetworkFollower{};
    return ck_inspector_pathnetworkfollower::TryGetFollower(InEntity, Follower);
}
auto FCkInspector_PathNetworkFollower::Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    const FCk_Handle CapturedEntity = InEntity;
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Route Status:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            return FText::FromString(ck_inspector_pathnetworkfollower::GetText(
                CapturedEntity, ck_inspector_pathnetworkfollower::EValue::RouteStatus));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            return ck_inspector_pathnetworkfollower::GetRouteTone(CapturedEntity);
        }));
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Fail Reason:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            return FText::FromString(ck_inspector_pathnetworkfollower::GetText(
                CapturedEntity, ck_inspector_pathnetworkfollower::EValue::FailReason));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            return ck_inspector_pathnetworkfollower::GetFailReasonTone(CapturedEntity);
        }));

    auto GoalComponents = TArray<TAttribute<FText>>{};
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        GoalComponents.Add(TAttribute<FText>::CreateLambda([CapturedEntity, Axis]()
        {
            return FText::FromString(ck_inspector_pathnetworkfollower::GetText(
                CapturedEntity, static_cast<ck_inspector_pathnetworkfollower::EValue>(
                    static_cast<uint8>(ck_inspector_pathnetworkfollower::EValue::GoalX) + Axis)));
        }));
    }
    Builder.AddAlignedNumericRow(FText::FromString(TEXT("Goal:")), GoalComponents);

    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Legs:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        {
            auto Result = FCk_PathNetwork_RouteResult{};
            return ck_inspector_pathnetworkfollower::TryGetResult(CapturedEntity, Result)
                ? Result.Get_Legs().Num()
                : 0;
        }),
        ECk_Tone::Info);
    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Waypoints:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        {
            auto Result = FCk_PathNetwork_RouteResult{};
            return ck_inspector_pathnetworkfollower::TryGetResult(CapturedEntity, Result)
                ? Result.Get_CompiledWaypoints().Num()
                : 0;
        }),
        ECk_Tone::Info);
    Builder.AddRow(
        FText::FromString(TEXT("Total Cost:")),
        [CapturedEntity](const FCk_Handle&)
        {
            return FText::FromString(ck_inspector_pathnetworkfollower::GetText(
                CapturedEntity, ck_inspector_pathnetworkfollower::EValue::TotalCost));
        },
        CkStyle::Value_Numeric());
    Builder.AddActionRow(
        FText::FromString(TEXT("Feature:")),
        {FCkInspector_Action
        {
            FText::FromString(TEXT("Remove")),
            FText::FromString(TEXT("Remove the PathNetworkFollower feature from this entity.")),
            [CapturedEntity]()
            {
                auto Follower = FCk_Handle_PathNetworkFollower{};
                if (ck_inspector_pathnetworkfollower::TryGetFollower(CapturedEntity, Follower))
                {
                    UCk_Utils_PathNetworkFollower_UE::Remove(Follower);
                }
            },
            ECk_DebugRequest_Requirement::LocalOk
        }});
    return Builder.Build(InEntity, FString{});
}
auto FCkInspector_PathNetworkFollower::Build_Inspector(const FCk_Handle& InEntity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(InEntity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    {
        return NativeBody;
    }

    const TSharedRef<SCkInspector_PathNetworkFollowerAuthored> Authored =
        SNew(SCkInspector_PathNetworkFollowerAuthored)
            .Entity(InEntity)
            .RouteStatusDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Route Status:")))
            .FailReasonDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Fail Reason:")))
            .GoalDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Goal:")))
            .LegsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Legs:")))
            .WaypointsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Waypoints:")))
            .TotalCostDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Total Cost:")))
            .FeatureDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Feature:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}
auto FCkInspector_PathNetworkFollower::Tick(const FCk_Handle&, float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_PathNetworkFollowerAuthored>& Instance)
    {
        return NOT Instance.IsValid() || Instance.Pin()->Is_Inert();
    });
}
auto FCkInspector_PathNetworkFollower::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_PathNetworkFollowerAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> Instance = WeakInstance.Pin();
            Instance.IsValid())
        {
            Instance->Release();
        }
    }
    _AuthoredInstances.Reset();
}
