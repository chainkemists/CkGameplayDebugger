#include "CkInspector_InteractionResolver.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Fragment.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_InteractionResolver)

namespace ck_inspector_interaction_resolver
{
    auto TryGetResolver(const FCk_Handle& InEntity, FCk_Handle_InteractionResolver& OutResolver) -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_InteractionResolver_UE::Has(InEntity))
        { return false; }
        auto Mutable = InEntity;
        OutResolver = UCk_Utils_InteractionResolver_UE::CastChecked(Mutable);
        return ck::IsValid(OutResolver);
    }

    auto GetIntentName(const FGameplayTag& InIntent) -> FString
    { return InIntent.IsValid() ? InIntent.GetTagName().ToString() : TEXT("Unknown Intent"); }

    auto GetChannelName(const FGameplayTag& InChannel) -> FString
    { return InChannel.IsValid() ? InChannel.GetTagName().ToString() : TEXT("None"); }

    auto GetMappingKey(const int32 InIndex, const FGameplayTag& InIntent) -> FString
    { return ck::Format_UE(TEXT("mapping-{}|{}"), InIndex, GetIntentName(InIntent)); }

    auto GetChannelKey(const FString& InMappingKey, const int32 InIndex, const FGameplayTag& InChannel) -> FString
    { return ck::Format_UE(TEXT("{}/channel-{}|{}"), InMappingKey, InIndex, GetChannelName(InChannel)); }

    auto GetHandleKey(const FString& InPrefix, const FCk_Handle& InHandle) -> FString
    {
        return ck::IsValid(InHandle)
            ? ck::Format_UE(TEXT("{}/{}"), InPrefix, InHandle.Get_Entity())
            : FString{};
    }

    auto MakeInteractionResolverTextField(const FString& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    auto MakeInteractionResolverBoolField(const bool InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = InValue}; }

    auto AddTargetRecord(const FString& InKey, const FCk_Handle& InTarget, TArray<FCkUiRecordData>& OutRecords) -> void
    {
        if (InKey.IsEmpty() || ck::Is_NOT_Valid(InTarget))
        { return; }
        auto Record = FCkUiRecordData{};
        Record.Key = InKey;
        Record.Fields.Add(
            TEXT("target-id"), MakeInteractionResolverTextField(ck::Format_UE(TEXT("{}"), InTarget.Get_Entity())));
        Record.Fields.Add(TEXT("target-name"),
            MakeInteractionResolverTextField(UCk_Utils_Handle_UE::Get_DebugName(InTarget).ToString()));
        OutRecords.Add(MoveTemp(Record));
    }

    auto TryResolveIntent(const FCk_Handle& InEntity, const FString& InStableKey,
        FCk_Handle_InteractionResolver& OutResolver, FGameplayTag& OutIntent) -> bool
    {
        if (NOT TryGetResolver(InEntity, OutResolver))
        { return false; }
        const auto& Mappings = OutResolver.Get<ck::FFragment_InteractionResolver_Params>().Get_IntentChannelMappings();
        for (int32 Index = 0; Index < Mappings.Num(); ++Index)
        {
            const FGameplayTag& Intent = Mappings[Index].Get_Intent();
            if (Intent.IsValid() && GetMappingKey(Index, Intent) == InStableKey)
            { OutIntent = Intent; return true; }
        }
        return false;
    }

    auto TryResolveChannel(const FCk_Handle& InEntity, const FString& InStableKey,
        FCk_Handle_InteractionResolver& OutResolver, FGameplayTag& OutChannel) -> bool
    {
        if (NOT TryGetResolver(InEntity, OutResolver))
        { return false; }
        const auto& Mappings = OutResolver.Get<ck::FFragment_InteractionResolver_Params>().Get_IntentChannelMappings();
        for (int32 MappingIndex = 0; MappingIndex < Mappings.Num(); ++MappingIndex)
        {
            const FString MappingKey = GetMappingKey(MappingIndex, Mappings[MappingIndex].Get_Intent());
            const auto& Channels = Mappings[MappingIndex].Get_Channels();
            for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ++ChannelIndex)
            {
                if (Channels[ChannelIndex].IsValid()
                    && GetChannelKey(MappingKey, ChannelIndex, Channels[ChannelIndex]) == InStableKey)
                { OutChannel = Channels[ChannelIndex]; return true; }
            }
        }
        return false;
    }

    auto GetActiveIntentsText(const FCk_Handle& InEntity) -> FString
    {
        auto Resolver = FCk_Handle_InteractionResolver{};
        if (NOT TryGetResolver(InEntity, Resolver))
        { return TEXT("--"); }
        auto Names = TArray<FString>{};
        for (const FGameplayTag& Intent : Resolver.Get<ck::FFragment_InteractionResolver_Current>().Get_ActiveIntents())
        { Names.Add(Intent.IsValid() ? Intent.GetTagName().ToString() : TEXT("?")); }
        return Names.IsEmpty() ? TEXT("None") : FString::Join(Names, TEXT(", "));
    }

    auto DiffColor(const bool bMarked) -> FLinearColor
    { return bMarked ? CkStyle::Accent() : CkStyle::Text(); }
}

