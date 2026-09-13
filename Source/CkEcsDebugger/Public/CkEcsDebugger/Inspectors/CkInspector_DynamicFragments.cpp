#include "CkInspector_DynamicFragments.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDynamic/CkDynamic_Fragment.h"
#include "CkDynamic/CkDynamic_FragmentDisplaySchema.h"
#include "CkDynamic/CkDynamic_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "GameplayTagContainer.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "StructUtils/InstancedStruct.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_DynamicFragments)

// =====================================================================================================================

auto FCkInspector_DynamicFragments::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Dynamic Fragments"));
}

auto FCkInspector_DynamicFragments::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && Entity.Has<ck::FFragment_DynamicFragment_Data>();
}

auto FCkInspector_DynamicFragments::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return SNullWidget::NullWidget;
}

auto FCkInspector_DynamicFragments::Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection>
{
    auto Sections = TArray<FInspectorSection>{};
    const auto AllFragments = UCk_Utils_DynamicFragment_UE::Get_AllFragments(Entity);
    auto FragmentTypes = TArray<TWeakObjectPtr<const UScriptStruct>>{};
    _LastFragmentKeys.Reset();

    for (const auto& Fragment : AllFragments)
    {
        const auto* ScriptStruct = Fragment.GetScriptStruct();
        if (NOT ck::IsValid(ScriptStruct))
        { continue; }

        _LastFragmentKeys.Add(ScriptStruct->GetPathName());
        FragmentTypes.Add(TWeakObjectPtr<const UScriptStruct>{ScriptStruct});
        Sections.Add(FInspectorSection
        {
            FText::FromString(ck::dynamic::Resolve_FragmentDisplayName(ScriptStruct)),
            BuildFragmentWidget(Entity, Fragment)
        });
    }

    _LastFragmentKeys.Sort();
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return Sections; }

    auto Candidates = TArray<TSharedRef<SCkInspector_DynamicFragmentAuthored>>{};
    for (const TWeakObjectPtr<const UScriptStruct>& WeakType : FragmentTypes)
    {
        const auto* ScriptStruct = WeakType.Get();
        if (NOT ck::IsValid(ScriptStruct))
        { continue; }

        auto DiffLabels = TSet<FString>{};
        for (TFieldIterator<FProperty> PropertyIt(ScriptStruct); PropertyIt; ++PropertyIt)
        {
            const FString Label = ck::dynamic::Resolve_PropertyDisplayName(ScriptStruct, *PropertyIt);
            if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
            { DiffLabels.Add(Label); }
        }
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Fragment:")))
        { DiffLabels.Add(TEXT("Fragment:")); }

        const TSharedRef<SCkInspector_DynamicFragmentAuthored> Candidate =
            SNew(SCkInspector_DynamicFragmentAuthored)
            .Entity(Entity)
            .FragmentType(WeakType)
            .SelectionModel(Get_SelectionModel())
            .DiffLabels(MoveTemp(DiffLabels));
        if (NOT Candidate->Is_Mounted())
        {
            _LastAuthoredLoadError = Candidate->Get_LoadError();
            for (const TSharedRef<SCkInspector_DynamicFragmentAuthored>& Mounted : Candidates)
            { Mounted->Release(); }
            return Sections;
        }
        Candidates.Add(Candidate);
    }

    if (Candidates.Num() != Sections.Num())
    {
        _LastAuthoredLoadError = TEXT("A dynamic fragment type became unavailable during authored composition.");
        for (const TSharedRef<SCkInspector_DynamicFragmentAuthored>& Mounted : Candidates)
        { Mounted->Release(); }
        return Sections;
    }

    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        Sections[Index].Widget = Candidates[Index];
        _AuthoredInstances.Add(Candidates[Index]);
    }
    _LastAuthoredLoadError.Reset();

    return Sections;
}

// =====================================================================================================================

namespace
{
    auto IsStructType(const FStructProperty* InProp, const UScriptStruct* InType) -> bool
    {
        return InProp && InProp->Struct == InType;
    }

    // ---- Formatting helpers

    struct FPropertyDisplay
    {
        FString Value;
        FLinearColor Color = CkStyle::Text();
    };

