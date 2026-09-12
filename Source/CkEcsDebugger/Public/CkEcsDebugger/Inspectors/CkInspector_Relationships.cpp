#include "CkInspector_Relationships.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkRelationship/Team/CkTeam_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Relationships)

namespace ck_inspector_relationships
{
    constexpr TCHAR ContextOwnerPort[] = TEXT("relationships-context-owner-port");
    constexpr TCHAR LifetimeOwnerPort[] = TEXT("relationships-lifetime-owner-port");

    auto Build_TeamText(const FCk_Handle& InEntity) -> FString
    {
        if (const auto TeamEntity = UCk_Utils_Team_UE::Cast(InEntity); ck::IsValid(TeamEntity))
        { return ck::Format_UE(TEXT("{} (Starts from ZERO)"), UCk_Utils_Team_UE::Get_ID(TeamEntity)); }
        return TEXT("Unknown");
    }

    auto Build_TeamColor(const FCk_Handle& InEntity) -> FLinearColor
    {
        return ck::IsValid(UCk_Utils_Team_UE::Cast(InEntity)) ? CkStyle::Relationship() : CkStyle::Err();
    }

    auto Describe_Entity(const FCk_Handle& InEntity) -> FString
    {
        return ck::IsValid(InEntity) ? InEntity.Get_Entity().ToString() : TEXT("Invalid");
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkInspector_RelationshipsAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _TeamDiffMarked = InArgs._TeamDiffMarked;
    _ContextOwnerDiffMarked = InArgs._ContextOwnerDiffMarked;
    _LifetimeOwnerDiffMarked = InArgs._LifetimeOwnerDiffMarked;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_AuthoredView() && Populate_NativePorts())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_RelationshipsAuthored::~SCkInspector_RelationshipsAuthored()
{
    Release();
}

auto SCkInspector_RelationshipsAuthored::Get_TeamText() const -> FString
{
    return _Active ? ck_inspector_relationships::Build_TeamText(_Entity) : FString{};
}

auto SCkInspector_RelationshipsAuthored::Get_TeamColor() const -> FLinearColor
{
    return _Active ? ck_inspector_relationships::Build_TeamColor(_Entity) : FLinearColor::Transparent;
}

auto SCkInspector_RelationshipsAuthored::Build_AuthoredView() -> bool
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

    const TSharedRef<SBox> ContextOwnerPort = SNew(SBox);
    const TSharedRef<SBox> LifetimeOwnerPort = SNew(SBox);
    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(ck_inspector_relationships::ContextOwnerPort, ContextOwnerPort);
    NativeBindings.Add(ck_inspector_relationships::LifetimeOwnerPort, LifetimeOwnerPort);

    const TWeakPtr<SCkInspector_RelationshipsAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("relationships-team-value"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_RelationshipsAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? FText::FromString(ck_inspector_relationships::Build_TeamText(Widget->_Entity))
            : FText::GetEmpty();
    }));
    Data.Color.Add(TEXT("relationships-team-color"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_RelationshipsAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? ck_inspector_relationships::Build_TeamColor(Widget->_Entity)
            : FLinearColor::Transparent;
    }));
    auto BindDiffColor = [&Data, WeakWidget](const FString& InName, const bool SCkInspector_RelationshipsAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_RelationshipsAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_relationships::DiffColor(Widget.Get()->*InMember)
                : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("relationships-team-diff-color"), &SCkInspector_RelationshipsAuthored::_TeamDiffMarked);
    BindDiffColor(TEXT("relationships-context-owner-diff-color"), &SCkInspector_RelationshipsAuthored::_ContextOwnerDiffMarked);
    BindDiffColor(TEXT("relationships-lifetime-owner-diff-color"), &SCkInspector_RelationshipsAuthored::_LifetimeOwnerDiffMarked);

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorRelationships.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorRelationships.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }

    _View = Candidate;
    _ContextOwnerPort = ContextOwnerPort;
    _LifetimeOwnerPort = LifetimeOwnerPort;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_RelationshipsAuthored::Populate_NativePorts() -> bool
{
    if (NOT _Active || NOT _ContextOwnerPort.IsValid() || NOT _LifetimeOwnerPort.IsValid())
    { return false; }
    const auto ContextOwner = UCk_Utils_ContextOwner_UE::Has(_Entity)
        ? UCk_Utils_ContextOwner_UE::Get_ContextOwner(_Entity)
        : FCk_Handle{};
    const auto LifetimeOwner = _Entity.Has<ck::FFragment_LifetimeOwner>()
        ? FCk_Handle{_Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity()}
        : FCk_Handle{};
    _ContextOwnerPort->SetContent(SNew(SCkDebug_EntityRef).Entity(ContextOwner).ShowName(true));
    _LifetimeOwnerPort->SetContent(SNew(SCkDebug_EntityRef).Entity(LifetimeOwner).ShowName(true));
    return true;
}

auto SCkInspector_RelationshipsAuthored::Detach_NativePorts() -> void
{
    if (_ContextOwnerPort.IsValid()) { _ContextOwnerPort->SetContent(SNullWidget::NullWidget); }
    if (_LifetimeOwnerPort.IsValid()) { _LifetimeOwnerPort->SetContent(SNullWidget::NullWidget); }
}

auto SCkInspector_RelationshipsAuthored::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_RelationshipsAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    Detach_NativePorts();
    _Entity = FCk_Handle{};
    _View.Reset();
    _ContextOwnerPort.Reset();
    _LifetimeOwnerPort.Reset();
    _Mounted = false;
}

// --------------------------------------------------------------------------------------------------------------------

FCkInspector_Relationships::~FCkInspector_Relationships()
{
    OnDeactivated();
}

auto FCkInspector_Relationships::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Relationships"));
}

auto FCkInspector_Relationships::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_Relationships::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    const auto ContextOwner = UCk_Utils_ContextOwner_UE::Has(Entity)
        ? UCk_Utils_ContextOwner_UE::Get_ContextOwner(Entity)
        : FCk_Handle{};
    const auto LifetimeOwner = Entity.Has<ck::FFragment_LifetimeOwner>()
        ? FCk_Handle{Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity()}
        : FCk_Handle{};

    return FCkInspectorWidgetBuilder()
        .AddConditionalRow(FText::FromString(TEXT("Team:")),
            [](const FCk_Handle& E) { return FText::FromString(ck_inspector_relationships::Build_TeamText(E)); },
            [](const FCk_Handle& E) { return ck_inspector_relationships::Build_TeamColor(E); })
        .AddWidgetRow(FText::FromString(TEXT("Context Owner:")), SNew(SCkDebug_EntityRef).Entity(ContextOwner).ShowName(true),
            [ContextOwner]() { return ck_inspector_relationships::Describe_Entity(ContextOwner); })
        .AddWidgetRow(FText::FromString(TEXT("Lifetime Owner:")), SNew(SCkDebug_EntityRef).Entity(LifetimeOwner).ShowName(true),
            [LifetimeOwner]() { return ck_inspector_relationships::Describe_Entity(LifetimeOwner); })
        .Build(Entity);
}

auto FCkInspector_Relationships::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return NativeBody; }

    const TSharedRef<SCkInspector_RelationshipsAuthored> Authored = SNew(SCkInspector_RelationshipsAuthored)
        .Entity(Entity)
        .TeamDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Team:")))
        .ContextOwnerDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Context Owner:")))
        .LifetimeOwnerDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Lifetime Owner:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Relationships::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_RelationshipsAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_Relationships::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_RelationshipsAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_RelationshipsAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