auto SCkInspector_InteractionResolverAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _SelectionModel = InArgs._SelectionModel;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Refresh_Collections() && Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_InteractionResolverAuthored::~SCkInspector_InteractionResolverAuthored()
{
    Release();
}

auto SCkInspector_InteractionResolverAuthored::Get_IsAvailable() const -> bool
{
    auto Resolver = FCk_Handle_InteractionResolver{};
    return _Active && ck_inspector_interaction_resolver::TryGetResolver(_Entity, Resolver);
}

auto SCkInspector_InteractionResolverAuthored::Get_CanRequest() const -> bool
{
    auto Resolver = FCk_Handle_InteractionResolver{};
    return _Active && ck_inspector_interaction_resolver::TryGetResolver(_Entity, Resolver)
        && ck::DebugRequestGate::Evaluate(Resolver, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_InteractionResolverAuthored::Get_RequestDisabledReason() const -> FString
{
    auto Resolver = FCk_Handle_InteractionResolver{};
    return _Active && ck_inspector_interaction_resolver::TryGetResolver(_Entity, Resolver)
        ? ck::DebugRequestGate::Evaluate(Resolver, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString()
        : TEXT("Interaction Resolver is unavailable.");
}

auto SCkInspector_InteractionResolverAuthored::Get_ActiveIntentsText() const -> FString
{
    return _Active ? ck_inspector_interaction_resolver::GetActiveIntentsText(_Entity) : TEXT("--");
}

auto SCkInspector_InteractionResolverAuthored::Get_HasAvailableTargets() const -> bool
{
    return _Active && _AvailableTargets.IsValid() && NOT _AvailableTargets->GetRecords().IsEmpty();
}

auto SCkInspector_InteractionResolverAuthored::Refresh_Collections() -> bool
{
    using namespace ck_inspector_interaction_resolver;
    if (NOT _Mappings.IsValid())
    {
        const FCkUiCollectionSchema Schema{
            .Fields = {
                {TEXT("intent"), ECkUiFieldKind::Text},
                {TEXT("distance-sort"), ECkUiFieldKind::Text},
                {TEXT("max-concurrent"), ECkUiFieldKind::Text},
                {TEXT("channels-empty"), ECkUiFieldKind::Bool},
                {TEXT("has-clear-actions"), ECkUiFieldKind::Bool},
                {TEXT("best-empty"), ECkUiFieldKind::Bool},
                {TEXT("start-label"), ECkUiFieldKind::Text},
                {TEXT("start-tooltip"), ECkUiFieldKind::Text},
                {TEXT("stop-label"), ECkUiFieldKind::Text},
                {TEXT("stop-tooltip"), ECkUiFieldKind::Text}},
            .Children = {
                {TEXT("channels"), {
                    {TEXT("channel"), ECkUiFieldKind::Text},
                    {TEXT("clear-visible"), ECkUiFieldKind::Bool},
                    {TEXT("clear-label"), ECkUiFieldKind::Text},
                    {TEXT("clear-tooltip"), ECkUiFieldKind::Text}}},
                {TEXT("best-targets"), {
                    {TEXT("target-id"), ECkUiFieldKind::Text},
                    {TEXT("target-name"), ECkUiFieldKind::Text}}}}
        };
        const FCkUiLoadResult Result = FCkUiCollection::TryCreateHierarchical(Schema, _Mappings);
        if (NOT Result.Succeeded || NOT _Mappings.IsValid())
        { _LoadError = FString::Join(Result.Errors, TEXT("\n")); return false; }
    }
    if (NOT _AvailableTargets.IsValid())
    {
        const FCkUiLoadResult Result = FCkUiCollection::TryCreate({
            {TEXT("target-id"), ECkUiFieldKind::Text},
            {TEXT("target-name"), ECkUiFieldKind::Text}}, _AvailableTargets);
        if (NOT Result.Succeeded || NOT _AvailableTargets.IsValid())
        { _LoadError = FString::Join(Result.Errors, TEXT("\n")); return false; }
    }

    auto MappingRecords = TArray<FCkUiRecordData>{};
    auto AvailableRecords = TArray<FCkUiRecordData>{};
    auto BestByKey = TMap<FString, FCk_Handle>{};
    auto AvailableByKey = TMap<FString, FCk_Handle>{};
    auto Resolver = FCk_Handle_InteractionResolver{};
    if (TryGetResolver(_Entity, Resolver))
    {
        const auto& Mappings = Resolver.Get<ck::FFragment_InteractionResolver_Params>().Get_IntentChannelMappings();
        for (int32 MappingIndex = 0; MappingIndex < Mappings.Num(); ++MappingIndex)
        {
            const auto& Mapping = Mappings[MappingIndex];
            const FGameplayTag& Intent = Mapping.Get_Intent();
            const FString IntentName = GetIntentName(Intent);
            const FString MappingKey = GetMappingKey(MappingIndex, Intent);
            auto Record = FCkUiRecordData{};
            Record.Key = MappingKey;
            Record.Fields = {
                {TEXT("intent"), MakeInteractionResolverTextField(IntentName)},
                    {TEXT("distance-sort"), MakeInteractionResolverTextField(
                        ck::Format_UE(TEXT("{}"), Mapping.Get_DistanceSorting()))},
                    {TEXT("max-concurrent"), MakeInteractionResolverTextField(
                        ck::Format_UE(TEXT("{}"), Mapping.Get_MaxConcurrentInteractions()))},
                    {TEXT("channels-empty"), MakeInteractionResolverBoolField(Mapping.Get_Channels().IsEmpty())},
                {TEXT("start-label"), MakeInteractionResolverTextField(TEXT("Start"))},
                {TEXT("start-tooltip"), MakeInteractionResolverTextField(
                    ck::Format_UE(TEXT("Request_StartIntent({})"), IntentName))},
                {TEXT("stop-label"), MakeInteractionResolverTextField(TEXT("Stop"))},
                {TEXT("stop-tooltip"), MakeInteractionResolverTextField(
                    ck::Format_UE(TEXT("Request_StopIntent({})"), IntentName))}};

            auto ChannelRecords = TArray<FCkUiRecordData>{};
            const auto& Channels = Mapping.Get_Channels();
            bool HasClearActions = false;
            for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ++ChannelIndex)
            {
                const FGameplayTag& Channel = Channels[ChannelIndex];
                const FString ChannelName = GetChannelName(Channel);
                HasClearActions |= Channel.IsValid();
                auto ChannelRecord = FCkUiRecordData{};
                ChannelRecord.Key = GetChannelKey(MappingKey, ChannelIndex, Channel);
                ChannelRecord.Fields = {
                    {TEXT("channel"), MakeInteractionResolverTextField(ChannelName)},
                    {TEXT("clear-visible"), MakeInteractionResolverBoolField(Channel.IsValid())},
                    {TEXT("clear-label"), MakeInteractionResolverTextField(ChannelName)},
                    {TEXT("clear-tooltip"), MakeInteractionResolverTextField(ck::Format_UE(
                        TEXT("Request_RemoveAllTargetsByChannel({}) — drops every target this resolver holds on that channel."),
                        ChannelName))}};
                ChannelRecords.Add(MoveTemp(ChannelRecord));
            }
            Record.Fields.Add(TEXT("has-clear-actions"), MakeInteractionResolverBoolField(HasClearActions));
            Record.Children.Add(TEXT("channels"), MoveTemp(ChannelRecords));

            auto BestRecords = TArray<FCkUiRecordData>{};
            if (Intent.IsValid())
            {
                for (const FCk_Handle_InteractTarget& Target :
                    UCk_Utils_InteractionResolver_UE::Get_BestInteractTargets(Resolver, Intent))
                {
                    const FString Key = GetHandleKey(MappingKey + TEXT("/best"), Target);
                    AddTargetRecord(Key, Target, BestRecords);
                    if (NOT Key.IsEmpty()) { BestByKey.Add(Key, Target); }
                }
            }
            Record.Fields.Add(TEXT("best-empty"), MakeInteractionResolverBoolField(BestRecords.IsEmpty()));
            Record.Children.Add(TEXT("best-targets"), MoveTemp(BestRecords));
            MappingRecords.Add(MoveTemp(Record));
        }

        auto Available = TArray<FCk_Handle>{};
        for (const FCk_Handle_InteractTarget& Target :
            Resolver.Get<ck::FFragment_InteractionResolver_Current>().Get_AvailableTargets())
        { if (ck::IsValid(Target)) { Available.Add(Target); } }
        for (const FCk_Handle& Target : Available)
        {
            const FString Key = GetHandleKey(TEXT("available"), Target);
            AddTargetRecord(Key, Target, AvailableRecords);
            if (NOT Key.IsEmpty()) { AvailableByKey.Add(Key, Target); }
        }
    }

    const FCkUiLoadResult Result = FCkUiCollection::TrySetRecordsBatch({
        {_Mappings, MoveTemp(MappingRecords)},
        {_AvailableTargets, MoveTemp(AvailableRecords)}});
    if (NOT Result.Succeeded)
    { _LoadError = FString::Join(Result.Errors, TEXT("\n")); return false; }
    _BestTargetsByKey = MoveTemp(BestByKey);
    _AvailableTargetsByKey = MoveTemp(AvailableByKey);
    return true;
}

auto SCkInspector_InteractionResolverAuthored::Request_StartIntent(const FString& InStableKey) -> void
{
    auto Resolver = FCk_Handle_InteractionResolver{};
    auto Intent = FGameplayTag{};
    if (ck_inspector_interaction_resolver::TryResolveIntent(_Entity, InStableKey, Resolver, Intent)
        && ck::DebugRequestGate::Evaluate(Resolver, ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
    { UCk_Utils_InteractionResolver_UE::Request_StartIntent(Resolver, FCk_Request_InteractionResolver_StartIntent{Intent}, {}); }
}

auto SCkInspector_InteractionResolverAuthored::Request_StopIntent(const FString& InStableKey) -> void
{
    auto Resolver = FCk_Handle_InteractionResolver{};
    auto Intent = FGameplayTag{};
    if (ck_inspector_interaction_resolver::TryResolveIntent(_Entity, InStableKey, Resolver, Intent)
        && ck::DebugRequestGate::Evaluate(Resolver, ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
    { UCk_Utils_InteractionResolver_UE::Request_StopIntent(Resolver, FCk_Request_InteractionResolver_StopIntent{Intent}, {}); }
}

auto SCkInspector_InteractionResolverAuthored::Request_ClearChannel(const FString& InStableKey) -> void
{
    auto Resolver = FCk_Handle_InteractionResolver{};
    auto Channel = FGameplayTag{};
    if (ck_inspector_interaction_resolver::TryResolveChannel(_Entity, InStableKey, Resolver, Channel)
        && ck::DebugRequestGate::Evaluate(Resolver, ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
    {
        UCk_Utils_InteractionResolver_UE::Request_RemoveAllTargetsByChannel(
            Resolver, FCk_Request_InteractionResolver_RemoveAllTargetsByChannel{Channel}, {});
    }
}

auto SCkInspector_InteractionResolverAuthored::Navigate_BestTarget(const FString& InStableKey) -> void
{
    if (NOT Get_IsAvailable()) { return; }
    const FCk_Handle* Expected = _BestTargetsByKey.Find(InStableKey);
    if (Expected == nullptr || ck::Is_NOT_Valid(*Expected)) { return; }
    auto Resolver = FCk_Handle_InteractionResolver{};
    if (NOT ck_inspector_interaction_resolver::TryGetResolver(_Entity, Resolver)) { return; }
    const auto& Mappings = Resolver.Get<ck::FFragment_InteractionResolver_Params>().Get_IntentChannelMappings();
    for (int32 Index = 0; Index < Mappings.Num(); ++Index)
    {
        const FString Prefix = ck_inspector_interaction_resolver::GetMappingKey(Index, Mappings[Index].Get_Intent())
            + TEXT("/best");
        for (const FCk_Handle_InteractTarget& Current :
            UCk_Utils_InteractionResolver_UE::Get_BestInteractTargets(Resolver, Mappings[Index].Get_Intent()))
        {
            if (Current == *Expected
                && ck_inspector_interaction_resolver::GetHandleKey(Prefix, Current) == InStableKey)
            {
                if (const auto Selection = _SelectionModel.Pin(); Selection.IsValid())
                { Selection->Set_SelectedEntities({Current}); }
                else { ck::DebugNav::Goto_Entity(Current); }
                return;
            }
        }
    }
}

auto SCkInspector_InteractionResolverAuthored::Navigate_AvailableTarget(const FString& InStableKey) -> void
{
    if (NOT Get_IsAvailable()) { return; }
    const FCk_Handle* Expected = _AvailableTargetsByKey.Find(InStableKey);
    if (Expected == nullptr || ck::Is_NOT_Valid(*Expected)) { return; }
    auto Resolver = FCk_Handle_InteractionResolver{};
    if (NOT ck_inspector_interaction_resolver::TryGetResolver(_Entity, Resolver)) { return; }
    for (const FCk_Handle_InteractTarget& Current :
        Resolver.Get<ck::FFragment_InteractionResolver_Current>().Get_AvailableTargets())
    {
        if (Current == *Expected
            && ck_inspector_interaction_resolver::GetHandleKey(TEXT("available"), Current) == InStableKey)
        {
            if (const auto Selection = _SelectionModel.Pin(); Selection.IsValid())
            { Selection->Set_SelectedEntities({Current}); }
            else { ck::DebugNav::Goto_Entity(Current); }
            return;
        }
    }
}

auto SCkInspector_InteractionResolverAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_InteractionResolverAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("interaction-resolver-active-intents"), TAttribute<FText>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return FText::FromString(Widget.IsValid() ? Widget->Get_ActiveIntentsText() : FString{});
    }));
    Data.Text.Add(TEXT("interaction-resolver-disabled-reason"), TAttribute<FText>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return FText::FromString(Widget.IsValid() ? Widget->Get_RequestDisabledReason() : FString{});
    }));
    Data.Visibility.Add(TEXT("interaction-resolver-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("interaction-resolver-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("interaction-resolver-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Data.Visibility.Add(TEXT("interaction-resolver-has-available-targets"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_HasAvailableTargets(); }));
    Data.Visibility.Add(TEXT("interaction-resolver-no-available-targets"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_HasAvailableTargets(); }));
    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("intent"), TEXT("Intent:")}, {TEXT("channels"), TEXT("Channels:")},
        {TEXT("clear-channel"), TEXT("Clear Channel:")}, {TEXT("distance-sort"), TEXT("Distance Sort:")},
        {TEXT("max-concurrent"), TEXT("Max Concurrent:")}, {TEXT("best-targets"), TEXT("Best Targets:")},
        {TEXT("active-intents"), TEXT("Active Intents:")}, {TEXT("available-targets"), TEXT("Available Targets:")}})
    {
        Data.Color.Add(TEXT("interaction-resolver-") + Pair.Key + TEXT("-diff-color"),
            TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]()
            {
                const auto Widget = Weak.Pin();
                return Widget.IsValid()
                    ? ck_inspector_interaction_resolver::DiffColor(Widget->Is_DiffMarked(Label))
                    : FLinearColor::Transparent;
            }));
    }
    Data.Collections.Add(TEXT("interaction-resolver-mappings"), _Mappings);
    Data.Collections.Add(TEXT("interaction-resolver-available-targets"), _AvailableTargets);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    Data.ItemActions.Add(TEXT("interaction-resolver-start-intent"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& Key) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_StartIntent(Key); } }));
    Data.ItemActions.Add(TEXT("interaction-resolver-stop-intent"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& Key) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_StopIntent(Key); } }));
    Data.ItemActions.Add(TEXT("interaction-resolver-clear-channel"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& Key) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_ClearChannel(Key); } }));
    Data.ItemActions.Add(TEXT("interaction-resolver-navigate-best"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& Key) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Navigate_BestTarget(Key); } }));
    Data.ItemActions.Add(TEXT("interaction-resolver-navigate-available"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& Key) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Navigate_AvailableTarget(Key); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorInteractionResolver.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorInteractionResolver.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_InteractionResolverAuthored::Tick(
    const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    if (NOT Refresh_Collections()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_InteractionResolverAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _SelectionModel.Reset();
    _DiffLabels.Reset();
    _BestTargetsByKey.Reset();
    _AvailableTargetsByKey.Reset();
    _Mappings.Reset();
    _AvailableTargets.Reset();
    _View.Reset();
    _Mounted = false;
}

FCkInspector_InteractionResolver::~FCkInspector_InteractionResolver()
{
    OnDeactivated();
}

auto FCkInspector_InteractionResolver::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Interaction Resolver"));
}

auto FCkInspector_InteractionResolver::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Resolver = FCk_Handle_InteractionResolver{};
    return ck_inspector_interaction_resolver::TryGetResolver(Entity, Resolver);
}