    auto FormatProperty(
        const UScriptStruct* InFragmentType,
        const FProperty* InProperty,
        const void* InContainer) -> FPropertyDisplay
    {
        const auto* ValuePtr = InProperty->ContainerPtrToValuePtr<void>(InContainer);

        // ---- Bool

        if (const auto* BoolProp = CastField<FBoolProperty>(InProperty))
        {
            const auto BoolValue = BoolProp->GetPropertyValue(ValuePtr);
            return { BoolValue ? TEXT("true") : TEXT("false"), BoolValue ? CkStyle::Value_Bool_True() : CkStyle::Value_Bool_False() };
        }

        // ---- Enum (FEnumProperty or FByteProperty with enum)

        if (const auto* EnumProp = CastField<FEnumProperty>(InProperty))
        {
            const auto* Enum = EnumProp->GetEnum();
            const auto NumericValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
            const auto DisplayName = ck::dynamic::Resolve_EnumValueDisplayName(InFragmentType, Enum, NumericValue);
            return { DisplayName, CkStyle::Value_Enum() };
        }

        if (const auto* ByteProp = CastField<FByteProperty>(InProperty);
            ByteProp && ByteProp->Enum)
        {
            const auto ByteValue = *static_cast<const uint8*>(ValuePtr);
            const auto DisplayName = ck::dynamic::Resolve_EnumValueDisplayName(InFragmentType, ByteProp->Enum, ByteValue);
            return { DisplayName, CkStyle::Value_Enum() };
        }

        // ---- Numeric (int, float, double)

        if (CastField<FNumericProperty>(InProperty))
        {
            auto ValueStr = FString{};
            InProperty->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
            return { ValueStr, CkStyle::Value_Numeric() };
        }

        // ---- String types

        if (CastField<FNameProperty>(InProperty))
        {
            const auto NameValue = *static_cast<const FName*>(ValuePtr);
            return { NameValue.IsNone() ? TEXT("(None)") : NameValue.ToString(), CkStyle::Value_String() };
        }

        if (CastField<FStrProperty>(InProperty))
        {
            const auto& StrValue = *static_cast<const FString*>(ValuePtr);
            return { StrValue.IsEmpty() ? TEXT("(Empty)") : StrValue, CkStyle::Value_String() };
        }

        if (CastField<FTextProperty>(InProperty))
        {
            const auto& TextValue = *static_cast<const FText*>(ValuePtr);
            return { TextValue.IsEmpty() ? TEXT("(Empty)") : TextValue.ToString(), CkStyle::Value_String() };
        }

        // ---- UObject pointer

        if (const auto* ObjProp = CastField<FObjectPropertyBase>(InProperty))
        {
            const auto* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);

            // Raw UObject* in an FInstancedStruct is not guaranteed to be nulled by GC the
            // way a UPROPERTY-reflected pointer on a UObject is. Stale references survive as
            // non-null but pending-kill / unreachable, and GetName() on those will crash.
            // ::IsValid checks GUObjectArray + object flags safely.
            if (Obj == nullptr || NOT ::IsValid(Obj))
            { return { TEXT("(None)"), CkStyle::Value_Object() }; }

            return { Obj->GetName(), CkStyle::Value_Object() };
        }

        // ---- Struct types (order matters - check specific before generic fallback)

        if (const auto* StructProp = CastField<FStructProperty>(InProperty))
        {
            // FGameplayTag
            if (IsStructType(StructProp, FGameplayTag::StaticStruct()))
            {
                const auto& Tag = *static_cast<const FGameplayTag*>(ValuePtr);
                return { Tag.IsValid() ? Tag.GetTagName().ToString() : TEXT("(None)"), CkStyle::Value_Tag() };
            }

            // FGameplayTagContainer
            if (IsStructType(StructProp, FGameplayTagContainer::StaticStruct()))
            {
                const auto& Container = *static_cast<const FGameplayTagContainer*>(ValuePtr);
                return { Container.IsEmpty() ? TEXT("(Empty)") : Container.ToString(), CkStyle::Value_Tag() };
            }

            // FVector
            if (IsStructType(StructProp, TBaseStructure<FVector>::Get()))
            {
                return { static_cast<const FVector*>(ValuePtr)->ToString(), CkStyle::Value_Math() };
            }

            // FVector2D
            if (IsStructType(StructProp, TBaseStructure<FVector2D>::Get()))
            {
                return { static_cast<const FVector2D*>(ValuePtr)->ToString(), CkStyle::Value_Math() };
            }

            // FRotator
            if (IsStructType(StructProp, TBaseStructure<FRotator>::Get()))
            {
                return { static_cast<const FRotator*>(ValuePtr)->ToString(), CkStyle::Value_Math() };
            }

            // FTransform
            if (IsStructType(StructProp, TBaseStructure<FTransform>::Get()))
            {
                return { static_cast<const FTransform*>(ValuePtr)->ToString(), CkStyle::Value_Math() };
            }

            // FLinearColor
            if (IsStructType(StructProp, TBaseStructure<FLinearColor>::Get()))
            {
                const auto& Color = *static_cast<const FLinearColor*>(ValuePtr);
                return { Color.ToString(), Color };
            }

            // FColor
            if (IsStructType(StructProp, TBaseStructure<FColor>::Get()))
            {
                const auto& Color = *static_cast<const FColor*>(ValuePtr);
                return { Color.ToString(), FLinearColor(Color) };
            }
        }

