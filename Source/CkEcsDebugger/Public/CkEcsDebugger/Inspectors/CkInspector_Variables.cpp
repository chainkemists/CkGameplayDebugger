#include "CkInspector_Variables.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkVariables/CkUnrealVariables_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Variables)

// =====================================================================================================================

auto FCkInspector_Variables::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Variables"));
}

auto FCkInspector_Variables::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Variable_Bool,
        ck::FFragment_Variable_Byte,
        ck::FFragment_Variable_Int32,
        ck::FFragment_Variable_Int64,
        ck::FFragment_Variable_Float,
        ck::FFragment_Variable_Name,
        ck::FFragment_Variable_String,
        ck::FFragment_Variable_Text,
        ck::FFragment_Variable_Vector,
        ck::FFragment_Variable_Vector2D,
        ck::FFragment_Variable_Rotator,
        ck::FFragment_Variable_Transform,
        ck::FFragment_Variable_GameplayTag,
        ck::FFragment_Variable_GameplayTagContainer,
        ck::FFragment_Variable_LinearColor,
        ck::FFragment_Variable_Entity>();
}

auto FCkInspector_Variables::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildVariablesGrid(Entity);
}

// =====================================================================================================================

namespace
{
    template <typename T_Fragment, typename T_Formatter>
    auto AddVariableRows(
        FCkInspectorWidgetBuilder& InBuilder,
        const FCk_Handle& InEntity,
        const FText& InTypeName,
        const FLinearColor& InColor,
        T_Formatter InFormatter) -> void
    {
        if (NOT InEntity.Has<T_Fragment>())
        { return; }

        const auto& Variables = InEntity.Get<T_Fragment>().Get_Variables();
        if (Variables.IsEmpty())
        { return; }

        InBuilder.AddHeader(InTypeName);

        for (const auto& [Name, Value] : Variables)
        {
            const auto NameStr = Name.ToString();
            const auto ValueStr = InFormatter(Value);

            InBuilder.AddRow(
                FText::FromString(NameStr),
                [ValueStr](const FCk_Handle& E) { return FText::FromString(ValueStr); },
                InColor);
        }
    }

    // Fixed-precision components for AddAlignedNumericRow, in X/Y/Z order so the row's index-based
    // axis coloring lands on the right axis. Values are snapshots — every row in this inspector is
    // (the variable maps are read once at build time), so nothing changes about liveness here.
    auto Make_VariableNumericComponents(
        const TArray<double>& InValues) -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(InValues.Num());

        for (const auto& Value : InValues)
        {
            Components.Emplace(FText::FromString(FString::Printf(TEXT("%.3f"), Value)));
        }

        return Components;
    }
}

// =====================================================================================================================

