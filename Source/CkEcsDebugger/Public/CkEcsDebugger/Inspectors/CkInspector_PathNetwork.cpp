#include "CkInspector_PathNetwork.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"

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

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_PathNetwork)

namespace ck_inspector_pathnetwork
{
    auto TryGetNetwork(const FCk_Handle& InEntity, FCk_Handle_PathNetwork& OutNetwork) -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_PathNetwork_UE::Has(InEntity)) { return false; }
        auto Mutable = InEntity;
        OutNetwork = UCk_Utils_PathNetwork_UE::Cast(Mutable);
        return ck::IsValid(OutNetwork);
    }

    auto Build_BuiltText(const FCk_Handle& InEntity) -> FString
    {
        auto Network = FCk_Handle_PathNetwork{};
        if (NOT TryGetNetwork(InEntity, Network)) { return TEXT("--"); }
        return UCk_Utils_PathNetwork_UE::Get_IsBuilt(Network) ? TEXT("Yes") : TEXT("No");
    }

    auto Build_CountText(const FCk_Handle& InEntity, int32 (*InGetter)(const FCk_Handle_PathNetwork&)) -> FString
    {
        auto Network = FCk_Handle_PathNetwork{};
        return TryGetNetwork(InEntity, Network) ? LexToString(InGetter(Network)) : TEXT("--");
    }

    auto Build_CountValue(const FCk_Handle& InEntity, int32 (*InGetter)(const FCk_Handle_PathNetwork&)) -> int32
    {
        auto Network = FCk_Handle_PathNetwork{};
        return TryGetNetwork(InEntity, Network) ? InGetter(Network) : 0;
    }

    auto Build_EpochText(const FCk_Handle& InEntity) -> FString
    {
        auto Network = FCk_Handle_PathNetwork{};
        return TryGetNetwork(InEntity, Network) ? LexToString(UCk_Utils_PathNetwork_UE::Get_BuildEpoch(Network)) : TEXT("--");
    }

    auto Build_BuiltTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        auto Network = FCk_Handle_PathNetwork{};
        if (NOT TryGetNetwork(InEntity, Network)) { return ECk_Tone::Neutral; }
        return UCk_Utils_PathNetwork_UE::Get_IsBuilt(Network) ? ECk_Tone::Ok : ECk_Tone::Warn;
    }

    auto Build_CountTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        return ck::IsValid(InEntity) && UCk_Utils_PathNetwork_UE::Has(InEntity) ? ECk_Tone::Info : ECk_Tone::Neutral;
    }

    auto Build_EpochTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        auto Network = FCk_Handle_PathNetwork{};
        if (NOT TryGetNetwork(InEntity, Network)) { return ECk_Tone::Neutral; }
        return UCk_Utils_PathNetwork_UE::Get_BuildEpoch(Network) > 0 ? ECk_Tone::Accent : ECk_Tone::Neutral;
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkInspector_PathNetworkAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _BuiltDiffMarked = InArgs._BuiltDiffMarked;
    _NodesDiffMarked = InArgs._NodesDiffMarked;
    _EdgesDiffMarked = InArgs._EdgesDiffMarked;
    _BuildEpochDiffMarked = InArgs._BuildEpochDiffMarked;

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

SCkInspector_PathNetworkAuthored::~SCkInspector_PathNetworkAuthored()
{
    Release();
}

auto SCkInspector_PathNetworkAuthored::Get_BuiltText() const -> FString { return _Active ? ck_inspector_pathnetwork::Build_BuiltText(_Entity) : FString{}; }
auto SCkInspector_PathNetworkAuthored::Get_NodesText() const -> FString { return _Active ? ck_inspector_pathnetwork::Build_CountText(_Entity, &UCk_Utils_PathNetwork_UE::Get_NumNodes) : FString{}; }
auto SCkInspector_PathNetworkAuthored::Get_EdgesText() const -> FString { return _Active ? ck_inspector_pathnetwork::Build_CountText(_Entity, &UCk_Utils_PathNetwork_UE::Get_NumEdges) : FString{}; }
auto SCkInspector_PathNetworkAuthored::Get_BuildEpochText() const -> FString { return _Active ? ck_inspector_pathnetwork::Build_EpochText(_Entity) : FString{}; }
auto SCkInspector_PathNetworkAuthored::Get_BuiltForeground() const -> FLinearColor { return _Active ? CkStyle::GetToneColor(ck_inspector_pathnetwork::Build_BuiltTone(_Entity)) : FLinearColor::Transparent; }
auto SCkInspector_PathNetworkAuthored::Get_BuiltBackground() const -> FLinearColor { return _Active ? CkStyle::GetToneDimColor(ck_inspector_pathnetwork::Build_BuiltTone(_Entity)) : FLinearColor::Transparent; }
auto SCkInspector_PathNetworkAuthored::Get_NodesForeground() const -> FLinearColor { return _Active ? CkStyle::GetToneColor(ck_inspector_pathnetwork::Build_CountTone(_Entity)) : FLinearColor::Transparent; }
auto SCkInspector_PathNetworkAuthored::Get_NodesBackground() const -> FLinearColor { return _Active ? CkStyle::GetToneDimColor(ck_inspector_pathnetwork::Build_CountTone(_Entity)) : FLinearColor::Transparent; }
auto SCkInspector_PathNetworkAuthored::Get_EdgesForeground() const -> FLinearColor { return _Active ? CkStyle::GetToneColor(ck_inspector_pathnetwork::Build_CountTone(_Entity)) : FLinearColor::Transparent; }
auto SCkInspector_PathNetworkAuthored::Get_EdgesBackground() const -> FLinearColor { return _Active ? CkStyle::GetToneDimColor(ck_inspector_pathnetwork::Build_CountTone(_Entity)) : FLinearColor::Transparent; }
auto SCkInspector_PathNetworkAuthored::Get_BuildEpochForeground() const -> FLinearColor { return _Active ? CkStyle::GetToneColor(ck_inspector_pathnetwork::Build_EpochTone(_Entity)) : FLinearColor::Transparent; }

