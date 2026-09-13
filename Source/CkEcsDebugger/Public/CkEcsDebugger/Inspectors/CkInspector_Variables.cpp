#include "CkInspector_Variables.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkVariables/CkUnrealVariables_Fragment.h"
#include "CkVariables/CkUnrealVariables_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Variables)

// =====================================================================================================================

namespace ck_inspector_variables
{
    auto Is_Destroying(const FCk_Handle& InEntity) -> bool;
}

// =====================================================================================================================

auto FCkInspector_Variables::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Variables"));
}

auto FCkInspector_Variables::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck_inspector_variables::Is_Destroying(Entity))
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
    const TSharedRef<SWidget> Native = BuildVariablesGrid(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return Native; }

    auto DiffLabels = TSet<FString>{};
    const auto CaptureDiffs = [&DiffLabels](const auto& Variables)
    {
        for (const auto& [Name, Value] : Variables)
        {
            if (FCkInspector_DiffMarkScope::Is_LabelMarked(Name.ToString()))
            { DiffLabels.Add(Name.ToString()); }
        }
    };
    if (ck::IsValid(Entity))
    {
        if (Entity.Has<ck::FFragment_Variable_Bool>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Bool>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Byte>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Byte>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Int32>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Int32>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Int64>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Int64>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Float>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Float>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Name>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Name>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_String>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_String>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Text>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Text>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Vector>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Vector>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Vector2D>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Vector2D>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Rotator>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Rotator>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Transform>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Transform>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_GameplayTag>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_GameplayTag>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_GameplayTagContainer>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_GameplayTagContainer>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_LinearColor>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_LinearColor>().Get_Variables()); }
        if (Entity.Has<ck::FFragment_Variable_Entity>()) { CaptureDiffs(Entity.Get<ck::FFragment_Variable_Entity>().Get_Variables()); }
    }

    const TSharedRef<SCkInspector_VariablesAuthored> Authored = SNew(SCkInspector_VariablesAuthored)
        .Entity(Entity)
        .SelectionModel(Get_SelectionModel())
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return Native;
    }

    _LastAuthoredLoadError.Reset();
    _Instances.Add(Authored);
    return Authored;
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

namespace ck_inspector_variables
{
    struct FAuthoredRecord
    {
        FString Key;
        FString Type;
        FString Name;
        FString ValueText;
        FString Axis0Label;
        FString Axis1Label;
        FString Axis2Label;
        FString EntityId;
        FString EntityName;
        int32 IntegerValue = 0;
        float NumberValue = 0.0f;
        float Axis0 = 0.0f;
        float Axis1 = 0.0f;
        float Axis2 = 0.0f;
        bool BoolValue = false;
        bool HeaderVisible = false;
        bool BoolVisible = false;
        bool IntegerVisible = false;
        bool NumberVisible = false;
        bool TextInputVisible = false;
        bool ReadVisible = false;
        bool VectorVisible = false;
        bool Axis2Visible = false;
        bool EntityVisible = false;
        bool DiffMarked = false;
    };