auto FCkInspector_InteractionResolver::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : TArray<FString>{
        TEXT("Intent:"), TEXT("Channels:"), TEXT("Clear Channel:"), TEXT("Distance Sort:"),
        TEXT("Max Concurrent:"), TEXT("Best Targets:"), TEXT("Active Intents:"), TEXT("Available Targets:")})
    { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }
    const auto Authored = SNew(SCkInspector_InteractionResolverAuthored)
        .Entity(Entity).SelectionModel(Get_SelectionModel()).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    { _LastAuthoredLoadError = Authored->Get_LoadError(); return NativeBody; }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_InteractionResolver::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    using namespace ck_inspector_interaction_resolver;
    auto Builder = FCkInspectorWidgetBuilder{};
    Builder.SetEditGuard(Get_EditGuard());
    auto Resolver = FCk_Handle_InteractionResolver{};
    if (NOT TryGetResolver(Entity, Resolver))
    { return Builder.Build(Entity, FString{}); }
    const auto& Mappings = Resolver.Get<ck::FFragment_InteractionResolver_Params>().Get_IntentChannelMappings();
    for (int32 MappingIndex = 0; MappingIndex < Mappings.Num(); ++MappingIndex)
    {
        const auto& Mapping = Mappings[MappingIndex];
        const FGameplayTag Intent = Mapping.Get_Intent();
        const FString IntentName = GetIntentName(Intent);
        const FString MappingKey = GetMappingKey(MappingIndex, Intent);
        Builder.AddHeader(FText::FromString(IntentName));
        Builder.AddActionRow(FText::FromString(TEXT("Intent:")), {
            {FText::FromString(TEXT("Start")), FText::FromString(ck::Format_UE(TEXT("Request_StartIntent({})"), IntentName)),
                [Entity, MappingKey]()
                {
                    auto Current = FCk_Handle_InteractionResolver{}; auto CurrentIntent = FGameplayTag{};
                    if (TryResolveIntent(Entity, MappingKey, Current, CurrentIntent)
                        && ck::DebugRequestGate::Evaluate(Current, ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
                    { UCk_Utils_InteractionResolver_UE::Request_StartIntent(Current, FCk_Request_InteractionResolver_StartIntent{CurrentIntent}, {}); }
                }},
            {FText::FromString(TEXT("Stop")), FText::FromString(ck::Format_UE(TEXT("Request_StopIntent({})"), IntentName)),
                [Entity, MappingKey]()
                {
                    auto Current = FCk_Handle_InteractionResolver{}; auto CurrentIntent = FGameplayTag{};
                    if (TryResolveIntent(Entity, MappingKey, Current, CurrentIntent)
                        && ck::DebugRequestGate::Evaluate(Current, ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
                    { UCk_Utils_InteractionResolver_UE::Request_StopIntent(Current, FCk_Request_InteractionResolver_StopIntent{CurrentIntent}, {}); }
                }}});

        auto ChannelChips = TArray<FCkInspector_Chip>{};
        auto ClearActions = TArray<FCkInspector_Action>{};
        for (int32 ChannelIndex = 0; ChannelIndex < Mapping.Get_Channels().Num(); ++ChannelIndex)
        {
            const FGameplayTag Channel = Mapping.Get_Channels()[ChannelIndex];
            const FString ChannelName = GetChannelName(Channel);
            ChannelChips.Add({FText::FromString(ChannelName), Channel.IsValid() ? ECk_Tone::Neutral : ECk_Tone::Warn});
            if (NOT Channel.IsValid()) { continue; }
            const FString ChannelKey = GetChannelKey(MappingKey, ChannelIndex, Channel);
            ClearActions.Add({FText::FromString(ChannelName), FText::FromString(ck::Format_UE(
                    TEXT("Request_RemoveAllTargetsByChannel({}) — drops every target this resolver holds on that channel."), ChannelName)),
                [Entity, ChannelKey]()
                {
                    auto Current = FCk_Handle_InteractionResolver{}; auto CurrentChannel = FGameplayTag{};
                    if (TryResolveChannel(Entity, ChannelKey, Current, CurrentChannel)
                        && ck::DebugRequestGate::Evaluate(Current, ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
                    { UCk_Utils_InteractionResolver_UE::Request_RemoveAllTargetsByChannel(Current,
                        FCk_Request_InteractionResolver_RemoveAllTargetsByChannel{CurrentChannel}, {}); }
                }});
        }
        if (ChannelChips.IsEmpty())
        {
            Builder.AddRow(FText::FromString(TEXT("Channels:")), [](const FCk_Handle&)
            { return FText::FromString(TEXT("None")); }, CkStyle::TextDim());
        }
        else { Builder.AddChipsRow(FText::FromString(TEXT("Channels:")), ChannelChips); }
        if (NOT ClearActions.IsEmpty())
        { Builder.AddActionRow(FText::FromString(TEXT("Clear Channel:")), ClearActions); }
        Builder.AddRow(FText::FromString(TEXT("Distance Sort:")), [Value = Mapping.Get_DistanceSorting()](const FCk_Handle&)
        { return FText::FromString(ck::Format_UE(TEXT("{}"), Value)); }, CkStyle::Value_Enum());
        Builder.AddRow(FText::FromString(TEXT("Max Concurrent:")), [Value = Mapping.Get_MaxConcurrentInteractions()](const FCk_Handle&)
        { return FText::FromString(ck::Format_UE(TEXT("{}"), Value)); }, CkStyle::Value_Numeric());
        auto BestHandles = TArray<FCk_Handle>{};
        for (const FCk_Handle_InteractTarget& Target : UCk_Utils_InteractionResolver_UE::Get_BestInteractTargets(Resolver, Intent))
        { if (ck::IsValid(Target)) { BestHandles.Add(Target); } }
        Builder.AddWidgetRow(FText::FromString(TEXT("Best Targets:")), FCkInspectorWidgetBuilder::MakeBadgeBox(BestHandles));
    }
    Builder.AddHeader(FText::FromString(TEXT("Runtime")));
    Builder.AddConditionalRow(FText::FromString(TEXT("Active Intents:")), [Entity](const FCk_Handle&)
    { return FText::FromString(GetActiveIntentsText(Entity)); }, [Entity](const FCk_Handle&)
    {
        auto Current = FCk_Handle_InteractionResolver{};
        if (NOT TryGetResolver(Entity, Current)) { return CkStyle::None(); }
        return Current.Get<ck::FFragment_InteractionResolver_Current>().Get_ActiveIntents().IsEmpty()
            ? CkStyle::TextDim() : CkStyle::Status_Active();
    });
    auto AvailableHandles = TArray<FCk_Handle>{};
    if (TryGetResolver(Entity, Resolver))
    {
        for (const FCk_Handle_InteractTarget& Target :
            Resolver.Get<ck::FFragment_InteractionResolver_Current>().Get_AvailableTargets())
        { if (ck::IsValid(Target)) { AvailableHandles.Add(Target); } }
    }
    Builder.AddWidgetRow(FText::FromString(TEXT("Available Targets:")),
        FCkInspectorWidgetBuilder::MakeBadgeBox(AvailableHandles));
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_InteractionResolver::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_InteractionResolverAuthored>& Instance)
    { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); });
    _LastAuthoredLoadError.Reset();
    for (const auto& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        { _LastAuthoredLoadError = Instance->Get_LoadError(); break; }
    }
}

auto FCkInspector_InteractionResolver::OnDeactivated() -> void
{
    for (const auto& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
    _LastAuthoredLoadError.Reset();
}
