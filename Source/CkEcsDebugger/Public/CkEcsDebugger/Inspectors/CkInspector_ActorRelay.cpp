#include "CkInspector_ActorRelay.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkActorRelay/CkActorRelay_Actor.h"
#include "CkActorRelay/CkActorRelay_GroupSubsystem.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
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

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ActorRelay)

// =====================================================================================================================

namespace ck_inspector_actor_relay
{
    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

    auto Get_RelayActor(const FCk_Handle& InEntity) -> ACk_ActorRelay_UE*
    {
        if (IsDestroying(InEntity))
        { return nullptr; }

        auto* const OwningActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(InEntity);
        return ck::IsValid(OwningActor) ? Cast<ACk_ActorRelay_UE>(OwningActor) : nullptr;
    }

    auto DiffColor(const bool bInDiffMarked) -> FLinearColor
    {
        return bInDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// =====================================================================================================================

auto SCkInspector_ActorRelayAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _RelayActor = ck_inspector_actor_relay::Get_RelayActor(_Entity);
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

SCkInspector_ActorRelayAuthored::~SCkInspector_ActorRelayAuthored()
{
    Release();
}

auto SCkInspector_ActorRelayAuthored::Get_RelayActor() const -> ACk_ActorRelay_UE*
{
    if (NOT _Active || ck_inspector_actor_relay::Get_RelayActor(_Entity) != _RelayActor.Get())
    { return nullptr; }
    return _RelayActor.Get();
}

auto SCkInspector_ActorRelayAuthored::Get_IsAvailable() const -> bool
{
    return Get_RelayActor() != nullptr;
}

auto SCkInspector_ActorRelayAuthored::Get_ClassText() const -> FString
{
    const auto* const Relay = Get_RelayActor();
    return Relay != nullptr ? Relay->GetClass()->GetName() : TEXT("--");
}

auto SCkInspector_ActorRelayAuthored::Get_ActorText() const -> FString
{
    const auto* const Relay = Get_RelayActor();
    return Relay != nullptr ? Relay->GetName() : TEXT("--");
}

auto SCkInspector_ActorRelayAuthored::Get_GroupTagText() const -> FString
{
    const auto* const Relay = Get_RelayActor();
    if (Relay == nullptr)
    { return TEXT("--"); }
    const auto Group = Relay->Get_GroupSubsystem();
    if (NOT Group.IsValid())
    { return TEXT("<unregistered>"); }
    const FGameplayTag GroupTag = Group->Get_GroupTag();
    return GroupTag.IsValid() ? GroupTag.ToString() : TEXT("None");
}

auto SCkInspector_ActorRelayAuthored::Get_OwnershipText() const -> FString
{
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    return Group.IsValid() ? ck::Format_UE(TEXT("{}"), Group->Get_OwnershipPolicy()) : TEXT("--");
}

auto SCkInspector_ActorRelayAuthored::Get_SelectionText() const -> FString
{
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    return Group.IsValid() ? ck::Format_UE(TEXT("{}"), Group->Get_SelectionAlgorithm()) : TEXT("--");
}

auto SCkInspector_ActorRelayAuthored::Get_DisconnectText() const -> FString
{
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    return Group.IsValid() ? ck::Format_UE(TEXT("{}"), Group->Get_DisconnectPolicy()) : TEXT("--");
}

auto SCkInspector_ActorRelayAuthored::Get_ChannelFraction() const -> float
{
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    if (NOT Group.IsValid() || Group->Get_ChannelCount() <= 0)
    { return 0.0f; }
    return static_cast<float>(Group->Get_ChannelCount_Active())
        / static_cast<float>(Group->Get_ChannelCount());
}

auto SCkInspector_ActorRelayAuthored::Get_ChannelText() const -> FString
{
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    return Group.IsValid()
        ? ck::Format_UE(TEXT("{} active / {} configured"),
            Group->Get_ChannelCount_Active(), Group->Get_ChannelCount())
        : TEXT("--");
}

auto SCkInspector_ActorRelayAuthored::Get_MaxEntitiesText() const -> FString
{
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    if (NOT Group.IsValid())
    { return TEXT("--"); }
    const int32 Maximum = Group->Get_MaxEntitiesPerChannel();
    return Maximum >= 0 ? ck::Format_UE(TEXT("{}"), Maximum) : TEXT("unlimited");
}

auto SCkInspector_ActorRelayAuthored::Get_HasChannelCapacity() const -> bool
{
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    return Group.IsValid() && Group->Get_MaxEntitiesPerChannel() > 0;
}

auto SCkInspector_ActorRelayAuthored::Get_EntitiesFraction() const -> float
{
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    const int32 Maximum = Group.IsValid() ? Group->Get_MaxEntitiesPerChannel() : -1;
    if (Maximum <= 0 || ck::Is_NOT_Valid(_Entity))
    { return 0.0f; }
    return static_cast<float>(UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(_Entity).Num())
        / static_cast<float>(Maximum);
}

auto SCkInspector_ActorRelayAuthored::Get_EntitiesText() const -> FString
{
    if (NOT Get_IsAvailable())
    { return TEXT("--"); }
    const int32 Count = UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(_Entity).Num();
    const auto* const Relay = Get_RelayActor();
    const auto Group = Relay != nullptr ? Relay->Get_GroupSubsystem() : nullptr;
    const int32 Maximum = Group.IsValid() ? Group->Get_MaxEntitiesPerChannel() : -1;
    return Maximum > 0 ? ck::Format_UE(TEXT("{} / {}"), Count, Maximum) : ck::Format_UE(TEXT("{}"), Count);
}

auto SCkInspector_ActorRelayAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_ActorRelayAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& InName,
        FString (SCkInspector_ActorRelayAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([Weak, InGetter]()
        {
            const auto Widget = Weak.Pin();
            return FText::FromString(Widget.IsValid() ? ((*Widget).*InGetter)() : FString{});
        }));
    };
    BindText(TEXT("actor-relay-class"), &SCkInspector_ActorRelayAuthored::Get_ClassText);
    BindText(TEXT("actor-relay-actor"), &SCkInspector_ActorRelayAuthored::Get_ActorText);
    BindText(TEXT("actor-relay-group-tag"), &SCkInspector_ActorRelayAuthored::Get_GroupTagText);
    BindText(TEXT("actor-relay-ownership"), &SCkInspector_ActorRelayAuthored::Get_OwnershipText);
    BindText(TEXT("actor-relay-selection"), &SCkInspector_ActorRelayAuthored::Get_SelectionText);
    BindText(TEXT("actor-relay-disconnect"), &SCkInspector_ActorRelayAuthored::Get_DisconnectText);
    BindText(TEXT("actor-relay-channels"), &SCkInspector_ActorRelayAuthored::Get_ChannelText);
    BindText(TEXT("actor-relay-max-entities"), &SCkInspector_ActorRelayAuthored::Get_MaxEntitiesText);
    BindText(TEXT("actor-relay-entities"), &SCkInspector_ActorRelayAuthored::Get_EntitiesText);