    auto Make_TextField(const FString& InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)};
    }

    auto Make_BoolField(const bool InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = InValue};
    }

    auto Make_IntegerField(const int32 InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Integer, .Integer = InValue};
    }

    auto Make_NumberField(const float InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Number, .Number = InValue};
    }

    auto Make_Record(const FAuthoredRecord& In) -> FCkUiRecordData
    {
        auto Record = FCkUiRecordData{};
        Record.Key = In.Key;
        Record.Fields.Add(TEXT("type"), Make_TextField(In.Type));
        Record.Fields.Add(TEXT("name"), Make_TextField(In.Name));
        Record.Fields.Add(TEXT("value-text"), Make_TextField(In.ValueText));
        Record.Fields.Add(TEXT("axis-0-label"), Make_TextField(In.Axis0Label));
        Record.Fields.Add(TEXT("axis-1-label"), Make_TextField(In.Axis1Label));
        Record.Fields.Add(TEXT("axis-2-label"), Make_TextField(In.Axis2Label));
        Record.Fields.Add(TEXT("entity-id"), Make_TextField(In.EntityId));
        Record.Fields.Add(TEXT("entity-name"), Make_TextField(In.EntityName));
        Record.Fields.Add(TEXT("integer-value"), Make_IntegerField(In.IntegerValue));
        Record.Fields.Add(TEXT("number-value"), Make_NumberField(In.NumberValue));
        Record.Fields.Add(TEXT("axis-0"), Make_NumberField(In.Axis0));
        Record.Fields.Add(TEXT("axis-1"), Make_NumberField(In.Axis1));
        Record.Fields.Add(TEXT("axis-2"), Make_NumberField(In.Axis2));
        Record.Fields.Add(TEXT("bool-value"), Make_BoolField(In.BoolValue));
        Record.Fields.Add(TEXT("header-visible"), Make_BoolField(In.HeaderVisible));
        Record.Fields.Add(TEXT("bool-visible"), Make_BoolField(In.BoolVisible));
        Record.Fields.Add(TEXT("integer-visible"), Make_BoolField(In.IntegerVisible));
        Record.Fields.Add(TEXT("number-visible"), Make_BoolField(In.NumberVisible));
        Record.Fields.Add(TEXT("text-input-visible"), Make_BoolField(In.TextInputVisible));
        Record.Fields.Add(TEXT("read-visible"), Make_BoolField(In.ReadVisible));
        Record.Fields.Add(TEXT("vector-visible"), Make_BoolField(In.VectorVisible));
        Record.Fields.Add(TEXT("axis-2-visible"), Make_BoolField(In.Axis2Visible));
        Record.Fields.Add(TEXT("entity-visible"), Make_BoolField(In.EntityVisible));
        Record.Fields.Add(TEXT("diff-color"), FCkUiFieldValue{
            .Kind = ECkUiFieldKind::Color,
            .Color = In.DiffMarked ? CkStyle::Accent() : CkStyle::Text()});
        return Record;
    }

    auto Is_Destroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<
                ck::FTag_DestroyEntity_Initiate,
                ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown,
                ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

    template <typename T_Fragment>
    auto Has_NamedVariable(const FCk_Handle& InEntity, const FName InName) -> bool
    {
        return NOT Is_Destroying(InEntity)
            && InEntity.Has<T_Fragment>()
            && InEntity.Get<T_Fragment>().Get_Variables().Contains(InName);
    }
}

auto SCkInspector_VariablesAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _SelectionModel = InArgs._SelectionModel;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Refresh_Records() && Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_VariablesAuthored::~SCkInspector_VariablesAuthored()
{
    Release();
}

