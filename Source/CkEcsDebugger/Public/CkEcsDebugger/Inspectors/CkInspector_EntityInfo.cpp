#include "CkInspector_EntityInfo.h"

#include "CkCore/Enums/CkEnums.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityInfo)

namespace ck_inspector_entity_info
{
    constexpr TCHAR IdPort[] = TEXT("entity-info-id-port");

    auto Build_NameText(const FCk_Handle& InEntity) -> FString
    {
        return ck::IsValid(InEntity) ? UCk_Utils_Handle_UE::Get_DebugName(InEntity).ToString() : FString{};
    }

    auto Build_ActorText(const FCk_Handle& InEntity) -> FString
    {
        if (ck::IsValid(InEntity) && UCk_Utils_OwningActor_UE::Has(InEntity))
        { return ck::Format_UE(TEXT("{}"), UCk_Utils_OwningActor_UE::Get_EntityOwningActor(InEntity)); }
        return TEXT("None");
    }

    auto Build_ActorColor(const FCk_Handle& InEntity) -> FLinearColor
    {
        return ck::IsValid(InEntity) && UCk_Utils_OwningActor_UE::Has(InEntity) ? CkStyle::Text() : CkStyle::None();
    }

    auto Describe_Entity(const FCk_Handle& InEntity) -> FString
    {
        return ck::IsValid(InEntity) ? InEntity.Get_Entity().ToString() : TEXT("Invalid");
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }

    auto Find_NameInput(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTag() == TEXT("entity-info-name-input")
            && InRoot->GetTypeAsString() == TEXT("SCkUiTextInputBox"))
        { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = Find_NameInput(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }
}

auto SCkInspector_EntityInfoAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _NameDiffMarked = InArgs._NameDiffMarked;
    _SetNameDiffMarked = InArgs._SetNameDiffMarked;
    _IdDiffMarked = InArgs._IdDiffMarked;
    _ActorDiffMarked = InArgs._ActorDiffMarked;
    _EditScope = MakeShared<FCkInspectorEditScope>(InArgs._EditGuard);

    _RootHost = SNew(SBox);
    ChildSlot[_RootHost.ToSharedRef()];
    if (Build_AuthoredView() && Populate_NativePorts())
    {
        _RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    _RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_EntityInfoAuthored::~SCkInspector_EntityInfoAuthored()
{
    Release();
}

auto SCkInspector_EntityInfoAuthored::Get_NameText() const -> FString
{
    return _Active ? ck_inspector_entity_info::Build_NameText(_Entity) : FString{};
}

auto SCkInspector_EntityInfoAuthored::Get_ActorText() const -> FString
{
    return _Active ? ck_inspector_entity_info::Build_ActorText(_Entity) : FString{};
}

auto SCkInspector_EntityInfoAuthored::Build_AuthoredView() -> bool
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

    const TSharedRef<SBox> IdPort = SNew(SBox);
    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(ck_inspector_entity_info::IdPort, IdPort);

    const TWeakPtr<SCkInspector_EntityInfoAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    auto BindText = [&Data, WeakWidget](const FString& InName, FString (*InGetter)(const FCk_Handle&))
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_EntityInfoAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString(InGetter(Widget->_Entity))
                : FText::GetEmpty();
        }));
    };
    BindText(TEXT("entity-info-name-value"), &ck_inspector_entity_info::Build_NameText);
    BindText(TEXT("entity-info-name-input"), &ck_inspector_entity_info::Build_NameText);
    BindText(TEXT("entity-info-actor-value"), &ck_inspector_entity_info::Build_ActorText);
    Data.TextChanged.Add(TEXT("entity-info-name-changed"), FOnTextChanged::CreateSP(
        SharedThis(this), &SCkInspector_EntityInfoAuthored::Handle_NameChanged));
    Data.TextCommitted.Add(TEXT("entity-info-name-committed"), FOnTextCommitted::CreateSP(
        SharedThis(this), &SCkInspector_EntityInfoAuthored::Handle_NameCommitted));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_EntityInfoAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert() && ck::IsValid(Widget->_Entity);
    });
    Data.Color.Add(TEXT("entity-info-actor-color"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_EntityInfoAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? ck_inspector_entity_info::Build_ActorColor(Widget->_Entity)
            : FLinearColor::Transparent;
    }));
    auto BindDiffColor = [&Data, WeakWidget](const FString& InName, const bool SCkInspector_EntityInfoAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_EntityInfoAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_entity_info::DiffColor(Widget.Get()->*InMember)
                : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("entity-info-name-diff-color"), &SCkInspector_EntityInfoAuthored::_NameDiffMarked);
    BindDiffColor(TEXT("entity-info-set-name-diff-color"), &SCkInspector_EntityInfoAuthored::_SetNameDiffMarked);
    BindDiffColor(TEXT("entity-info-id-diff-color"), &SCkInspector_EntityInfoAuthored::_IdDiffMarked);
    BindDiffColor(TEXT("entity-info-actor-diff-color"), &SCkInspector_EntityInfoAuthored::_ActorDiffMarked);

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityInfo.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityInfo.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }

    _View = Candidate;
    _IdPort = IdPort;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_EntityInfoAuthored::Populate_NativePorts() -> bool
{
    if (NOT _Active || NOT _IdPort.IsValid()) { return false; }
    _IdPort->SetContent(SNew(SCkDebug_EntityRef).Entity(_Entity));
    _NameInput = _View.IsValid() ? ck_inspector_entity_info::Find_NameInput(_View->GetRegion(TEXT("main"))) : nullptr;
    return _NameInput.IsValid();
}