    Data.Visibility.Add(TEXT("actor-relay-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("actor-relay-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("actor-relay-has-channel-capacity"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_HasChannelCapacity(); }));
    Data.Visibility.Add(TEXT("actor-relay-has-no-channel-capacity"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Get_HasChannelCapacity(); }));
    Data.Number.Add(TEXT("actor-relay-channels-fraction"), TAttribute<float>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_ChannelFraction() : 0.0f; }));
    Data.Number.Add(TEXT("actor-relay-entities-fraction"), TAttribute<float>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_EntitiesFraction() : 0.0f; }));
    Data.Color.Add(TEXT("actor-relay-meter-fill"), CkStyle::Accent());

    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("class"), TEXT("Class:")}, {TEXT("actor"), TEXT("Actor:")},
        {TEXT("group-tag"), TEXT("Group Tag:")}, {TEXT("ownership"), TEXT("Ownership:")},
        {TEXT("selection"), TEXT("Selection:")}, {TEXT("disconnect"), TEXT("Disconnect:")},
        {TEXT("channels"), TEXT("Channels:")}, {TEXT("max-entities"), TEXT("Max Entities/Ch:")},
        {TEXT("entities"), TEXT("Entities On Channel:")}})
    {
        Data.Color.Add(TEXT("actor-relay-") + Pair.Key + TEXT("-diff-color"),
            TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]()
            {
                const auto Widget = Weak.Pin();
                return Widget.IsValid()
                    ? ck_inspector_actor_relay::DiffColor(Widget->Is_DiffMarked(Label))
                    : FLinearColor::Transparent;
            }));
    }

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorActorRelay.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorActorRelay.ui.css")));
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

