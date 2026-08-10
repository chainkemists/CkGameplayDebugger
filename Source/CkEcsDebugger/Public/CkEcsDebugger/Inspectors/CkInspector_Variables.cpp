#include "CkInspector_Variables.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkVariables/CkUnrealVariables_Fragment.h"
#include "CkVariables/CkUnrealVariables_Utils.h"

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

namespace ck_inspector_variables
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

    // ----------------------------------------------------------------------------------------------------------------
    // EDITOR SWAP
    //
    // Same one-row-per-variable shape as AddVariableRows above; only the VALUE side changes, from a
    // build-time snapshot string to a live-reading builder control. The variable maps are keyed by
    // FName, so the write goes through Set_ByName — the FGameplayTag `Set` overload the census names
    // is the same UtilsType::Set behind a tag key we do not have here.
    //
    // All variable writes are LocalOk: TUtils_Variables::Set is a plain fragment-map write with no
    // authority-gated processor behind it.
    // ----------------------------------------------------------------------------------------------------------------

    template <typename T_Utils, typename T_Value>
    auto Get_VariableValue(
        const FCk_Handle& InEntity,
        FName             InVariableName) -> T_Value
    {
        if (ck::Is_NOT_Valid(InEntity))
        { return T_Value{}; }

        auto Result = ECk_SucceededFailed::Failed;
        return T_Utils::Get_ByName(InEntity, InVariableName, ECk_Recursion::NotRecursive, Result);
    }

    template <typename T_Utils, typename T_Value>
    auto Set_VariableValue(
        const FCk_Handle& InEntity,
        FName             InVariableName,
        T_Value           InValue) -> void
    {
        auto MutableEntity = InEntity;

        if (ck::Is_NOT_Valid(MutableEntity))
        { return; }

        T_Utils::Set_ByName(MutableEntity, InVariableName, InValue);
    }

    /**
     * Header + one row per variable, where the row itself is composed by the caller's type-specific
     * lambda. Keeps the iteration and the header emission in ONE place so an editable type and a
     * read-only type still produce the same section shape.
     */
    template <typename T_Fragment, typename T_RowBuilder>
    auto AddEditableVariableRows(
        FCkInspectorWidgetBuilder& InBuilder,
        const FCk_Handle&          InEntity,
        const FText&               InTypeName,
        T_RowBuilder               InRowBuilder) -> void
    {
        if (NOT InEntity.Has<T_Fragment>())
        { return; }

        const auto& Variables = InEntity.Get<T_Fragment>().Get_Variables();
        if (Variables.IsEmpty())
        { return; }

        InBuilder.AddHeader(InTypeName);

        for (const auto& [Name, Value] : Variables)
        {
            InRowBuilder(InBuilder, InEntity, Name, Value);
        }
    }

    // Fixed-precision components for AddAlignedNumericRow, in X/Y/Z order so the row's index-based
    // axis coloring lands on the right axis. Values are snapshots — the read-only rows that still use
    // this read their variable map once at build time.
    inline auto Make_VariableNumericComponents(
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
    namespace vars = ck_inspector_variables;

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    // ---- Bool
    // The old row coloured true/false through CkStyle::Value_Bool_*; the switch carries that state in
    // its own on/off rendering, and the Hidden EditControlStyle falls back to plain "true"/"false".
    vars::AddEditableVariableRows<ck::FFragment_Variable_Bool>(Builder, Entity, FText::FromString(TEXT("Bool")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, bool)
        {
            InBuilder.AddToggleRow(
                FText::FromString(InName.ToString()),
                TAttribute<bool>::CreateLambda([InEntity, InName]()
                { return vars::Get_VariableValue<UCk_Utils_Variables_Bool_UE, bool>(InEntity, InName); }),
                [InEntity, InName](bool InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_Bool_UE, bool>(InEntity, InName, InValue); },
                ECk_DebugRequest_Requirement::LocalOk);
        });

    // ---- Numeric types
    vars::AddEditableVariableRows<ck::FFragment_Variable_Byte>(Builder, Entity, FText::FromString(TEXT("Byte")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, uint8)
        {
            InBuilder.AddIntegerRow(
                FText::FromString(InName.ToString()),
                TAttribute<int32>::CreateLambda([InEntity, InName]()
                { return static_cast<int32>(vars::Get_VariableValue<UCk_Utils_Variables_Byte_UE, uint8>(InEntity, InName)); }),
                [InEntity, InName](int32 InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_Byte_UE, uint8>(InEntity, InName, static_cast<uint8>(FMath::Clamp(InValue, 0, 255))); },
                TOptional<int32>{0},
                TOptional<int32>{255},
                ECk_DebugRequest_Requirement::LocalOk);
        });

    vars::AddEditableVariableRows<ck::FFragment_Variable_Int32>(Builder, Entity, FText::FromString(TEXT("Int32")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, int32)
        {
            InBuilder.AddIntegerRow(
                FText::FromString(InName.ToString()),
                TAttribute<int32>::CreateLambda([InEntity, InName]()
                { return vars::Get_VariableValue<UCk_Utils_Variables_Int32_UE, int32>(InEntity, InName); }),
                [InEntity, InName](int32 InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_Int32_UE, int32>(InEntity, InName, InValue); },
                TOptional<int32>{},
                TOptional<int32>{},
                ECk_DebugRequest_Requirement::LocalOk);
        });

    // Int64 gets the editor only while its CURRENT value is representable as int32 — AddIntegerRow is
    // int32-typed, so a wider value would be displayed truncated and committing it back would silently
    // destroy the high bits. Out-of-range int64s keep the read-only row and print in full.
    vars::AddEditableVariableRows<ck::FFragment_Variable_Int64>(Builder, Entity, FText::FromString(TEXT("Int64")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, int64 InCurrent)
        {
            const auto NameText = FText::FromString(InName.ToString());

            if (InCurrent > static_cast<int64>(MAX_int32) || InCurrent < static_cast<int64>(MIN_int32))
            {
                const auto ValueStr = FString::Printf(TEXT("%lld"), InCurrent);
                InBuilder.AddRow(NameText,
                    [ValueStr](const FCk_Handle&) { return FText::FromString(ValueStr); },
                    CkStyle::Value_Numeric());
                return;
            }

            InBuilder.AddIntegerRow(
                NameText,
                TAttribute<int32>::CreateLambda([InEntity, InName]()
                { return static_cast<int32>(vars::Get_VariableValue<UCk_Utils_Variables_Int64_UE, int64>(InEntity, InName)); }),
                [InEntity, InName](int32 InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_Int64_UE, int64>(InEntity, InName, static_cast<int64>(InValue)); },
                TOptional<int32>{},
                TOptional<int32>{},
                ECk_DebugRequest_Requirement::LocalOk);
        });

    vars::AddEditableVariableRows<ck::FFragment_Variable_Float>(Builder, Entity, FText::FromString(TEXT("Float")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, float)
        {
            InBuilder.AddNumericRow(
                FText::FromString(InName.ToString()),
                TAttribute<float>::CreateLambda([InEntity, InName]()
                { return vars::Get_VariableValue<UCk_Utils_Variables_Float_UE, float>(InEntity, InName); }),
                [InEntity, InName](float InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_Float_UE, float>(InEntity, InName, InValue); },
                TOptional<float>{},
                TOptional<float>{},
                ECk_DebugRequest_Requirement::LocalOk);
        });

    // ---- String types
    vars::AddEditableVariableRows<ck::FFragment_Variable_Name>(Builder, Entity, FText::FromString(TEXT("Name")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, const FName&)
        {
            InBuilder.AddNameEntryRow(
                FText::FromString(InName.ToString()),
                TAttribute<FText>::CreateLambda([InEntity, InName]()
                {
                    const auto Value = vars::Get_VariableValue<UCk_Utils_Variables_Name_UE, FName>(InEntity, InName);
                    return FText::FromString(Value.IsNone() ? FString{TEXT("(None)")} : Value.ToString());
                }),
                [InEntity, InName](FName InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_Name_UE, FName>(InEntity, InName, InValue); },
                ECk_DebugRequest_Requirement::LocalOk);
        });

    // String rides the name-entry row: the builder's text entry hands back an FName, which round-trips
    // any string a debug session realistically types. (A string longer than NAME_SIZE would not, which
    // is why this is the entry row and not a general multi-line editor.)
    vars::AddEditableVariableRows<ck::FFragment_Variable_String>(Builder, Entity, FText::FromString(TEXT("String")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, const FString&)
        {
            InBuilder.AddNameEntryRow(
                FText::FromString(InName.ToString()),
                TAttribute<FText>::CreateLambda([InEntity, InName]()
                {
                    const auto Value = vars::Get_VariableValue<UCk_Utils_Variables_String_UE, FString>(InEntity, InName);
                    return FText::FromString(Value.IsEmpty() ? FString{TEXT("(Empty)")} : Value);
                }),
                [InEntity, InName](FName InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_String_UE, FString>(InEntity, InName, InValue.ToString()); },
                ECk_DebugRequest_Requirement::LocalOk);
        });

    // FText stays READ-ONLY: committing one through a text entry would replace a localizable value with
    // a culture-invariant literal, which is a data change the debugger has no business making silently.
    vars::AddVariableRows<ck::FFragment_Variable_Text>(Builder, Entity, FText::FromString(TEXT("Text")),
        CkStyle::Value_String(),
        [](const FText& InValue) -> FString { return InValue.IsEmpty() ? TEXT("(Empty)") : InValue.ToString(); });

    // ---- Math types
    // Vector is now the builder's three-editor vector row: same aligned fixed-width columns and X/Y/Z
    // axis colors as before (that IS its read-only fallback), with each component committable.
    //
    // Rotator joins it through AddRotatorRow, which lays the components out as (Roll, Pitch, Yaw) so
    // index 0/1/2 colouring matches the axis each angle turns about — the reason the old textual row
    // existed (FRotator::ToString()'s "P= Y= R=" order) is exactly what that row fixes. The label
    // states the order, as CkInspector_Transform does.
    //
    // Vector2D and Transform stay read-only: the builder has no two-component editor row, and splitting
    // a Vector2D into two separate numeric rows would change the row layout this swap is meant to
    // preserve; a Transform is three vectors and has no single-row form at all.
    vars::AddEditableVariableRows<ck::FFragment_Variable_Vector>(Builder, Entity, FText::FromString(TEXT("Vector")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, const FVector&)
        {
            InBuilder.AddVectorRow(
                FText::FromString(InName.ToString()),
                TAttribute<FVector>::CreateLambda([InEntity, InName]()
                { return vars::Get_VariableValue<UCk_Utils_Variables_Vector_UE, FVector>(InEntity, InName); }),
                [InEntity, InName](const FVector& InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_Vector_UE, FVector>(InEntity, InName, InValue); },
                ECk_DebugRequest_Requirement::LocalOk);
        });

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
                    vars::Make_VariableNumericComponents(TArray<double>{ Value.X, Value.Y }));
            }
        }
    }

    vars::AddEditableVariableRows<ck::FFragment_Variable_Rotator>(Builder, Entity, FText::FromString(TEXT("Rotator")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, const FRotator&)
        {
            InBuilder.AddRotatorRow(
                FText::FromString(ck::Format_UE(TEXT("{} (R,P,Y)"), InName.ToString())),
                TAttribute<FRotator>::CreateLambda([InEntity, InName]()
                { return vars::Get_VariableValue<UCk_Utils_Variables_Rotator_UE, FRotator>(InEntity, InName); }),
                [InEntity, InName](const FRotator& InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_Rotator_UE, FRotator>(InEntity, InName, InValue); },
                ECk_DebugRequest_Requirement::LocalOk);
        });

    vars::AddVariableRows<ck::FFragment_Variable_Transform>(Builder, Entity, FText::FromString(TEXT("Transform")),
        CkStyle::Value_Math(),
        [](const FTransform& InValue) -> FString { return InValue.ToString(); });

    // ---- Tag types
    // Text entry, not a picker: SGameplayTagPicker lives in GameplayTagsEditor, which no debugger module
    // depends on. Unknown text is left uncommitted rather than clearing the variable (builder contract).
    vars::AddEditableVariableRows<ck::FFragment_Variable_GameplayTag>(Builder, Entity, FText::FromString(TEXT("GameplayTag")),
        [](FCkInspectorWidgetBuilder& InBuilder, const FCk_Handle& InEntity, const FName& InName, const FGameplayTag&)
        {
            InBuilder.AddTagEntryRow(
                FText::FromString(InName.ToString()),
                TAttribute<FText>::CreateLambda([InEntity, InName]()
                {
                    const auto Value = vars::Get_VariableValue<UCk_Utils_Variables_GameplayTag_UE, FGameplayTag>(InEntity, InName);
                    return FText::FromString(Value.IsValid() ? Value.GetTagName().ToString() : FString{TEXT("(None)")});
                }),
                [InEntity, InName](FGameplayTag InValue)
                { vars::Set_VariableValue<UCk_Utils_Variables_GameplayTag_UE, FGameplayTag>(InEntity, InName, InValue); },
                ECk_DebugRequest_Requirement::LocalOk);
        });

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

    // ---- LinearColor (read-only: no colour-swatch editor row exists in the builder vocabulary)
    vars::AddVariableRows<ck::FFragment_Variable_LinearColor>(Builder, Entity, FText::FromString(TEXT("LinearColor")),
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