        // ---- Fallback: use ExportTextItem

        auto ValueStr = FString{};
        InProperty->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
        return { ValueStr, CkStyle::Text() };
    }

    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

    auto TryResolveFragment(
        const FCk_Handle& InEntity,
        const UScriptStruct* InFragmentType,
        FInstancedStruct*& OutFragment) -> bool
    {
        OutFragment = nullptr;
        if (IsDestroying(InEntity) || NOT ck::IsValid(InFragmentType))
        { return false; }
        if (NOT UCk_Utils_DynamicFragment_UE::Has_Fragment(InEntity, InFragmentType))
        { return false; }

        OutFragment = UCk_Utils_DynamicFragment_UE::TryGet_Fragment_TypeUnsafe(InEntity, InFragmentType);
        return OutFragment != nullptr
            && OutFragment->GetScriptStruct() == InFragmentType
            && OutFragment->GetMemory() != nullptr;
    }

    auto GetPropertyKey(const UScriptStruct* InFragmentType, const FProperty* InProperty) -> FString
    {
        return ck::IsValid(InFragmentType) && InProperty != nullptr
            ? InFragmentType->GetPathName() + TEXT("::") + InProperty->GetName()
            : FString{};
    }

    auto TextField(const FString& InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)};
    }

    auto BoolField(const bool bInValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = bInValue};
    }

    auto ColorField(const FLinearColor InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = InValue};
    }
}

// =====================================================================================================================

auto SCkInspector_DynamicFragmentAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _FragmentType = InArgs._FragmentType;
    _SelectionModel = InArgs._SelectionModel;
    _DiffLabels = InArgs._DiffLabels;

    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Refresh_PropertyRecords() && Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_DynamicFragmentAuthored::~SCkInspector_DynamicFragmentAuthored()
{
    Release();
}

auto SCkInspector_DynamicFragmentAuthored::Get_IsAvailable() const -> bool
{
    FInstancedStruct* Fragment = nullptr;
    return _Active && TryResolveFragment(_Entity, _FragmentType.Get(), Fragment);
}

auto SCkInspector_DynamicFragmentAuthored::Get_IsReplicated() const -> bool
{
    const auto* FragmentType = _FragmentType.Get();
    return Get_IsAvailable()
        && ck::IsValid(FragmentType)
        && _Entity.Has<ck::FFragment_DynamicFragment_ReplicatedTypes>()
        && _Entity.Get<ck::FFragment_DynamicFragment_ReplicatedTypes>().Get_Types().Contains(FragmentType);
}

auto SCkInspector_DynamicFragmentAuthored::Get_CanRemove() const -> bool
{
    return Get_IsAvailable()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_DynamicFragmentAuthored::Get_CanMarkReplicationDirty() const -> bool
{
    return Get_IsReplicated()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled;
}

auto SCkInspector_DynamicFragmentAuthored::Get_ActionDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable())
    { return TEXT("Dynamic fragment is unavailable."); }
    return ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString();
}

auto SCkInspector_DynamicFragmentAuthored::Get_MarkReplicationDirtyDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable())
    { return TEXT("Dynamic fragment is unavailable."); }
    if (NOT Get_IsReplicated())
    { return TEXT("Dynamic fragment is not registered for replication."); }
    return ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).Reason.ToString();
}