auto SCkInspector_ActorRelayAuthored::Tick(
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

auto SCkInspector_ActorRelayAuthored::Release() -> void
{
    if (NOT _Active)
    { return; }
    _Active = false;
    _Entity = {};
    _RelayActor.Reset();
    _DiffLabels.Reset();
    _View.Reset();
    _Mounted = false;
}

// =====================================================================================================================

FCkInspector_ActorRelay::~FCkInspector_ActorRelay()
{
    OnDeactivated();
}

auto FCkInspector_ActorRelay::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Actor Relay"));
}

auto FCkInspector_ActorRelay::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck_inspector_actor_relay::Get_RelayActor(Entity) != nullptr;
}

auto FCkInspector_ActorRelay::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return NativeBody; }

    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : TArray<FString>{
        TEXT("Class:"), TEXT("Actor:"), TEXT("Group Tag:"), TEXT("Ownership:"), TEXT("Selection:"),
        TEXT("Disconnect:"), TEXT("Channels:"), TEXT("Max Entities/Ch:"), TEXT("Entities On Channel:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }

    const TSharedRef<SCkInspector_ActorRelayAuthored> Authored =
        SNew(SCkInspector_ActorRelayAuthored).Entity(Entity).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_ActorRelay::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    auto* const RelayActor = ck_inspector_actor_relay::Get_RelayActor(Entity);
    if (RelayActor == nullptr)
    { return Builder.Build(Entity, FString{}); }

    const auto CapturedEntity = Entity;
    const auto CapturedActor = TWeakObjectPtr<ACk_ActorRelay_UE>{RelayActor};
    Builder.AddRow(FText::FromString(TEXT("Class:")), [CapturedActor](const FCk_Handle&)
    {
        const auto* const Actor = CapturedActor.Get();
        return FText::FromString(Actor != nullptr ? Actor->GetClass()->GetName() : TEXT("--"));
    }, CkStyle::Value_Object());
    Builder.AddRow(FText::FromString(TEXT("Actor:")), [CapturedActor](const FCk_Handle&)
    {
        const auto* const Actor = CapturedActor.Get();
        return FText::FromString(Actor != nullptr ? Actor->GetName() : TEXT("--"));
    }, CkStyle::Value_String());
    Builder.AddRow(FText::FromString(TEXT("Group Tag:")), [CapturedActor](const FCk_Handle&)
    {
        const auto* const Actor = CapturedActor.Get();
        if (Actor == nullptr) { return FText::FromString(TEXT("--")); }
        const auto Group = Actor->Get_GroupSubsystem();
        if (NOT Group.IsValid()) { return FText::FromString(TEXT("<unregistered>")); }
        const FGameplayTag Tag = Group->Get_GroupTag();
        return FText::FromString(Tag.IsValid() ? Tag.ToString() : TEXT("None"));
    }, CkStyle::Value_Tag());
    Builder.AddRow(FText::FromString(TEXT("Ownership:")), [CapturedActor](const FCk_Handle&)
    {
        const auto* const Actor = CapturedActor.Get();
        const auto Group = Actor != nullptr ? Actor->Get_GroupSubsystem() : nullptr;
        return FText::FromString(Group.IsValid() ? ck::Format_UE(TEXT("{}"), Group->Get_OwnershipPolicy()) : TEXT("--"));
    }, CkStyle::Value_Enum());
    Builder.AddRow(FText::FromString(TEXT("Selection:")), [CapturedActor](const FCk_Handle&)
    {
        const auto* const Actor = CapturedActor.Get();
        const auto Group = Actor != nullptr ? Actor->Get_GroupSubsystem() : nullptr;
        return FText::FromString(Group.IsValid() ? ck::Format_UE(TEXT("{}"), Group->Get_SelectionAlgorithm()) : TEXT("--"));
    }, CkStyle::Value_Enum());
    Builder.AddRow(FText::FromString(TEXT("Disconnect:")), [CapturedActor](const FCk_Handle&)
    {
        const auto* const Actor = CapturedActor.Get();
        const auto Group = Actor != nullptr ? Actor->Get_GroupSubsystem() : nullptr;
        return FText::FromString(Group.IsValid() ? ck::Format_UE(TEXT("{}"), Group->Get_DisconnectPolicy()) : TEXT("--"));
    }, CkStyle::Value_Enum());
    Builder.AddMeterRow(FText::FromString(TEXT("Channels:")),
        TAttribute<float>::CreateLambda([CapturedActor]()
        {
            const auto* const Actor = CapturedActor.Get();
            const auto Group = Actor != nullptr ? Actor->Get_GroupSubsystem() : nullptr;
            return Group.IsValid() && Group->Get_ChannelCount() > 0
                ? static_cast<float>(Group->Get_ChannelCount_Active()) / static_cast<float>(Group->Get_ChannelCount())
                : 0.0f;
        }), ECk_Tone::Accent, TAttribute<FText>::CreateLambda([CapturedActor]()
        {
            const auto* const Actor = CapturedActor.Get();
            const auto Group = Actor != nullptr ? Actor->Get_GroupSubsystem() : nullptr;
            return FText::FromString(Group.IsValid()
                ? ck::Format_UE(TEXT("{} active / {} configured"), Group->Get_ChannelCount_Active(), Group->Get_ChannelCount())
                : TEXT("--"));
        }));
    Builder.AddRow(FText::FromString(TEXT("Max Entities/Ch:")), [CapturedActor](const FCk_Handle&)
    {
        const auto* const Actor = CapturedActor.Get();
        const auto Group = Actor != nullptr ? Actor->Get_GroupSubsystem() : nullptr;
        if (NOT Group.IsValid()) { return FText::FromString(TEXT("--")); }
        const int32 Maximum = Group->Get_MaxEntitiesPerChannel();
        return FText::FromString(Maximum >= 0 ? ck::Format_UE(TEXT("{}"), Maximum) : TEXT("unlimited"));
    }, CkStyle::Value_Numeric());

    const auto Group = RelayActor->Get_GroupSubsystem();
    const int32 Maximum = Group.IsValid() ? Group->Get_MaxEntitiesPerChannel() : -1;
    if (Maximum > 0)
    {
        Builder.AddMeterRow(FText::FromString(TEXT("Entities On Channel:")),
            TAttribute<float>::CreateLambda([CapturedEntity, Maximum]()
            {
                return ck::IsValid(CapturedEntity)
                    ? static_cast<float>(UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(CapturedEntity).Num())
                        / static_cast<float>(Maximum)
                    : 0.0f;
            }), ECk_Tone::Accent, TAttribute<FText>::CreateLambda([CapturedEntity, Maximum]()
            {
                return FText::FromString(ck::IsValid(CapturedEntity)
                    ? ck::Format_UE(TEXT("{} / {}"),
                        UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(CapturedEntity).Num(), Maximum)
                    : TEXT("--"));
            }));
    }
    else
    {
        Builder.AddRow(FText::FromString(TEXT("Entities On Channel:")), [CapturedEntity](const FCk_Handle&)
        {
            return FText::FromString(ck::IsValid(CapturedEntity)
                ? ck::Format_UE(TEXT("{}"), UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(CapturedEntity).Num())
                : TEXT("--"));
        }, CkStyle::Value_Numeric());
    }
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_ActorRelay::Tick(const FCk_Handle&, float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_ActorRelayAuthored>& Weak)
    { const auto Instance = Weak.Pin(); return NOT Instance.IsValid() || Instance->Is_Inert(); });
    _LastAuthoredLoadError.Reset();
    for (const auto& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        { _LastAuthoredLoadError = Instance->Get_LoadError(); break; }
    }
}

auto FCkInspector_ActorRelay::OnDeactivated() -> void
{
    for (const auto& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
    _LastAuthoredLoadError.Reset();
}

// =====================================================================================================================
