#include "CkInspector_IsmProxy.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Fragment.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Utils.h"
#include "CkIsmRenderer/Renderer/CkIsmRenderer_Fragment_Data.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/StaticMesh.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IsmProxy)

// =====================================================================================================================

namespace ck_inspector_ism_proxy
{
    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

    auto TryGetProxy(const FCk_Handle& InEntity, FCk_Handle_IsmProxy& OutProxy) -> bool
    {
        OutProxy = {};
        if (IsDestroying(InEntity)
            || NOT InEntity.Has<ck::FFragment_IsmProxy_Params>()
            || NOT InEntity.Has<ck::FFragment_IsmProxy_Current>())
        { return false; }
        if (NOT ck::IsValid(InEntity.Get<ck::FFragment_IsmProxy_Params>().Get_IsmRenderer()))
        { return false; }

        auto MutableEntity = InEntity;
        OutProxy = UCk_Utils_IsmProxy_UE::Cast(MutableEntity);
        return ck::IsValid(OutProxy);
    }

    auto GetGate(const FCk_Handle& InEntity) -> FCk_DebugRequest_GateVerdict
    {
        auto Proxy = FCk_Handle_IsmProxy{};
        if (NOT TryGetProxy(InEntity, Proxy))
        { return {false, FText::FromString(TEXT("ISM Proxy is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::CosmeticOnly);
    }

    auto GetMobilityText(const ECk_Mobility InMobility) -> FString
    {
        switch (InMobility)
        {
            case ECk_Mobility::Static: return TEXT("Static");
            case ECk_Mobility::Movable: return TEXT("Movable");
            case ECk_Mobility::Stationary: return TEXT("Stationary");
            default: return TEXT("Unknown");
        }
    }

    auto DiffColor(const bool bInDiffMarked) -> FLinearColor
    {
        return bInDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }

    auto Make_AxisComponents(
        const FCk_Handle& InEntity,
        const TCHAR* InFormat,
        TFunction<FVector(const FCk_Handle_IsmProxy&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, InFormat, InProjector, Axis]()
            {
                auto Proxy = FCk_Handle_IsmProxy{};
                return TryGetProxy(InEntity, Proxy)
                    ? FText::FromString(ck::Format_UE(InFormat, InProjector(Proxy)[Axis]))
                    : FText::FromString(TEXT("--"));
            }));
        }
        return Components;
    }
}

namespace
{
    auto FormatVectorComponent(
        const FCk_Handle& InEntity,
        const int32 InAxis,
        const TCHAR* InFormat,
        TFunction<FVector(const FCk_Handle_IsmProxy&)> InProjector) -> FString
    {
        auto Proxy = FCk_Handle_IsmProxy{};
        return ck_inspector_ism_proxy::TryGetProxy(InEntity, Proxy)
            ? ck::Format_UE(InFormat, InProjector(Proxy)[InAxis]) : TEXT("--");
    }
}

// =====================================================================================================================

auto SCkInspector_IsmProxyAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_IsmProxyAuthored::~SCkInspector_IsmProxyAuthored()
{
    Release();
}

auto SCkInspector_IsmProxyAuthored::Get_IsAvailable() const -> bool
{
    auto Proxy = FCk_Handle_IsmProxy{};
    return _Active && ck_inspector_ism_proxy::TryGetProxy(_Entity, Proxy);
}

auto SCkInspector_IsmProxyAuthored::Get_MeshText() const -> FString
{
    auto Proxy = FCk_Handle_IsmProxy{};
    if (NOT _Active || NOT ck_inspector_ism_proxy::TryGetProxy(_Entity, Proxy))
    { return TEXT("--"); }
    const UStaticMesh* const Mesh = UCk_Utils_IsmProxy_UE::Get_Mesh(Proxy);
    return ck::IsValid(Mesh) ? Mesh->GetName() : TEXT("None");
}

auto SCkInspector_IsmProxyAuthored::Get_MobilityText() const -> FString
{
    auto Proxy = FCk_Handle_IsmProxy{};
    return _Active && ck_inspector_ism_proxy::TryGetProxy(_Entity, Proxy)
        ? ck_inspector_ism_proxy::GetMobilityText(UCk_Utils_IsmProxy_UE::Get_Mobility(Proxy))
        : TEXT("--");
}

auto SCkInspector_IsmProxyAuthored::Get_LocationXText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 0, TEXT("{:.1f}"), [](const auto& P) { return UCk_Utils_IsmProxy_UE::Get_LocalLocationOffset(P); }) : TEXT("--"); }
auto SCkInspector_IsmProxyAuthored::Get_LocationYText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 1, TEXT("{:.1f}"), [](const auto& P) { return UCk_Utils_IsmProxy_UE::Get_LocalLocationOffset(P); }) : TEXT("--"); }
auto SCkInspector_IsmProxyAuthored::Get_LocationZText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 2, TEXT("{:.1f}"), [](const auto& P) { return UCk_Utils_IsmProxy_UE::Get_LocalLocationOffset(P); }) : TEXT("--"); }
auto SCkInspector_IsmProxyAuthored::Get_RotationRollText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 0, TEXT("{:.2f}"), [](const auto& P) { const auto R = UCk_Utils_IsmProxy_UE::Get_LocalRotationOffset(P); return FVector{R.Roll, R.Pitch, R.Yaw}; }) : TEXT("--"); }
auto SCkInspector_IsmProxyAuthored::Get_RotationPitchText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 1, TEXT("{:.2f}"), [](const auto& P) { const auto R = UCk_Utils_IsmProxy_UE::Get_LocalRotationOffset(P); return FVector{R.Roll, R.Pitch, R.Yaw}; }) : TEXT("--"); }
auto SCkInspector_IsmProxyAuthored::Get_RotationYawText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 2, TEXT("{:.2f}"), [](const auto& P) { const auto R = UCk_Utils_IsmProxy_UE::Get_LocalRotationOffset(P); return FVector{R.Roll, R.Pitch, R.Yaw}; }) : TEXT("--"); }
auto SCkInspector_IsmProxyAuthored::Get_ScaleXText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 0, TEXT("{:.2f}"), [](const auto& P) { return UCk_Utils_IsmProxy_UE::Get_ScaleMultiplier(P); }) : TEXT("--"); }
auto SCkInspector_IsmProxyAuthored::Get_ScaleYText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 1, TEXT("{:.2f}"), [](const auto& P) { return UCk_Utils_IsmProxy_UE::Get_ScaleMultiplier(P); }) : TEXT("--"); }
auto SCkInspector_IsmProxyAuthored::Get_ScaleZText() const -> FString
{ return _Active ? FormatVectorComponent(_Entity, 2, TEXT("{:.2f}"), [](const auto& P) { return UCk_Utils_IsmProxy_UE::Get_ScaleMultiplier(P); }) : TEXT("--"); }