auto SCkInspector_DynamicFragmentAuthored::Refresh_PropertyRecords() -> bool
{
    if (NOT _Properties.IsValid())
    {
        const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreate(
            {
                {TEXT("property-name"), ECkUiFieldKind::Text},
                {TEXT("property-value"), ECkUiFieldKind::Text},
                {TEXT("property-color"), ECkUiFieldKind::Color},
                {TEXT("diff-color"), ECkUiFieldKind::Color},
                {TEXT("is-handle"), ECkUiFieldKind::Bool},
                {TEXT("is-value"), ECkUiFieldKind::Bool},
                {TEXT("handle-valid"), ECkUiFieldKind::Bool},
                {TEXT("handle-invalid"), ECkUiFieldKind::Bool},
                {TEXT("handle-id"), ECkUiFieldKind::Text},
                {TEXT("handle-name"), ECkUiFieldKind::Text},
                {TEXT("handle-tooltip"), ECkUiFieldKind::Text},
            },
            _Properties);
        if (NOT CreateResult.Succeeded || NOT _Properties.IsValid())
        {
            _LoadError = FString::Join(CreateResult.Errors, TEXT("\n"));
            if (_LoadError.IsEmpty())
            { _LoadError = TEXT("Dynamic fragment property collection is unavailable."); }
            return false;
        }
    }

    auto Records = TArray<FCkUiRecordData>{};
    const auto* FragmentType = _FragmentType.Get();
    FInstancedStruct* Fragment = nullptr;
    if (TryResolveFragment(_Entity, FragmentType, Fragment))
    {
        const void* Memory = Fragment->GetMemory();
        for (TFieldIterator<FProperty> PropertyIt(FragmentType); PropertyIt; ++PropertyIt)
        {
            const FProperty* Property = *PropertyIt;
            const FString Key = GetPropertyKey(FragmentType, Property);
            if (Key.IsEmpty())
            { continue; }

            const FString PropertyName = ck::dynamic::Resolve_PropertyDisplayName(FragmentType, Property);
            const auto* StructProperty = CastField<FStructProperty>(Property);
            const bool bIsHandle = StructProperty != nullptr
                && StructProperty->Struct != nullptr
                && (StructProperty->Struct == FCk_Handle::StaticStruct()
                    || StructProperty->Struct->IsChildOf(FCk_Handle::StaticStruct()));

            auto Record = FCkUiRecordData{};
            Record.Key = Key;
            Record.Fields.Add(TEXT("property-name"), TextField(PropertyName));
            Record.Fields.Add(TEXT("diff-color"), ColorField(
                _DiffLabels.Contains(PropertyName) ? CkStyle::Accent() : CkStyle::Text()));
            Record.Fields.Add(TEXT("is-handle"), BoolField(bIsHandle));
            Record.Fields.Add(TEXT("is-value"), BoolField(NOT bIsHandle));
            Record.Fields.Add(TEXT("handle-valid"), BoolField(false));
            Record.Fields.Add(TEXT("handle-invalid"), BoolField(false));
            Record.Fields.Add(TEXT("handle-id"), TextField(TEXT("--")));
            Record.Fields.Add(TEXT("handle-name"), TextField(TEXT("(Invalid)")));
            Record.Fields.Add(TEXT("handle-tooltip"), TextField(PropertyName));

            if (bIsHandle)
            {
                const FCk_Handle Handle = *StructProperty->ContainerPtrToValuePtr<FCk_Handle>(Memory);
                const bool bHandleValid = ck::IsValid(Handle);
                Record.Fields[TEXT("handle-valid")] = BoolField(bHandleValid);
                Record.Fields[TEXT("handle-invalid")] = BoolField(NOT bHandleValid);
                if (bHandleValid)
                {
                    Record.Fields[TEXT("handle-id")] = TextField(ck::Format_UE(TEXT("{}"), Handle.Get_Entity()));
                    Record.Fields[TEXT("handle-name")] = TextField(UCk_Utils_Handle_UE::Get_DebugName(Handle).ToString());
                }
                Record.Fields.Add(TEXT("property-value"), TextField(bHandleValid ? FString{} : TEXT("(Invalid)")));
                Record.Fields.Add(TEXT("property-color"), ColorField(CkStyle::Value_Object()));
            }
            else
            {
                const FPropertyDisplay Display = FormatProperty(FragmentType, Property, Memory);
                Record.Fields.Add(TEXT("property-value"), TextField(Display.Value));
                Record.Fields.Add(TEXT("property-color"), ColorField(Display.Color));
            }
            Records.Add(MoveTemp(Record));
        }
    }

    const FCkUiLoadResult RecordsResult = _Properties->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded)
    {
        _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n"));
        return false;
    }
    return true;
}

