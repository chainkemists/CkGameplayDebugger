#include "CkInspector_DynamicFragments.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDynamic/CkDynamic_Fragment.h"
#include "CkDynamic/CkDynamic_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "GameplayTagContainer.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

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

    for (const auto& Fragment : AllFragments)
    {
        const auto* ScriptStruct = Fragment.GetScriptStruct();
        if (NOT ck::IsValid(ScriptStruct))
        { continue; }

        Sections.Add(FInspectorSection
        {
            ScriptStruct->GetDisplayNameText(),
            BuildFragmentWidget(Entity, Fragment)
        });
    }

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
            auto NumericValue = int64{};
            EnumProp->GetUnderlyingProperty()->GetValue_InContainer(InContainer, &NumericValue);
            const auto DisplayName = Enum->GetDisplayNameTextByValue(NumericValue).ToString();
            return { DisplayName, CkStyle::Value_Enum() };
        }

        if (const auto* ByteProp = CastField<FByteProperty>(InProperty);
            ByteProp && ByteProp->Enum)
        {
            const auto ByteValue = *static_cast<const uint8*>(ValuePtr);
            const auto DisplayName = ByteProp->Enum->GetDisplayNameTextByValue(ByteValue).ToString();
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

        // ---- Struct types (order matters — check specific before generic fallback)

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
        const auto PropertyName = Property->GetDisplayNameText().ToString();

        // ---- FCk_Handle: clickable navigation

        const auto* StructProp = CastField<FStructProperty>(Property);
        const auto IsHandle = StructProp && StructProp->Struct &&
            (StructProp->Struct == FCk_Handle::StaticStruct() ||
             StructProp->Struct->IsChildOf(FCk_Handle::StaticStruct()));

        if (IsHandle)
        {
            const auto* HandlePtr = StructProp->ContainerPtrToValuePtr<FCk_Handle>(StructMemory);
            const auto HandleValue = *HandlePtr;

            // Property name in the key column (plain label — the value column is the
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

            const auto [ValueStr, ValueColor] = FormatProperty(Property, StructMemory);

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
    // two type-addressed requests take — so the verbs cost nothing to wire. The struct is held WEAKLY
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
        FText::FromString(TEXT("Request_TryRemove — drops this dynamic fragment from the entity. No-op if it is already gone.")),
        [CapturedEntity, WeakStruct]()
        {
            auto MutableEntity = CapturedEntity;
            const auto* Struct = WeakStruct.Get();

            if (ck::Is_NOT_Valid(MutableEntity) || NOT ck::IsValid(Struct)) { return; }

            UCk_Utils_DynamicFragment_UE::Request_TryRemove(MutableEntity, Struct, {});
        }
    });

    if (IsReplicated)
    {
        Actions.Add(FCkInspector_Action
        {
            FText::FromString(TEXT("Mark Rep Dirty")),
            FText::FromString(TEXT("Request_MarkReplicationDirty — re-sends this fragment on the next replication pass.")),
            [CapturedEntity, WeakStruct]()
            {
                auto MutableEntity = CapturedEntity;
                const auto* Struct = WeakStruct.Get();

                if (ck::Is_NOT_Valid(MutableEntity) || NOT ck::IsValid(Struct)) { return; }

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
}