auto SCkInspector_IsmProxyAuthored::Get_IsEnabled() const -> bool
{
    return Get_IsAvailable() && NOT _Entity.Has<ck::FTag_IsmProxy_Disabled>();
}

auto SCkInspector_IsmProxyAuthored::Get_CanEdit() const -> bool
{
    auto Proxy = FCk_Handle_IsmProxy{};
    return _Active && ck_inspector_ism_proxy::TryGetProxy(_Entity, Proxy)
        && ck_inspector_ism_proxy::GetGate(_Entity).IsEnabled;
}

auto SCkInspector_IsmProxyAuthored::Get_CanEditCustomValue() const -> bool
{
    auto Proxy = FCk_Handle_IsmProxy{};
    return Get_CanEdit()
        && ck_inspector_ism_proxy::TryGetProxy(_Entity, Proxy)
        && UCk_Utils_IsmProxy_UE::Get_CustomInstanceData(Proxy).IsValidIndex(_CustomDataIndex);
}

auto SCkInspector_IsmProxyAuthored::Get_EditDisabledReason() const -> FString
{
    if (NOT _Active)
    { return {}; }
    const auto Verdict = ck_inspector_ism_proxy::GetGate(_Entity);
    return Verdict.Reason.ToString();
}

auto SCkInspector_IsmProxyAuthored::Get_CustomDataValueDisabledReason() const -> FString
{
    if (NOT _Active)
    { return {}; }
    const auto Verdict = ck_inspector_ism_proxy::GetGate(_Entity);
    if (NOT Verdict.IsEnabled)
    { return Verdict.Reason.ToString(); }
    return Get_CanEditCustomValue()
        ? FString{} : FString{TEXT("Custom data index is outside the proxy's configured data range.")};
}

auto SCkInspector_IsmProxyAuthored::Get_CustomDataIndex() const -> float
{
    return static_cast<float>(_CustomDataIndex);
}

auto SCkInspector_IsmProxyAuthored::Get_CustomDataValue() const -> float
{
    auto Proxy = FCk_Handle_IsmProxy{};
    if (NOT _Active || NOT ck_inspector_ism_proxy::TryGetProxy(_Entity, Proxy))
    { return 0.0f; }
    const TArray<float> Data = UCk_Utils_IsmProxy_UE::Get_CustomInstanceData(Proxy);
    return Data.IsValidIndex(_CustomDataIndex) ? Data[_CustomDataIndex] : 0.0f;
}