auto SCkInspector_PathNetworkAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_PathNetworkAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    auto BindText = [&Data, WeakWidget](const FString& InName, FString (SCkInspector_PathNetworkAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_PathNetworkAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    auto BindColor = [&Data, WeakWidget](const FString& InName, FLinearColor (SCkInspector_PathNetworkAuthored::* InGetter)() const)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_PathNetworkAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert() ? (Widget.Get()->*InGetter)() : FLinearColor::Transparent;
        }));
    };
    BindText(TEXT("path-network-built"), &SCkInspector_PathNetworkAuthored::Get_BuiltText);
    BindText(TEXT("path-network-nodes"), &SCkInspector_PathNetworkAuthored::Get_NodesText);
    BindText(TEXT("path-network-edges"), &SCkInspector_PathNetworkAuthored::Get_EdgesText);
    BindText(TEXT("path-network-build-epoch"), &SCkInspector_PathNetworkAuthored::Get_BuildEpochText);
    BindColor(TEXT("path-network-built-foreground"), &SCkInspector_PathNetworkAuthored::Get_BuiltForeground);
    BindColor(TEXT("path-network-built-background"), &SCkInspector_PathNetworkAuthored::Get_BuiltBackground);
    BindColor(TEXT("path-network-nodes-foreground"), &SCkInspector_PathNetworkAuthored::Get_NodesForeground);
    BindColor(TEXT("path-network-nodes-background"), &SCkInspector_PathNetworkAuthored::Get_NodesBackground);
    BindColor(TEXT("path-network-edges-foreground"), &SCkInspector_PathNetworkAuthored::Get_EdgesForeground);
    BindColor(TEXT("path-network-edges-background"), &SCkInspector_PathNetworkAuthored::Get_EdgesBackground);
    BindColor(TEXT("path-network-build-epoch-foreground"), &SCkInspector_PathNetworkAuthored::Get_BuildEpochForeground);
    auto BindDiffColor = [&Data, WeakWidget](const FString& InName, const bool SCkInspector_PathNetworkAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_PathNetworkAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert() ? ck_inspector_pathnetwork::DiffColor(Widget.Get()->*InMember) : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("path-network-built-diff-color"), &SCkInspector_PathNetworkAuthored::_BuiltDiffMarked);
    BindDiffColor(TEXT("path-network-nodes-diff-color"), &SCkInspector_PathNetworkAuthored::_NodesDiffMarked);
    BindDiffColor(TEXT("path-network-edges-diff-color"), &SCkInspector_PathNetworkAuthored::_EdgesDiffMarked);
    BindDiffColor(TEXT("path-network-build-epoch-diff-color"), &SCkInspector_PathNetworkAuthored::_BuildEpochDiffMarked);

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create({}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorPathNetwork.ui.html")), FPaths::Combine(ResourceRoot, TEXT("EcsInspectorPathNetwork.ui.css")));
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

auto SCkInspector_PathNetworkAuthored::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_PathNetworkAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = FCk_Handle{};
    _View.Reset();
    _Mounted = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspector_PathNetwork::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Path Network"));
}

auto FCkInspector_PathNetwork::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_PathNetwork_UE::Has(Entity);
}

FCkInspector_PathNetwork::~FCkInspector_PathNetwork()
{
    OnDeactivated();
}

auto FCkInspector_PathNetwork::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    const auto CapturedEntity = Entity;

    // An unbuilt network routes nothing, so "No" warns rather than reading as a neutral mode.
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Built:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            return FText::FromString(ck_inspector_pathnetwork::Build_BuiltText(CapturedEntity));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            return ck_inspector_pathnetwork::Build_BuiltTone(CapturedEntity);
        }));

    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Nodes:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        {
            return ck_inspector_pathnetwork::Build_CountValue(CapturedEntity, &UCk_Utils_PathNetwork_UE::Get_NumNodes);
        }),
        ECk_Tone::Info);

    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Edges:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        {
            return ck_inspector_pathnetwork::Build_CountValue(CapturedEntity, &UCk_Utils_PathNetwork_UE::Get_NumEdges);
        }),
        ECk_Tone::Info);

    Builder.AddRow(
        FText::FromString(TEXT("Build Epoch:")),
        [CapturedEntity](const FCk_Handle&)
        {
            return FText::FromString(ck_inspector_pathnetwork::Build_EpochText(CapturedEntity));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

auto FCkInspector_PathNetwork::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_PathNetworkAuthored> Authored = SNew(SCkInspector_PathNetworkAuthored)
        .Entity(Entity)
        .BuiltDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Built:")))
        .NodesDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Nodes:")))
        .EdgesDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Edges:")))
        .BuildEpochDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Build Epoch:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_PathNetwork::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_PathNetworkAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_PathNetwork::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_PathNetworkAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_PathNetworkAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
