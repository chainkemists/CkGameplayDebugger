#include "CkInspector_Network.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Net/CkNet_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Network)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_inspector_network
{
    // Tone mapping — "who is in charge here?" reads at a glance:
    //   Host / Authority  = this instance owns the entity            -> Ok
    //   ClientAndHost     = listen server, both hats                 -> Accent (the notable case)
    //   Client / Proxy    = following someone else's authority       -> Info
    //   Unknown / None    = not resolved / not networked             -> Neutral
    static auto Get_NetModeTone(
        ECk_Net_NetModeType InNetMode)
        -> ECk_Tone
    {
        switch (InNetMode)
        {
            case ECk_Net_NetModeType::Host:          return ECk_Tone::Ok;
            case ECk_Net_NetModeType::ClientAndHost: return ECk_Tone::Accent;
            case ECk_Net_NetModeType::Client:        return ECk_Tone::Info;
            case ECk_Net_NetModeType::Unknown:
            default:                                 return ECk_Tone::Neutral;
        }
    }

    static auto Get_NetRoleTone(
        ECk_Net_EntityNetRole InNetRole)
        -> ECk_Tone
    {
        switch (InNetRole)
        {
            case ECk_Net_EntityNetRole::Authority: return ECk_Tone::Ok;
            case ECk_Net_EntityNetRole::Proxy:     return ECk_Tone::Info;
            case ECk_Net_EntityNetRole::None:
            default:                               return ECk_Tone::Neutral;
        }
    }

    auto Build_NetModeText(const FCk_Handle& InEntity) -> FString
    {
        return ck::IsValid(InEntity)
            ? ck::Format_UE(TEXT("{}"), UCk_Utils_Net_UE::Get_EntityNetMode(InEntity))
            : TEXT("--");
    }

    auto Build_NetRoleText(const FCk_Handle& InEntity) -> FString
    {
        return ck::IsValid(InEntity)
            ? ck::Format_UE(TEXT("{}"), UCk_Utils_Net_UE::Get_EntityNetRole(InEntity))
            : TEXT("--");
    }

    auto Build_NetModeTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        return ck::IsValid(InEntity)
            ? Get_NetModeTone(UCk_Utils_Net_UE::Get_EntityNetMode(InEntity))
            : ECk_Tone::Neutral;
    }

    auto Build_NetRoleTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        return ck::IsValid(InEntity)
            ? Get_NetRoleTone(UCk_Utils_Net_UE::Get_EntityNetRole(InEntity))
            : ECk_Tone::Neutral;
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkInspector_NetworkAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _NetModeDiffMarked = InArgs._NetModeDiffMarked;
    _NetRoleDiffMarked = InArgs._NetRoleDiffMarked;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_AuthoredView())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_NetworkAuthored::~SCkInspector_NetworkAuthored()
{
    Release();
}

auto SCkInspector_NetworkAuthored::Get_NetModeText() const -> FString
{
    return _Active ? ck_inspector_network::Build_NetModeText(_Entity) : FString{};
}

auto SCkInspector_NetworkAuthored::Get_NetRoleText() const -> FString
{
    return _Active ? ck_inspector_network::Build_NetRoleText(_Entity) : FString{};
}

auto SCkInspector_NetworkAuthored::Get_NetModeForeground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneColor(ck_inspector_network::Build_NetModeTone(_Entity)) : FLinearColor::Transparent;
}

auto SCkInspector_NetworkAuthored::Get_NetModeBackground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneDimColor(ck_inspector_network::Build_NetModeTone(_Entity)) : FLinearColor::Transparent;
}

auto SCkInspector_NetworkAuthored::Get_NetRoleForeground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneColor(ck_inspector_network::Build_NetRoleTone(_Entity)) : FLinearColor::Transparent;
}

auto SCkInspector_NetworkAuthored::Get_NetRoleBackground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneDimColor(ck_inspector_network::Build_NetRoleTone(_Entity)) : FLinearColor::Transparent;
}

auto SCkInspector_NetworkAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_NetworkAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    auto BindText = [&Data, WeakWidget](const FString& InName, FString (SCkInspector_NetworkAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_NetworkAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    auto BindColor = [&Data, WeakWidget](const FString& InName, FLinearColor (SCkInspector_NetworkAuthored::* InGetter)() const)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_NetworkAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert() ? (Widget.Get()->*InGetter)() : FLinearColor::Transparent;
        }));
    };
    BindText(TEXT("network-net-mode"), &SCkInspector_NetworkAuthored::Get_NetModeText);
    BindText(TEXT("network-net-role"), &SCkInspector_NetworkAuthored::Get_NetRoleText);
    BindColor(TEXT("network-net-mode-foreground"), &SCkInspector_NetworkAuthored::Get_NetModeForeground);
    BindColor(TEXT("network-net-mode-background"), &SCkInspector_NetworkAuthored::Get_NetModeBackground);
    BindColor(TEXT("network-net-role-foreground"), &SCkInspector_NetworkAuthored::Get_NetRoleForeground);
    BindColor(TEXT("network-net-role-background"), &SCkInspector_NetworkAuthored::Get_NetRoleBackground);
    auto BindDiffColor = [&Data, WeakWidget](const FString& InName, const bool SCkInspector_NetworkAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_NetworkAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_network::DiffColor(Widget.Get()->*InMember)
                : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("network-net-mode-diff-color"), &SCkInspector_NetworkAuthored::_NetModeDiffMarked);
    BindDiffColor(TEXT("network-net-role-diff-color"), &SCkInspector_NetworkAuthored::_NetRoleDiffMarked);

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorNetwork.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorNetwork.ui.css")));
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

auto SCkInspector_NetworkAuthored::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_NetworkAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = FCk_Handle{};
    _View.Reset();
    _Mounted = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspector_Network::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Network"));
}

auto FCkInspector_Network::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

FCkInspector_Network::~FCkInspector_Network()
{
    OnDeactivated();
}

auto FCkInspector_Network::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    // Captured by value and re-validated per read — same contract as every other inspector row
    // attribute (rows outlive nothing, but the entity can die under them mid-PIE).
    const auto CapturedEntity = Entity;

    return FCkInspectorWidgetBuilder()
        .AddStatusPillRow(
            FText::FromString(TEXT("NetMode:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                return FText::FromString(ck_inspector_network::Build_NetModeText(CapturedEntity));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                return ck_inspector_network::Build_NetModeTone(CapturedEntity);
            }))
        .AddStatusPillRow(
            FText::FromString(TEXT("NetRole:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                return FText::FromString(ck_inspector_network::Build_NetRoleText(CapturedEntity));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                return ck_inspector_network::Build_NetRoleTone(CapturedEntity);
            }))
        .Build(Entity);
}

auto FCkInspector_Network::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_NetworkAuthored> Authored = SNew(SCkInspector_NetworkAuthored)
        .Entity(Entity)
        .NetModeDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("NetMode:")))
        .NetRoleDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("NetRole:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Network::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_NetworkAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_Network::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_NetworkAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_NetworkAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