auto SCkInspector_IsmProxyAuthored::Set_Enabled(const bool InIsEnabled) -> void
{
    auto Proxy = FCk_Handle_IsmProxy{};
    if (NOT _Active || NOT ck_inspector_ism_proxy::TryGetProxy(_Entity, Proxy)
        || NOT ck_inspector_ism_proxy::GetGate(_Entity).IsEnabled)
    { return; }
    UCk_Utils_IsmProxy_UE::Request_EnableDisable(Proxy,
        FCk_Request_IsmProxy_EnableDisable{
            InIsEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable}, {});
}

auto SCkInspector_IsmProxyAuthored::Commit_CustomDataIndex(const float InValue) -> void
{
    if (NOT _Active || NOT ck_inspector_ism_proxy::GetGate(_Entity).IsEnabled)
    { return; }
    _CustomDataIndex = FMath::Max(0, FMath::RoundToInt(InValue));
}

auto SCkInspector_IsmProxyAuthored::Commit_CustomDataValue(const float InValue) -> void
{
    auto Proxy = FCk_Handle_IsmProxy{};
    if (NOT _Active || NOT Get_CanEditCustomValue()
        || NOT ck_inspector_ism_proxy::TryGetProxy(_Entity, Proxy))
    { return; }
    UCk_Utils_IsmProxy_UE::Request_SetCustomInstanceDataValue(Proxy,
        FCk_Request_IsmProxy_SetCustomInstanceDataValue{_CustomDataIndex, InValue}, {});
}

auto SCkInspector_IsmProxyAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_IsmProxyAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& InName,
        FString (SCkInspector_IsmProxyAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([Weak, InGetter]()
        {
            const auto W = Weak.Pin();
            return W.IsValid() && NOT W->Is_Inert()
                ? FText::FromString((W.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("ism-proxy-mesh"), &SCkInspector_IsmProxyAuthored::Get_MeshText);
    BindText(TEXT("ism-proxy-mobility"), &SCkInspector_IsmProxyAuthored::Get_MobilityText);
    BindText(TEXT("ism-proxy-location-x"), &SCkInspector_IsmProxyAuthored::Get_LocationXText);
    BindText(TEXT("ism-proxy-location-y"), &SCkInspector_IsmProxyAuthored::Get_LocationYText);
    BindText(TEXT("ism-proxy-location-z"), &SCkInspector_IsmProxyAuthored::Get_LocationZText);
    BindText(TEXT("ism-proxy-rotation-roll"), &SCkInspector_IsmProxyAuthored::Get_RotationRollText);
    BindText(TEXT("ism-proxy-rotation-pitch"), &SCkInspector_IsmProxyAuthored::Get_RotationPitchText);
    BindText(TEXT("ism-proxy-rotation-yaw"), &SCkInspector_IsmProxyAuthored::Get_RotationYawText);
    BindText(TEXT("ism-proxy-scale-x"), &SCkInspector_IsmProxyAuthored::Get_ScaleXText);
    BindText(TEXT("ism-proxy-scale-y"), &SCkInspector_IsmProxyAuthored::Get_ScaleYText);
    BindText(TEXT("ism-proxy-scale-z"), &SCkInspector_IsmProxyAuthored::Get_ScaleZText);
    BindText(TEXT("ism-proxy-edit-tooltip"), &SCkInspector_IsmProxyAuthored::Get_EditDisabledReason);
    BindText(TEXT("ism-proxy-custom-value-tooltip"),
        &SCkInspector_IsmProxyAuthored::Get_CustomDataValueDisabledReason);

    Data.Visibility.Add(TEXT("ism-proxy-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto W = Weak.Pin(); return W.IsValid() && W->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("ism-proxy-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto W = Weak.Pin(); return NOT W.IsValid() || NOT W->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("ism-proxy-enabled"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto W = Weak.Pin(); return W.IsValid() && W->Get_IsEnabled(); }));
    Data.Visibility.Add(TEXT("ism-proxy-can-edit"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto W = Weak.Pin(); return W.IsValid() && W->Get_CanEdit(); }));
    Data.Visibility.Add(TEXT("ism-proxy-can-edit-value"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto W = Weak.Pin(); return W.IsValid() && W->Get_CanEditCustomValue(); }));
    Data.BoolChanged.Add(TEXT("ism-proxy-enabled-changed"), FCkUiOnBoolChanged::CreateLambda(
        [Weak](const bool bValue) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Set_Enabled(bValue); } }));
    Data.Number.Add(TEXT("ism-proxy-custom-index"), TAttribute<float>::CreateLambda([Weak]()
    { const auto W = Weak.Pin(); return W.IsValid() ? W->Get_CustomDataIndex() : 0.0f; }));
    Data.Number.Add(TEXT("ism-proxy-custom-value"), TAttribute<float>::CreateLambda([Weak]()
    { const auto W = Weak.Pin(); return W.IsValid() ? W->Get_CustomDataValue() : 0.0f; }));
    Data.NumberCommitted.Add(TEXT("ism-proxy-custom-index-committed"), FCkUiOnNumberCommitted::CreateLambda(
        [Weak](const float Value, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Commit_CustomDataIndex(Value); } }));
    Data.NumberCommitted.Add(TEXT("ism-proxy-custom-value-committed"), FCkUiOnNumberCommitted::CreateLambda(
        [Weak](const float Value, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Commit_CustomDataValue(Value); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto W = Weak.Pin(); return W.IsValid() && W->Get_IsAvailable(); });

    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("mesh"), TEXT("Mesh:")}, {TEXT("mobility"), TEXT("Mobility:")},
        {TEXT("location"), TEXT("Location Offset:")}, {TEXT("rotation"), TEXT("Rotation Offset (R,P,Y):")},
        {TEXT("scale"), TEXT("Scale Multiplier:")}, {TEXT("enabled"), TEXT("Enabled:")},
        {TEXT("custom-data"), TEXT("Custom Data:")}, {TEXT("index"), TEXT("  Index:")},
        {TEXT("value"), TEXT("  Value:")}})
    {
        Data.Color.Add(TEXT("ism-proxy-") + Pair.Key + TEXT("-diff-color"),
            TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]()
            { const auto W = Weak.Pin(); return W.IsValid() ? ck_inspector_ism_proxy::DiffColor(W->Is_DiffMarked(Label)) : FLinearColor::Transparent; }));
    }

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorIsmProxy.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorIsmProxy.ui.css")));
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