auto SCkInspector_EntityInfoAuthored::Handle_NameChanged(const FText& InText) -> void
{
    if (_Active && ck::IsValid(_Entity) && _EditScope.IsValid()) { _EditScope->Set_Active(true); }
}

auto SCkInspector_EntityInfoAuthored::Handle_NameCommitted(const FText& InText, ETextCommit::Type) -> void
{
    if (_EditScope.IsValid()) { _EditScope->Set_Active(false); }
    if (NOT _Active || ck::Is_NOT_Valid(_Entity)) { return; }
    const FString Trimmed = InText.ToString().TrimStartAndEnd();
    const FName DebugName{*Trimmed};
    if (DebugName.IsNone()) { return; }
    auto MutableEntity = _Entity;
    UCk_Utils_Handle_UE::Set_DebugName(MutableEntity, DebugName, ECk_Override::Override);
}

auto SCkInspector_EntityInfoAuthored::Detach_NativePorts() -> void
{
    if (_IdPort.IsValid()) { _IdPort->SetContent(SNullWidget::NullWidget); }
}

auto SCkInspector_EntityInfoAuthored::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    if (_EditScope.IsValid() && _EditScope->Get_IsActive() && _NameInput.IsValid()
        && _NameInput->GetText().ToString() == ck_inspector_entity_info::Build_NameText(_Entity))
    { _EditScope->Set_Active(false); }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_EntityInfoAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    if (_EditScope.IsValid()) { _EditScope->Set_Active(false); }
    Detach_NativePorts();
    _Entity = FCk_Handle{};
    _View.Reset();
    _IdPort.Reset();
    _NameInput.Reset();
    _EditScope.Reset();
    _Mounted = false;
}

FCkInspector_EntityInfo::~FCkInspector_EntityInfo()
{
    OnDeactivated();
}

auto FCkInspector_EntityInfo::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Entity Info"));
}

auto FCkInspector_EntityInfo::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_EntityInfo::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    const auto CapturedEntity = Entity;
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    return Builder
        .AddRow(FText::FromString(TEXT("Name:")),
            [](const FCk_Handle& E) { return FText::FromString(ck_inspector_entity_info::Build_NameText(E)); })
        .AddNameEntryRow(FText::FromString(TEXT("Set Name:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                return FText::FromString(ck_inspector_entity_info::Build_NameText(CapturedEntity));
            }),
            [CapturedEntity](FName InDebugName)
            {
                auto MutableEntity = CapturedEntity;
                if (ck::Is_NOT_Valid(MutableEntity) || InDebugName.IsNone()) { return; }
                UCk_Utils_Handle_UE::Set_DebugName(MutableEntity, InDebugName, ECk_Override::Override);
            })
        .AddWidgetRow(FText::FromString(TEXT("ID:")), SNew(SCkDebug_EntityRef).Entity(Entity),
            [CapturedEntity]() { return ck_inspector_entity_info::Describe_Entity(CapturedEntity); })
        .AddConditionalRow(FText::FromString(TEXT("Actor:")),
            [](const FCk_Handle& E) { return FText::FromString(ck_inspector_entity_info::Build_ActorText(E)); },
            [](const FCk_Handle& E) { return ck_inspector_entity_info::Build_ActorColor(E); })
        .Build(Entity);
}

auto FCkInspector_EntityInfo::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_EntityInfoAuthored> Authored = SNew(SCkInspector_EntityInfoAuthored)
        .Entity(Entity)
        .EditGuard(Get_EditGuard())
        .NameDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Name:")))
        .SetNameDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Set Name:")))
        .IdDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("ID:")))
        .ActorDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Actor:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_EntityInfo::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_EntityInfoAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_EntityInfo::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_EntityInfoAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_EntityInfoAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