auto SCkInspector_VariablesAuthored::Get_IsAvailable() const -> bool
{
    return _Active
        && NOT ck_inspector_variables::Is_Destroying(_Entity)
        && _Entity.Has_Any<
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

auto SCkInspector_VariablesAuthored::Get_CanEdit() const -> bool
{
    return Get_IsAvailable()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_VariablesAuthored::Get_EditDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("Variables are unavailable."); }
    return ck::DebugRequestGate::Evaluate(
        _Entity, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString();
}

auto SCkInspector_VariablesAuthored::Refresh_Records() -> bool
{
    namespace vars = ck_inspector_variables;
    if (NOT _Records.IsValid())
    {
        const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreate({
            {TEXT("type"), ECkUiFieldKind::Text},
            {TEXT("name"), ECkUiFieldKind::Text},
            {TEXT("value-text"), ECkUiFieldKind::Text},
            {TEXT("axis-0-label"), ECkUiFieldKind::Text},
            {TEXT("axis-1-label"), ECkUiFieldKind::Text},
            {TEXT("axis-2-label"), ECkUiFieldKind::Text},
            {TEXT("entity-id"), ECkUiFieldKind::Text},
            {TEXT("entity-name"), ECkUiFieldKind::Text},
            {TEXT("integer-value"), ECkUiFieldKind::Integer},
            {TEXT("number-value"), ECkUiFieldKind::Number},
            {TEXT("axis-0"), ECkUiFieldKind::Number},
            {TEXT("axis-1"), ECkUiFieldKind::Number},
            {TEXT("axis-2"), ECkUiFieldKind::Number},
            {TEXT("bool-value"), ECkUiFieldKind::Bool},
            {TEXT("header-visible"), ECkUiFieldKind::Bool},
            {TEXT("bool-visible"), ECkUiFieldKind::Bool},
            {TEXT("integer-visible"), ECkUiFieldKind::Bool},
            {TEXT("number-visible"), ECkUiFieldKind::Bool},
            {TEXT("text-input-visible"), ECkUiFieldKind::Bool},
            {TEXT("read-visible"), ECkUiFieldKind::Bool},
            {TEXT("vector-visible"), ECkUiFieldKind::Bool},
            {TEXT("axis-2-visible"), ECkUiFieldKind::Bool},
            {TEXT("entity-visible"), ECkUiFieldKind::Bool},
            {TEXT("diff-color"), ECkUiFieldKind::Color},
        }, _Records);
        if (NOT CreateResult.Succeeded || NOT _Records.IsValid())
        {
            _LoadError = FString::Join(CreateResult.Errors, TEXT("\n"));
            return false;
        }
    }

    auto Records = TArray<FCkUiRecordData>{};
    auto RouteTypes = TMap<FString, FString>{};
    auto RouteNames = TMap<FString, FName>{};
    auto SeenTypes = TSet<FString>{};
    const auto Add = [this, &Records, &RouteTypes, &RouteNames, &SeenTypes](vars::FAuthoredRecord Record)
    {
        Record.HeaderVisible = NOT SeenTypes.Contains(Record.Type);
        SeenTypes.Add(Record.Type);
        Record.DiffMarked = _DiffLabels.Contains(Record.Name);
        RouteTypes.Add(Record.Key, Record.Type);
        RouteNames.Add(Record.Key, FName{Record.Name});
        Records.Add(vars::Make_Record(Record));
    };

    if (Get_IsAvailable())
    {
        if (_Entity.Has<ck::FFragment_Variable_Bool>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Bool>().Get_Variables())
            { Add({.Key = FString{TEXT("Bool/")} + Name.ToString(), .Type = TEXT("Bool"), .Name = Name.ToString(), .BoolValue = Value, .BoolVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Byte>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Byte>().Get_Variables())
            { Add({.Key = FString{TEXT("Byte/")} + Name.ToString(), .Type = TEXT("Byte"), .Name = Name.ToString(), .IntegerValue = Value, .IntegerVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Int32>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Int32>().Get_Variables())
            { Add({.Key = FString{TEXT("Int32/")} + Name.ToString(), .Type = TEXT("Int32"), .Name = Name.ToString(), .IntegerValue = Value, .IntegerVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Int64>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Int64>().Get_Variables())
            {
                const bool bEditable = Value >= static_cast<int64>(MIN_int32) && Value <= static_cast<int64>(MAX_int32);
                Add({.Key = FString{TEXT("Int64/")} + Name.ToString(), .Type = TEXT("Int64"), .Name = Name.ToString(),
                    .ValueText = FString::Printf(TEXT("%lld"), Value), .IntegerValue = static_cast<int32>(bEditable ? Value : 0),
                    .IntegerVisible = bEditable, .ReadVisible = NOT bEditable});
            }
        }
        if (_Entity.Has<ck::FFragment_Variable_Float>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Float>().Get_Variables())
            { Add({.Key = FString{TEXT("Float/")} + Name.ToString(), .Type = TEXT("Float"), .Name = Name.ToString(), .NumberValue = Value, .NumberVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Name>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Name>().Get_Variables())
            { Add({.Key = FString{TEXT("Name/")} + Name.ToString(), .Type = TEXT("Name"), .Name = Name.ToString(), .ValueText = Value.IsNone() ? TEXT("(None)") : Value.ToString(), .TextInputVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_String>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_String>().Get_Variables())
            { Add({.Key = FString{TEXT("String/")} + Name.ToString(), .Type = TEXT("String"), .Name = Name.ToString(), .ValueText = Value.IsEmpty() ? TEXT("(Empty)") : Value, .TextInputVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Text>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Text>().Get_Variables())
            { Add({.Key = FString{TEXT("Text/")} + Name.ToString(), .Type = TEXT("Text"), .Name = Name.ToString(), .ValueText = Value.IsEmpty() ? TEXT("(Empty)") : Value.ToString(), .ReadVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Vector>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Vector>().Get_Variables())
            { Add({.Key = FString{TEXT("Vector/")} + Name.ToString(), .Type = TEXT("Vector"), .Name = Name.ToString(), .Axis0Label = TEXT("X"), .Axis1Label = TEXT("Y"), .Axis2Label = TEXT("Z"), .Axis0 = static_cast<float>(Value.X), .Axis1 = static_cast<float>(Value.Y), .Axis2 = static_cast<float>(Value.Z), .VectorVisible = true, .Axis2Visible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Vector2D>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Vector2D>().Get_Variables())
            { Add({.Key = FString{TEXT("Vector2D/")} + Name.ToString(), .Type = TEXT("Vector2D"), .Name = Name.ToString(), .ValueText = Value.ToString(), .ReadVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Rotator>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Rotator>().Get_Variables())
            { Add({.Key = FString{TEXT("Rotator/")} + Name.ToString(), .Type = TEXT("Rotator"), .Name = Name.ToString(), .Axis0Label = TEXT("R"), .Axis1Label = TEXT("P"), .Axis2Label = TEXT("Y"), .Axis0 = static_cast<float>(Value.Roll), .Axis1 = static_cast<float>(Value.Pitch), .Axis2 = static_cast<float>(Value.Yaw), .VectorVisible = true, .Axis2Visible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Transform>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Transform>().Get_Variables())
            { Add({.Key = FString{TEXT("Transform/")} + Name.ToString(), .Type = TEXT("Transform"), .Name = Name.ToString(), .ValueText = Value.ToString(), .ReadVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_GameplayTag>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_GameplayTag>().Get_Variables())
            { Add({.Key = FString{TEXT("GameplayTag/")} + Name.ToString(), .Type = TEXT("GameplayTag"), .Name = Name.ToString(), .ValueText = Value.IsValid() ? Value.ToString() : TEXT("(None)"), .TextInputVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_GameplayTagContainer>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_GameplayTagContainer>().Get_Variables())
            { Add({.Key = FString{TEXT("TagContainer/")} + Name.ToString(), .Type = TEXT("TagContainer"), .Name = Name.ToString(), .ValueText = Value.IsEmpty() ? TEXT("(Empty)") : Value.ToString(), .ReadVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_LinearColor>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_LinearColor>().Get_Variables())
            { Add({.Key = FString{TEXT("LinearColor/")} + Name.ToString(), .Type = TEXT("LinearColor"), .Name = Name.ToString(), .ValueText = Value.ToString(), .ReadVisible = true}); }
        }
        if (_Entity.Has<ck::FFragment_Variable_Entity>())
        {
            for (const auto& [Name, Value] : _Entity.Get<ck::FFragment_Variable_Entity>().Get_Variables())
            {
                Add({.Key = FString{TEXT("Entity/")} + Name.ToString(), .Type = TEXT("Entity"), .Name = Name.ToString(),
                    .EntityId = ck::IsValid(Value) ? ck::Format_UE(TEXT("{}"), Value.Get_Entity()) : TEXT("--"),
                    .EntityName = ck::IsValid(Value) ? UCk_Utils_Handle_UE::Get_DebugName(Value).ToString() : TEXT("(Invalid)"),
                    .EntityVisible = true});
            }
        }
    }

    const FCkUiLoadResult RecordsResult = _Records->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded)
    {
        _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n"));
        return false;
    }
    _RouteTypes = MoveTemp(RouteTypes);
    _RouteNames = MoveTemp(RouteNames);
    return true;
}

auto SCkInspector_VariablesAuthored::Resolve_Route(
    const FString& InKey,
    FString& OutType,
    FName& OutName) const -> bool
{
    OutType.Reset();
    OutName = NAME_None;
    const FString* Type = _RouteTypes.Find(InKey);
    const FName* Name = _RouteNames.Find(InKey);
    if (NOT _Active || Type == nullptr || Name == nullptr || Name->IsNone())
    { return false; }
    OutType = *Type;
    OutName = *Name;
    return true;
}

auto SCkInspector_VariablesAuthored::Change_Bool(const FString& InKey, const bool bInValue) -> void
{
    auto Type = FString{}; auto Name = FName{};
    if (NOT Resolve_Route(InKey, Type, Name) || Type != TEXT("Bool") || NOT Get_CanEdit()
        || NOT ck_inspector_variables::Has_NamedVariable<ck::FFragment_Variable_Bool>(_Entity, Name))
    { return; }
    auto Mutable = _Entity;
    UCk_Utils_Variables_Bool_UE::Set_ByName(Mutable, Name, bInValue);
}

auto SCkInspector_VariablesAuthored::Commit_Integer(const FString& InKey, const int32 InValue) -> void
{
    namespace vars = ck_inspector_variables;
    auto Type = FString{}; auto Name = FName{};
    if (NOT Resolve_Route(InKey, Type, Name) || NOT Get_CanEdit()) { return; }
    auto Mutable = _Entity;
    if (Type == TEXT("Byte") && vars::Has_NamedVariable<ck::FFragment_Variable_Byte>(_Entity, Name))
    { UCk_Utils_Variables_Byte_UE::Set_ByName(Mutable, Name, static_cast<uint8>(FMath::Clamp(InValue, 0, 255))); }
    else if (Type == TEXT("Int32") && vars::Has_NamedVariable<ck::FFragment_Variable_Int32>(_Entity, Name))
    { UCk_Utils_Variables_Int32_UE::Set_ByName(Mutable, Name, InValue); }
    else if (Type == TEXT("Int64") && vars::Has_NamedVariable<ck::FFragment_Variable_Int64>(_Entity, Name))
    { UCk_Utils_Variables_Int64_UE::Set_ByName(Mutable, Name, static_cast<int64>(InValue)); }
}

auto SCkInspector_VariablesAuthored::Commit_Number(const FString& InKey, const float InValue) -> void
{
    auto Type = FString{}; auto Name = FName{};
    if (NOT FMath::IsFinite(InValue) || NOT Resolve_Route(InKey, Type, Name) || Type != TEXT("Float")
        || NOT Get_CanEdit()
        || NOT ck_inspector_variables::Has_NamedVariable<ck::FFragment_Variable_Float>(_Entity, Name))
    { return; }
    auto Mutable = _Entity;
    UCk_Utils_Variables_Float_UE::Set_ByName(Mutable, Name, InValue);
}

auto SCkInspector_VariablesAuthored::Commit_Text(const FString& InKey, const FText& InValue) -> void
{
    namespace vars = ck_inspector_variables;
    auto Type = FString{}; auto Name = FName{};
    if (NOT Resolve_Route(InKey, Type, Name) || NOT Get_CanEdit()) { return; }
    auto Mutable = _Entity;
    const FString Value = InValue.ToString();
    if (Type == TEXT("Name") && vars::Has_NamedVariable<ck::FFragment_Variable_Name>(_Entity, Name))
    { UCk_Utils_Variables_Name_UE::Set_ByName(Mutable, Name, FName{Value}); }
    else if (Type == TEXT("String") && vars::Has_NamedVariable<ck::FFragment_Variable_String>(_Entity, Name))
    { UCk_Utils_Variables_String_UE::Set_ByName(Mutable, Name, Value); }
    else if (Type == TEXT("GameplayTag") && vars::Has_NamedVariable<ck::FFragment_Variable_GameplayTag>(_Entity, Name))
    {
        const FString Trimmed = Value.TrimStartAndEnd();
        const FGameplayTag GameplayValue = Trimmed.IsEmpty()
            ? FGameplayTag{}
            : FGameplayTag::RequestGameplayTag(FName{Trimmed}, false);
        if (NOT Trimmed.IsEmpty() && NOT GameplayValue.IsValid()) { return; }
        UCk_Utils_Variables_GameplayTag_UE::Set_ByName(Mutable, Name, GameplayValue);
    }
}

auto SCkInspector_VariablesAuthored::Commit_Axis(
    const FString& InKey,
    const float InValue,
    const int32 InAxis) -> void
{
    namespace vars = ck_inspector_variables;
    auto Type = FString{}; auto Name = FName{};
    if (NOT FMath::IsFinite(InValue) || InAxis < 0 || InAxis > 2
        || NOT Resolve_Route(InKey, Type, Name) || NOT Get_CanEdit())
    { return; }
    auto Mutable = _Entity;
    if (Type == TEXT("Vector") && vars::Has_NamedVariable<ck::FFragment_Variable_Vector>(_Entity, Name))
    {
        auto Value = vars::Get_VariableValue<UCk_Utils_Variables_Vector_UE, FVector>(_Entity, Name);
        Value[InAxis] = InValue;
        UCk_Utils_Variables_Vector_UE::Set_ByName(Mutable, Name, Value);
    }
    else if (Type == TEXT("Rotator") && vars::Has_NamedVariable<ck::FFragment_Variable_Rotator>(_Entity, Name))
    {
        auto Value = vars::Get_VariableValue<UCk_Utils_Variables_Rotator_UE, FRotator>(_Entity, Name);
        if (InAxis == 0) { Value.Roll = InValue; }
        else if (InAxis == 1) { Value.Pitch = InValue; }
        else { Value.Yaw = InValue; }
        UCk_Utils_Variables_Rotator_UE::Set_ByName(Mutable, Name, Value);
    }
}

auto SCkInspector_VariablesAuthored::Navigate(const FString& InKey) -> void
{
    auto Type = FString{}; auto Name = FName{};
    if (NOT Resolve_Route(InKey, Type, Name) || Type != TEXT("Entity")
        || NOT ck_inspector_variables::Has_NamedVariable<ck::FFragment_Variable_Entity>(_Entity, Name))
    { return; }
    const FCk_Handle Target = _Entity.Get<ck::FFragment_Variable_Entity>().Get_Variables().FindRef(Name);
    if (ck::Is_NOT_Valid(Target)) { return; }
    if (const auto Selection = _SelectionModel.Pin(); Selection.IsValid())
    { Selection->Set_SelectedEntities({Target}); }
}

auto SCkInspector_VariablesAuthored::Build_AuthoredView() -> bool
{
    auto Registry = TSharedPtr<const FCkUiWidgetRegistrySnapshot>{};
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

    const TWeakPtr<SCkInspector_VariablesAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Visibility.Add(TEXT("variables-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("variables-unavailable"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("variables-can-edit"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanEdit(); }));
    Data.Text.Add(TEXT("variables-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return FText::FromString(Widget.IsValid() ? Widget->Get_EditDisabledReason() : FString{}); }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    Data.Collections.Add(TEXT("variables-records"), _Records);
    Data.ItemBoolChanged.Add(TEXT("variable-bool-changed"), FCkUiOnItemBoolChanged::CreateLambda(
        [WeakWidget](const FString& Key, const bool Value)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Change_Bool(Key, Value); } }));
    Data.ItemIntegerCommitted.Add(TEXT("variable-integer-committed"), FCkUiOnItemIntegerCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const int32 Value, ETextCommit::Type)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Integer(Key, Value); } }));
    Data.ItemNumberCommitted.Add(TEXT("variable-number-committed"), FCkUiOnItemNumberCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const float Value, ETextCommit::Type)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Number(Key, Value); } }));
    Data.ItemTextCommitted.Add(TEXT("variable-text-committed"), FCkUiOnItemTextCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const FText Value, ETextCommit::Type)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Text(Key, Value); } }));
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const FString Handler = FString::Printf(TEXT("variable-axis-%d-committed"), Axis);
        Data.ItemNumberCommitted.Add(Handler, FCkUiOnItemNumberCommitted::CreateLambda(
            [WeakWidget, Axis](const FString& Key, const float Value, ETextCommit::Type)
            { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Axis(Key, Value, Axis); } }));
    }
    Data.ItemActions.Add(TEXT("variable-entity-navigate"), FCkUiOnItemAction::CreateLambda(
        [WeakWidget](const FString& Key)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Navigate(Key); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(Root, TEXT("EcsInspectorVariables.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorVariables.ui.css")));
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

auto SCkInspector_VariablesAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    if (NOT Refresh_Records()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_VariablesAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _SelectionModel.Reset();
    _DiffLabels.Reset();
    _RouteTypes.Reset();
    _RouteNames.Reset();
    _Records.Reset();
    _View.Reset();
    _Mounted = false;
}

FCkInspector_Variables::~FCkInspector_Variables()
{
    OnDeactivated();
}

auto FCkInspector_Variables::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _Instances.RemoveAll([](const TWeakPtr<SCkInspector_VariablesAuthored>& InInstance)
    {
        const auto Instance = InInstance.Pin();
        return NOT Instance.IsValid() || Instance->Is_Inert();
    });
}

auto FCkInspector_Variables::OnDeactivated() -> void
{
    for (const auto& WeakInstance : _Instances)
    {
        if (const auto Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _Instances.Reset();
}