auto FCkInspector_Variables::BuildVariablesGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Bool (conditional color per value)
    if (Entity.Has<ck::FFragment_Variable_Bool>())
    {
        const auto& Variables = Entity.Get<ck::FFragment_Variable_Bool>().Get_Variables();
        if (NOT Variables.IsEmpty())
        {
            Builder.AddHeader(FText::FromString(TEXT("Bool")));
            for (const auto& [Name, Value] : Variables)
            {
                const auto NameStr = Name.ToString();
                const auto ValueStr = Value ? TEXT("true") : TEXT("false");
                const auto ValueColor = Value ? CkStyle::Value_Bool_True() : CkStyle::Value_Bool_False();
                Builder.AddRow(
                    FText::FromString(NameStr),
                    [ValueStr](const FCk_Handle& E) { return FText::FromString(ValueStr); },
                    ValueColor);
            }
        }
    }

    // ---- Numeric types
    AddVariableRows<ck::FFragment_Variable_Byte>(Builder, Entity, FText::FromString(TEXT("Byte")),
        CkStyle::Value_Numeric(),
        [](uint8 InValue) -> FString { return FString::Printf(TEXT("%d"), InValue); });

    AddVariableRows<ck::FFragment_Variable_Int32>(Builder, Entity, FText::FromString(TEXT("Int32")),
        CkStyle::Value_Numeric(),
        [](int32 InValue) -> FString { return FString::Printf(TEXT("%d"), InValue); });

    AddVariableRows<ck::FFragment_Variable_Int64>(Builder, Entity, FText::FromString(TEXT("Int64")),
        CkStyle::Value_Numeric(),
        [](int64 InValue) -> FString { return FString::Printf(TEXT("%lld"), InValue); });

    AddVariableRows<ck::FFragment_Variable_Float>(Builder, Entity, FText::FromString(TEXT("Float")),
        CkStyle::Value_Numeric(),
        [](float InValue) -> FString { return FString::Printf(TEXT("%.3f"), InValue); });

    // ---- String types
    AddVariableRows<ck::FFragment_Variable_Name>(Builder, Entity, FText::FromString(TEXT("Name")),
        CkStyle::Value_String(),
        [](const FName& InValue) -> FString { return InValue.IsNone() ? TEXT("(None)") : InValue.ToString(); });

    AddVariableRows<ck::FFragment_Variable_String>(Builder, Entity, FText::FromString(TEXT("String")),
        CkStyle::Value_String(),
        [](const FString& InValue) -> FString { return InValue.IsEmpty() ? TEXT("(Empty)") : InValue; });

    AddVariableRows<ck::FFragment_Variable_Text>(Builder, Entity, FText::FromString(TEXT("Text")),
        CkStyle::Value_String(),
        [](const FText& InValue) -> FString { return InValue.IsEmpty() ? TEXT("(Empty)") : InValue.ToString(); });

    // ---- Math types
    // Vector / Vector2D render as aligned numeric rows — right-aligned fixed-width columns with the
    // X/Y/Z axis colors, the shape the Transform inspector uses. The axis colors carry what
    // FVector::ToString()'s inline "X= Y= Z=" labels used to, and several vector variables now line
    // up column-for-column instead of being ragged strings.
    //
    // Rotator and Transform deliberately STAY textual: FRotator::ToString() prints "P= Y= R=", whose
    // order does not match this row's positional X/Y/Z coloring, and a Transform is three vectors —
    // neither fits one aligned row without inventing a label convention.
    if (Entity.Has<ck::FFragment_Variable_Vector>())
    {
        const auto& Variables = Entity.Get<ck::FFragment_Variable_Vector>().Get_Variables();
        if (NOT Variables.IsEmpty())
        {
            Builder.AddHeader(FText::FromString(TEXT("Vector")));

            for (const auto& [Name, Value] : Variables)
            {
                Builder.AddAlignedNumericRow(
                    FText::FromString(Name.ToString()),
                    Make_VariableNumericComponents(TArray<double>{ Value.X, Value.Y, Value.Z }));
            }
        }
    }

    if (Entity.Has<ck::FFragment_Variable_Vector2D>())
    {
        const auto& Variables = Entity.Get<ck::FFragment_Variable_Vector2D>().Get_Variables();
        if (NOT Variables.IsEmpty())
        {
            Builder.AddHeader(FText::FromString(TEXT("Vector2D")));

            for (const auto& [Name, Value] : Variables)
            {
                Builder.AddAlignedNumericRow(
                    FText::FromString(Name.ToString()),
                    Make_VariableNumericComponents(TArray<double>{ Value.X, Value.Y }));
            }
        }
    }

    AddVariableRows<ck::FFragment_Variable_Rotator>(Builder, Entity, FText::FromString(TEXT("Rotator")),
        CkStyle::Value_Math(),
        [](const FRotator& InValue) -> FString { return InValue.ToString(); });

    AddVariableRows<ck::FFragment_Variable_Transform>(Builder, Entity, FText::FromString(TEXT("Transform")),
        CkStyle::Value_Math(),
        [](const FTransform& InValue) -> FString { return InValue.ToString(); });

    // ---- Tag types
    AddVariableRows<ck::FFragment_Variable_GameplayTag>(Builder, Entity, FText::FromString(TEXT("GameplayTag")),
        CkStyle::Value_Tag(),
        [](const FGameplayTag& InValue) -> FString { return InValue.IsValid() ? InValue.GetTagName().ToString() : TEXT("(None)"); });

    // A tag container printed through FGameplayTagContainer::ToString() is one long comma blob that
    // wraps badly and reads worse the more tags it holds. Chips give it the set shape it actually
    // has. Snapshot semantics are unchanged — this inspector already read every variable map once at
    // build time.
    if (Entity.Has<ck::FFragment_Variable_GameplayTagContainer>())
    {
        const auto& Variables = Entity.Get<ck::FFragment_Variable_GameplayTagContainer>().Get_Variables();
        if (NOT Variables.IsEmpty())
        {
            Builder.AddHeader(FText::FromString(TEXT("TagContainer")));

            for (const auto& [Name, Value] : Variables)
            {
                const auto NameText = FText::FromString(Name.ToString());

                if (Value.IsEmpty())
                {
                    Builder.AddRow(
                        NameText,
                        [](const FCk_Handle&) { return FText::FromString(TEXT("(Empty)")); },
                        CkStyle::Value_Tag());
                    continue;
                }

                auto Chips = TArray<FCkInspector_Chip>{};
                Chips.Reserve(Value.Num());

                for (const auto& Tag : Value)
                {
                    Chips.Add(FCkInspector_Chip{ FText::FromName(Tag.GetTagName()), ECk_Tone::Neutral });
                }

                Builder.AddChipsRow(NameText, Chips);
            }
        }
    }

    // ---- LinearColor
    AddVariableRows<ck::FFragment_Variable_LinearColor>(Builder, Entity, FText::FromString(TEXT("LinearColor")),
        CkStyle::Value_Math(),
        [](const FLinearColor& InValue) -> FString { return InValue.ToString(); });

    // ---- Entity (clickable to navigate via SCkDebug_EntityRef)
    if (Entity.Has<ck::FFragment_Variable_Entity>())
    {
        const auto& EntityVariables = Entity.Get<ck::FFragment_Variable_Entity>().Get_Variables();
        if (NOT EntityVariables.IsEmpty())
        {
            Builder.AddHeader(FText::FromString(TEXT("Entity")));

            for (const auto& [Name, Value] : EntityVariables)
            {
                const auto NameStr = Name.ToString();
                const auto EntityHandle = Value;

                Builder.AddWidgetRow(
                    FText::FromString(NameStr),
                    SNew(SCkDebug_EntityRef)
                        .Entity(EntityHandle)
                        .ShowName(true));
            }
        }
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Variables::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