auto SCkInspector_DynamicFragmentAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid())
        { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid())
        { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_DynamicFragmentAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Visibility.Add(TEXT("dynamic-fragment-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("dynamic-fragment-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("dynamic-fragment-can-remove"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRemove(); }));
    Data.Visibility.Add(TEXT("dynamic-fragment-replicated"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsReplicated(); }));
    Data.Visibility.Add(TEXT("dynamic-fragment-can-mark-rep-dirty"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanMarkReplicationDirty(); }));
    Data.Color.Add(TEXT("dynamic-fragment-actions-diff-color"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->_DiffLabels.Contains(TEXT("Fragment:"))
            ? CkStyle::Accent() : CkStyle::Text();
    }));
    Data.Text.Add(TEXT("dynamic-fragment-remove-label"), FText::FromString(TEXT("Remove")));
    Data.Text.Add(TEXT("dynamic-fragment-remove-tooltip"), FText::FromString(
        TEXT("Request_TryRemove - drops this dynamic fragment from the entity. No-op if it is already gone.")));
    Data.Text.Add(TEXT("dynamic-fragment-mark-label"), FText::FromString(TEXT("Mark Rep Dirty")));
    Data.Text.Add(TEXT("dynamic-fragment-mark-tooltip"), FText::FromString(
        TEXT("Request_MarkReplicationDirty - re-sends this fragment on the next replication pass.")));
    Data.Text.Add(TEXT("dynamic-fragment-remove-disabled-reason"), TAttribute<FText>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_ActionDisabledReason()) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("dynamic-fragment-mark-disabled-reason"), TAttribute<FText>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_MarkReplicationDirtyDisabledReason()) : FText::GetEmpty(); }));
    Data.Collections.Add(TEXT("dynamic-fragment-properties"), _Properties);
    Data.ItemActions.Add(TEXT("dynamic-fragment-navigate"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& InStableKey)
        { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Navigate_HandleProperty(InStableKey); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("dynamic-fragment-remove"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Remove(); } }));
    Actions.Add(TEXT("dynamic-fragment-mark-rep-dirty"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_MarkReplicationDirty(); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorDynamicFragments.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorDynamicFragments.ui.css")));
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

auto SCkInspector_DynamicFragmentAuthored::Request_Remove() -> void
{
    if (NOT Get_CanRemove())
    { return; }
    const auto* FragmentType = _FragmentType.Get();
    FInstancedStruct* Fragment = nullptr;
    if (NOT TryResolveFragment(_Entity, FragmentType, Fragment))
    { return; }

    auto MutableEntity = _Entity;
    UCk_Utils_DynamicFragment_UE::Request_TryRemove(MutableEntity, FragmentType, {});
}

auto SCkInspector_DynamicFragmentAuthored::Request_MarkReplicationDirty() -> void
{
    if (NOT Get_CanMarkReplicationDirty())
    { return; }
    const auto* FragmentType = _FragmentType.Get();
    FInstancedStruct* Fragment = nullptr;
    if (NOT TryResolveFragment(_Entity, FragmentType, Fragment))
    { return; }

    auto MutableEntity = _Entity;
    UCk_Utils_DynamicFragment_UE::Request_MarkReplicationDirty(MutableEntity, FragmentType, {});
}

auto SCkInspector_DynamicFragmentAuthored::Navigate_HandleProperty(const FString& InStableKey) -> void
{
    const auto* FragmentType = _FragmentType.Get();
    FInstancedStruct* Fragment = nullptr;
    if (NOT _Active || NOT TryResolveFragment(_Entity, FragmentType, Fragment))
    { return; }

    for (TFieldIterator<FProperty> PropertyIt(FragmentType); PropertyIt; ++PropertyIt)
    {
        const FProperty* Property = *PropertyIt;
        if (GetPropertyKey(FragmentType, Property) != InStableKey)
        { continue; }

        const auto* StructProperty = CastField<FStructProperty>(Property);
        if (StructProperty == nullptr || StructProperty->Struct == nullptr
            || (StructProperty->Struct != FCk_Handle::StaticStruct()
                && NOT StructProperty->Struct->IsChildOf(FCk_Handle::StaticStruct())))
        { return; }

        const FCk_Handle Handle = *StructProperty->ContainerPtrToValuePtr<FCk_Handle>(Fragment->GetMemory());
        if (ck::Is_NOT_Valid(Handle))
        { return; }
        if (const TSharedPtr<FCkDebuggerModel_EntitySelection> Selection = _SelectionModel.Pin(); Selection.IsValid())
        { Selection->Set_SelectedEntities({Handle}); }
        else
        { ck::DebugNav::Goto_Entity(Handle); }
        return;
    }
}

auto SCkInspector_DynamicFragmentAuthored::Tick(
    const FGeometry& InGeometry, const double InTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InGeometry, InTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid())
    { return; }
    if (NOT Refresh_PropertyRecords())
    { return; }

    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else
    { _LoadError.Reset(); }
}

auto SCkInspector_DynamicFragmentAuthored::Release() -> void
{
    if (NOT _Active)
    { return; }
    _Active = false;
    _Entity = {};
    _FragmentType.Reset();
    _SelectionModel.Reset();
    _DiffLabels.Reset();
    _Properties.Reset();
    _View.Reset();
    _Mounted = false;
}

// =====================================================================================================================

auto FCkInspector_DynamicFragments::BuildFragmentWidget(
    const FCk_Handle& Entity,
    const FInstancedStruct& InFragment) -> TSharedRef<SWidget>
{
    const auto* ScriptStruct = InFragment.GetScriptStruct();
    if (NOT ck::IsValid(ScriptStruct))
    { return SNullWidget::NullWidget; }

    const auto* StructMemory = InFragment.GetMemory();
    if (NOT StructMemory)
    { return SNullWidget::NullWidget; }

    // ---- Build the property grid for this fragment

    auto Grid = SNew(SGridPanel)
        .FillColumn(0, 0.5f)
        .FillColumn(1, 0.5f);

    auto Row = 0;

    for (TFieldIterator<FProperty> PropIt(ScriptStruct); PropIt; ++PropIt)
    {
        const auto* Property = *PropIt;
        const auto PropertyName = ck::dynamic::Resolve_PropertyDisplayName(ScriptStruct, Property);

        // ---- FCk_Handle: clickable navigation

        const auto* StructProp = CastField<FStructProperty>(Property);
        const auto IsHandle = StructProp && StructProp->Struct &&
            (StructProp->Struct == FCk_Handle::StaticStruct() ||
             StructProp->Struct->IsChildOf(FCk_Handle::StaticStruct()));

        if (IsHandle)
        {
            const auto* HandlePtr = StructProp->ContainerPtrToValuePtr<FCk_Handle>(StructMemory);
            const auto HandleValue = *HandlePtr;

            // Property name in the key column (plain label - the value column is the
            // clickable navigator now, so the key no longer needs to be a button).
            Grid->AddSlot(0, Row)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(PropertyName))
                    .ColorAndOpacity(CkStyle::TextDim())
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    .ToolTipText(FText::FromString(PropertyName))
                ];

            // Clickable entity ref in the value column.
            Grid->AddSlot(1, Row)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SCkDebug_EntityRef)
                    .Entity(HandleValue)
                    .ShowName(true)
                ];
        }
        else
        {
            // ---- All other types: format with type-aware coloring

            const auto [ValueStr, ValueColor] = FormatProperty(ScriptStruct, Property, StructMemory);

            Grid->AddSlot(0, Row)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(PropertyName))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    .ToolTipText(FText::FromString(PropertyName))
                ];

            Grid->AddSlot(1, Row)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(ValueStr))
                    .ColorAndOpacity(ValueColor)
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    .ToolTipText(FText::FromString(ValueStr))
                ];
        }

        Row++;
    }

    // ---- Fragment-level verbs -------------------------------------------------
    // This inspector already holds the fragment's UScriptStruct per section, which is exactly what the
    // two type-addressed requests take - so the verbs cost nothing to wire. The struct is held WEAKLY
    // (it is a UObject) and re-resolved on every fire; the entity is captured by value and re-validated.
    //
    // Mark-Replication-Dirty is only OFFERED when the fragment was registered as replicated: the Util
    // ensures otherwise (CkDynamic_Utils.cpp:624), and a button whose only outcome is an ensure is worse
    // than no button. Replicated-ness is config, so the choice is made once at compose time.
    const auto CapturedEntity = Entity;
    const auto WeakStruct    = TWeakObjectPtr<const UScriptStruct>{ScriptStruct};

    const auto IsReplicated = ck::IsValid(Entity) &&
        Entity.Has<ck::FFragment_DynamicFragment_ReplicatedTypes>() &&
        Entity.Get<ck::FFragment_DynamicFragment_ReplicatedTypes>().Get_Types().Contains(ScriptStruct);

    auto Actions = TArray<FCkInspector_Action>{};

    Actions.Add(FCkInspector_Action
    {
        FText::FromString(TEXT("Remove")),
        FText::FromString(TEXT("Request_TryRemove - drops this dynamic fragment from the entity. No-op if it is already gone.")),
        [CapturedEntity, WeakStruct]()
        {
            auto MutableEntity = CapturedEntity;
            const auto* Struct = WeakStruct.Get();

            FInstancedStruct* Fragment = nullptr;
            if (NOT TryResolveFragment(MutableEntity, Struct, Fragment)
                || NOT ck::DebugRequestGate::Evaluate(
                    MutableEntity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
            { return; }

            UCk_Utils_DynamicFragment_UE::Request_TryRemove(MutableEntity, Struct, {});
        }
    });

    if (IsReplicated)
    {
        Actions.Add(FCkInspector_Action
        {
            FText::FromString(TEXT("Mark Rep Dirty")),
            FText::FromString(TEXT("Request_MarkReplicationDirty - re-sends this fragment on the next replication pass.")),
            [CapturedEntity, WeakStruct]()
            {
                auto MutableEntity = CapturedEntity;
                const auto* Struct = WeakStruct.Get();

                FInstancedStruct* Fragment = nullptr;
                const bool bStillReplicated = TryResolveFragment(MutableEntity, Struct, Fragment)
                    && MutableEntity.Has<ck::FFragment_DynamicFragment_ReplicatedTypes>()
                    && MutableEntity.Get<ck::FFragment_DynamicFragment_ReplicatedTypes>()
                        .Get_Types().Contains(Struct);
                if (NOT bStillReplicated
                    || NOT ck::DebugRequestGate::Evaluate(
                        MutableEntity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
                { return; }

                UCk_Utils_DynamicFragment_UE::Request_MarkReplicationDirty(MutableEntity, Struct, {});
            },
            ECk_DebugRequest_Requirement::AuthorityOnly
        });
    }

    auto ActionBuilder = FCkInspectorWidgetBuilder();
    ActionBuilder.AddActionRow(FText::FromString(TEXT("Fragment:")), Actions);

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            Grid
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            ActionBuilder.Build(Entity)
        ];
}

// =====================================================================================================================

auto FCkInspector_DynamicFragments::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    static_cast<void>(InDeltaTime);
    auto FragmentKeys = TArray<FString>{};
    if (ck::IsValid(Entity) && Entity.Has<ck::FFragment_DynamicFragment_Data>())
    {
        for (const FInstancedStruct& Fragment : UCk_Utils_DynamicFragment_UE::Get_AllFragments(Entity))
        {
            if (const UScriptStruct* FragmentType = Fragment.GetScriptStruct(); ck::IsValid(FragmentType))
            { FragmentKeys.Add(FragmentType->GetPathName()); }
        }
    }
    FragmentKeys.Sort();
    if (FragmentKeys != _LastFragmentKeys)
    {
        _LastFragmentKeys = MoveTemp(FragmentKeys);
        RequestRebuild();
    }

    _LastAuthoredLoadError.Reset();
    for (const TWeakPtr<SCkInspector_DynamicFragmentAuthored>& Weak : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_DynamicFragmentAuthored> Instance = Weak.Pin();
            Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        {
            _LastAuthoredLoadError = Instance->Get_LoadError();
            break;
        }
    }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_DynamicFragmentAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

FCkInspector_DynamicFragments::~FCkInspector_DynamicFragments()
{
    OnDeactivated();
}

auto FCkInspector_DynamicFragments::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_DynamicFragmentAuthored>& Weak : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_DynamicFragmentAuthored> Instance = Weak.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
    _LastFragmentKeys.Reset();
    _LastAuthoredLoadError.Reset();
}
