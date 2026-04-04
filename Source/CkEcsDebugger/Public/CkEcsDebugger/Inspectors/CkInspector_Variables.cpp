#include "CkInspector_Variables.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkVariables/CkUnrealVariables_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Variables)

static const FLinearColor Color_Variable_Type = FLinearColor(0.4f, 0.7f, 1.0f);
static const FLinearColor Color_Variable_Value = FLinearColor(0.9f, 0.9f, 0.9f);

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
                Color_Variable_Value);
        }
    }
}

// =====================================================================================================================

auto FCkInspector_Variables::BuildVariablesGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Bool
    AddVariableRows<ck::FFragment_Variable_Bool>(Builder, Entity, FText::FromString(TEXT("Bool")),
        [](bool InValue) -> FString { return InValue ? TEXT("true") : TEXT("false"); });

    // ---- Byte
    AddVariableRows<ck::FFragment_Variable_Byte>(Builder, Entity, FText::FromString(TEXT("Byte")),
        [](uint8 InValue) -> FString { return FString::Printf(TEXT("%d"), InValue); });

    // ---- Int32
    AddVariableRows<ck::FFragment_Variable_Int32>(Builder, Entity, FText::FromString(TEXT("Int32")),
        [](int32 InValue) -> FString { return FString::Printf(TEXT("%d"), InValue); });

    // ---- Int64
    AddVariableRows<ck::FFragment_Variable_Int64>(Builder, Entity, FText::FromString(TEXT("Int64")),
        [](int64 InValue) -> FString { return FString::Printf(TEXT("%lld"), InValue); });

    // ---- Float
    AddVariableRows<ck::FFragment_Variable_Float>(Builder, Entity, FText::FromString(TEXT("Float")),
        [](float InValue) -> FString { return FString::Printf(TEXT("%.3f"), InValue); });

    // ---- Name
    AddVariableRows<ck::FFragment_Variable_Name>(Builder, Entity, FText::FromString(TEXT("Name")),
        [](const FName& InValue) -> FString { return InValue.ToString(); });

    // ---- String
    AddVariableRows<ck::FFragment_Variable_String>(Builder, Entity, FText::FromString(TEXT("String")),
        [](const FString& InValue) -> FString { return InValue; });

    // ---- Text
    AddVariableRows<ck::FFragment_Variable_Text>(Builder, Entity, FText::FromString(TEXT("Text")),
        [](const FText& InValue) -> FString { return InValue.ToString(); });

    // ---- Vector
    AddVariableRows<ck::FFragment_Variable_Vector>(Builder, Entity, FText::FromString(TEXT("Vector")),
        [](const FVector& InValue) -> FString { return InValue.ToString(); });

    // ---- Vector2D
    AddVariableRows<ck::FFragment_Variable_Vector2D>(Builder, Entity, FText::FromString(TEXT("Vector2D")),
        [](const FVector2D& InValue) -> FString { return InValue.ToString(); });

    // ---- Rotator
    AddVariableRows<ck::FFragment_Variable_Rotator>(Builder, Entity, FText::FromString(TEXT("Rotator")),
        [](const FRotator& InValue) -> FString { return InValue.ToString(); });

    // ---- Transform
    AddVariableRows<ck::FFragment_Variable_Transform>(Builder, Entity, FText::FromString(TEXT("Transform")),
        [](const FTransform& InValue) -> FString { return InValue.ToString(); });

    // ---- GameplayTag
    AddVariableRows<ck::FFragment_Variable_GameplayTag>(Builder, Entity, FText::FromString(TEXT("GameplayTag")),
        [](const FGameplayTag& InValue) -> FString { return InValue.IsValid() ? InValue.GetTagName().ToString() : TEXT("(None)"); });

    // ---- GameplayTagContainer
    AddVariableRows<ck::FFragment_Variable_GameplayTagContainer>(Builder, Entity, FText::FromString(TEXT("TagContainer")),
        [](const FGameplayTagContainer& InValue) -> FString { return InValue.IsEmpty() ? TEXT("(Empty)") : InValue.ToString(); });

    // ---- LinearColor
    AddVariableRows<ck::FFragment_Variable_LinearColor>(Builder, Entity, FText::FromString(TEXT("LinearColor")),
        [](const FLinearColor& InValue) -> FString { return InValue.ToString(); });

    // ---- Entity (clickable to navigate)
    if (Entity.Has<ck::FFragment_Variable_Entity>())
    {
        const auto& EntityVariables = Entity.Get<ck::FFragment_Variable_Entity>().Get_Variables();
        if (NOT EntityVariables.IsEmpty())
        {
            Builder.AddHeader(FText::FromString(TEXT("Entity")));
            auto WeakSelectionModel = SelectionModel;

            for (const auto& [Name, Value] : EntityVariables)
            {
                const auto NameStr = Name.ToString();
                const auto EntityHandle = Value;

                Builder.AddClickableRow(
                    FText::FromString(NameStr),
                    [EntityHandle](const FCk_Handle& E) -> FText
                    {
                        return FText::FromString(ck::IsValid(EntityHandle)
                            ? *ck::Format_UE(TEXT("[{}]"), EntityHandle)
                            : TEXT("(Invalid)"));
                    },
                    Color_Variable_Value,
                    [WeakSelectionModel, EntityHandle]()
                    {
                        if (WeakSelectionModel.IsValid() && ck::IsValid(EntityHandle))
                        {
                            WeakSelectionModel->Set_SelectedEntities({ EntityHandle });
                        }
                    });
            }
        }
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Variables::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