auto SCkInspector_IsmProxyAuthored::Tick(
    const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid())
    { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else
    { _LoadError.Reset(); }
}

auto SCkInspector_IsmProxyAuthored::Release() -> void
{
    if (NOT _Active)
    { return; }
    _Active = false;
    _Entity = {};
    _DiffLabels.Reset();
    _View.Reset();
    _Mounted = false;
}

// =====================================================================================================================

FCkInspector_IsmProxy::~FCkInspector_IsmProxy()
{
    OnDeactivated();
}

auto FCkInspector_IsmProxy::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("ISM Proxy"));
}

auto FCkInspector_IsmProxy::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Proxy = FCk_Handle_IsmProxy{};
    return ck_inspector_ism_proxy::TryGetProxy(Entity, Proxy);
}

auto FCkInspector_IsmProxy::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return NativeBody; }

    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : TArray<FString>{
        TEXT("Mesh:"), TEXT("Mobility:"), TEXT("Location Offset:"), TEXT("Rotation Offset (R,P,Y):"),
        TEXT("Scale Multiplier:"), TEXT("Enabled:"), TEXT("Custom Data:"), TEXT("  Index:"), TEXT("  Value:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }

    const TSharedRef<SCkInspector_IsmProxyAuthored> Authored =
        SNew(SCkInspector_IsmProxyAuthored).Entity(Entity).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_IsmProxy::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    Builder.SetEditGuard(Get_EditGuard());
    Builder.AddRow(FText::FromString(TEXT("Mesh:")), [Entity](const FCk_Handle&)
    {
        auto Proxy = FCk_Handle_IsmProxy{};
        if (NOT ck_inspector_ism_proxy::TryGetProxy(Entity, Proxy)) { return FText::FromString(TEXT("--")); }
        const UStaticMesh* const Mesh = UCk_Utils_IsmProxy_UE::Get_Mesh(Proxy);
        return FText::FromString(ck::IsValid(Mesh) ? Mesh->GetName() : TEXT("None"));
    }, CkStyle::Value_Object());
    Builder.AddRow(FText::FromString(TEXT("Mobility:")), [Entity](const FCk_Handle&)
    {
        auto Proxy = FCk_Handle_IsmProxy{};
        return FText::FromString(ck_inspector_ism_proxy::TryGetProxy(Entity, Proxy)
            ? ck_inspector_ism_proxy::GetMobilityText(UCk_Utils_IsmProxy_UE::Get_Mobility(Proxy)) : TEXT("--"));
    }, CkStyle::Value_Enum());
    Builder.AddAlignedNumericRow(FText::FromString(TEXT("Location Offset:")),
        ck_inspector_ism_proxy::Make_AxisComponents(Entity, TEXT("{:.1f}"),
            [](const auto& Proxy) { return UCk_Utils_IsmProxy_UE::Get_LocalLocationOffset(Proxy); }));
    Builder.AddAlignedNumericRow(FText::FromString(TEXT("Rotation Offset (R,P,Y):")),
        ck_inspector_ism_proxy::Make_AxisComponents(Entity, TEXT("{:.2f}"), [](const auto& Proxy)
        { const auto R = UCk_Utils_IsmProxy_UE::Get_LocalRotationOffset(Proxy); return FVector{R.Roll, R.Pitch, R.Yaw}; }));
    Builder.AddAlignedNumericRow(FText::FromString(TEXT("Scale Multiplier:")),
        ck_inspector_ism_proxy::Make_AxisComponents(Entity, TEXT("{:.2f}"),
            [](const auto& Proxy) { return UCk_Utils_IsmProxy_UE::Get_ScaleMultiplier(Proxy); }));

    Builder.AddHeader(FText::FromString(TEXT("Controls")));
    Builder.AddToggleRow(FText::FromString(TEXT("Enabled:")),
        TAttribute<bool>::CreateLambda([Entity]()
        {
            auto Proxy = FCk_Handle_IsmProxy{};
            return ck_inspector_ism_proxy::TryGetProxy(Entity, Proxy)
                && NOT Entity.Has<ck::FTag_IsmProxy_Disabled>();
        }),
        [Entity](const bool bEnabled)
        {
            auto Proxy = FCk_Handle_IsmProxy{};
            if (ck_inspector_ism_proxy::TryGetProxy(Entity, Proxy)
                && ck_inspector_ism_proxy::GetGate(Entity).IsEnabled)
            {
                UCk_Utils_IsmProxy_UE::Request_EnableDisable(Proxy,
                    FCk_Request_IsmProxy_EnableDisable{
                        bEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable}, {});
            }
        }, ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddRow(FText::FromString(TEXT("Custom Data:")), [](const FCk_Handle&)
    { return FText::FromString(TEXT("post-setup writes only reach the GPU on Movable proxies")); }, CkStyle::TextDim());
    const TSharedRef<int32> CustomDataIndex = MakeShared<int32>(0);
    Builder.AddIntegerRow(FText::FromString(TEXT("  Index:")),
        TAttribute<int32>::CreateLambda([CustomDataIndex]() { return *CustomDataIndex; }),
        [CustomDataIndex](const int32 Index) { *CustomDataIndex = FMath::Max(0, Index); },
        0, {}, ECk_DebugRequest_Requirement::CosmeticOnly);
    Builder.AddNumericRow(FText::FromString(TEXT("  Value:")),
        TAttribute<float>::CreateLambda([Entity, CustomDataIndex]()
        {
            auto Proxy = FCk_Handle_IsmProxy{};
            if (NOT ck_inspector_ism_proxy::TryGetProxy(Entity, Proxy)) { return 0.0f; }
            const auto Data = UCk_Utils_IsmProxy_UE::Get_CustomInstanceData(Proxy);
            return Data.IsValidIndex(*CustomDataIndex) ? Data[*CustomDataIndex] : 0.0f;
        }),
        [Entity, CustomDataIndex](const float Value)
        {
            auto Proxy = FCk_Handle_IsmProxy{};
            if (NOT ck_inspector_ism_proxy::TryGetProxy(Entity, Proxy)
                || NOT ck_inspector_ism_proxy::GetGate(Entity).IsEnabled
                || NOT UCk_Utils_IsmProxy_UE::Get_CustomInstanceData(Proxy).IsValidIndex(*CustomDataIndex))
            { return; }
            UCk_Utils_IsmProxy_UE::Request_SetCustomInstanceDataValue(Proxy,
                FCk_Request_IsmProxy_SetCustomInstanceDataValue{*CustomDataIndex, Value}, {});
        }, {}, {}, ECk_DebugRequest_Requirement::CosmeticOnly);
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_IsmProxy::Tick(const FCk_Handle&, float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_IsmProxyAuthored>& Weak)
    { const auto Instance = Weak.Pin(); return NOT Instance.IsValid() || Instance->Is_Inert(); });
    _LastAuthoredLoadError.Reset();
    for (const auto& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        { _LastAuthoredLoadError = Instance->Get_LoadError(); break; }
    }
}

auto FCkInspector_IsmProxy::OnDeactivated() -> void
{
    for (const auto& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
    _LastAuthoredLoadError.Reset();
}

// =====================================================================================================================
