#pragma once

#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkAttribute/CkAttribute_Fragment_Data.h"

#include "CkCore/Enums/CkEnums.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkLabel/CkLabel_Utils.h"

#include <type_traits>

// ====================================================================================================================
// ONE interactive row helper shared by the Float / Integer / Byte attribute inspectors.
//
// The three CkAttribute families are structurally identical — same Utils shape, same modifier record
// per Min/Max/Current component, same request names — differing only in the scalar they carry
// (float / int32 / uint8) and in whether a Refill feature exists (Byte has none). So the rows are
// written ONCE here, parameterized on the two Utils classes, and each inspector calls them from its
// existing per-attribute loop.
//
// Everything below goes out through public Utils Request_*/Set_* only. Attribute and modifier handles
// are captured BY VALUE and ck::IsValid-checked on every fire, per the Probes precedent.
//
// Gate: every attribute write is LocalOk. Attribute replication is symmetric but server-authored, so
// on a client world the gate's own advisory ("server projection will overwrite") rides along in the
// tooltip while the control stays live — a local experiment is legitimate, it just will not stick.
// ====================================================================================================================

namespace ck_inspector_attribute_rows
{
    // Row labels below open with "  ↳ " so the added editors read as children of the attribute row
    // above them instead of as siblings competing with it.

    inline auto Get_ComponentLabel(
        ECk_MinMaxCurrent InComponent) -> FString
    {
        return ck::Format_UE(TEXT("{}"), InComponent);
    }

    // Label is Has-gated: CkLabel's query API ensures on an unlabeled entity, and NotRevocable
    // modifiers are created with FGameplayTag::EmptyTag (they carry no label at all).
    inline auto Get_ModifierLabel(
        const FCk_Handle& InModifier) -> FGameplayTag
    {
        if (NOT UCk_Utils_GameplayLabel_UE::Has(InModifier))
        { return FGameplayTag{}; }

        return UCk_Utils_GameplayLabel_UE::Get_Label(InModifier);
    }

    /**
     * The Base / Final replication modifiers are CkAttribute's own bookkeeping — the projection of a
     * replicated attribute onto the client. Request_ClearAllModifiers skips them by label for exactly
     * that reason (CkAttribute_Utils.inl.h), so the debugger skips them too rather than offering an
     * Override/Remove that would corrupt the sync.
     *
     * Resolved BY NAME, not through ck::FAttributeModifier_ReplicationTags::Get_BaseTag(): that struct
     * is declared without CKATTRIBUTE_API, so its statics are unlinkable outside the CkAttribute module.
     * The two literals mirror CkAttribute_Fragment.cpp's AddNativeGameplayTag calls; if they are ever
     * renamed this filter degrades to showing two extra rows, never to a crash.
     */
    inline auto Get_IsInternalReplicationModifier(
        const FCk_Handle& InModifier) -> bool
    {
        const auto Label = Get_ModifierLabel(InModifier);

        if (NOT Label.IsValid())
        { return false; }

        constexpr auto ErrorIfNotFound = false;

        static const auto BaseTag  = FGameplayTag::RequestGameplayTag(TEXT("Ck.AttributeModifier.Replication.Base"), ErrorIfNotFound);
        static const auto FinalTag = FGameplayTag::RequestGameplayTag(TEXT("Ck.AttributeModifier.Replication.Final"), ErrorIfNotFound);

        return (BaseTag.IsValid() && Label == BaseTag) ||
               (FinalTag.IsValid() && Label == FinalTag);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /**
     * The one place the scalar family picks its editor: float takes the numeric row, int32 and uint8
     * take the integer row. A byte editor is bounded to 0..255 so the field cannot commit a value the
     * uint8 would silently wrap.
     */
    template <typename T_Value>
    auto Add_ValueRow(
        FCkInspectorWidgetBuilder&  InBuilder,
        const FText&                InLabel,
        TFunction<T_Value()>        InGet,
        TFunction<void(T_Value)>    InSet) -> void
    {
        constexpr auto Requirement = ECk_DebugRequest_Requirement::LocalOk;

        if constexpr (std::is_floating_point_v<T_Value>)
        {
            InBuilder.AddNumericRow(
                InLabel,
                TAttribute<float>::CreateLambda([InGet]() { return static_cast<float>(InGet()); }),
                [InSet](float InValue) { InSet(static_cast<T_Value>(InValue)); },
                TOptional<float>{},
                TOptional<float>{},
                Requirement);
        }
        else if constexpr (std::is_same_v<T_Value, uint8>)
        {
            InBuilder.AddIntegerRow(
                InLabel,
                TAttribute<int32>::CreateLambda([InGet]() { return static_cast<int32>(InGet()); }),
                [InSet](int32 InValue) { InSet(static_cast<uint8>(FMath::Clamp(InValue, 0, 255))); },
                TOptional<int32>{0},
                TOptional<int32>{255},
                Requirement);
        }
        else
        {
            InBuilder.AddIntegerRow(
                InLabel,
                TAttribute<int32>::CreateLambda([InGet]() { return static_cast<int32>(InGet()); }),
                [InSet](int32 InValue) { InSet(static_cast<T_Value>(InValue)); },
                TOptional<int32>{},
                TOptional<int32>{},
                Requirement);
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    /**
     * Value override for one attribute: a component dropdown (only when the attribute carries more
     * than one component) plus the editor that writes through Request_Override.
     *
     * The editor READS Get_FinalValue, not Get_BaseValue, and that is deliberate. Request_Override is
     * not a base-value write: it lands in Add_NotRevocable with Operation::Override, i.e. it creates
     * (or retargets) a permanent Override MODIFIER. The stored base never moves, so a Get_BaseValue
     * row would snap straight back to the old number the moment the request drained.
     *
     * Only components the attribute ACTUALLY has are offered — Add_NotRevocable CK_ENSUREs on a
     * component the attribute does not carry.
     */
    template <typename T_Utils, typename T_Attribute>
    auto Add_OverrideRows(
        FCkInspectorWidgetBuilder& InBuilder,
        const FString&             InAttributeName,
        T_Attribute                InAttribute) -> void
    {
        using ValueType = decltype(T_Utils::Get_FinalValue(std::declval<const T_Attribute&>(), ECk_MinMaxCurrent::Current));

        if (ck::Is_NOT_Valid(InAttribute))
        { return; }

        auto Components = TArray<ECk_MinMaxCurrent>{};

        for (const auto Component : {ECk_MinMaxCurrent::Current, ECk_MinMaxCurrent::Min, ECk_MinMaxCurrent::Max})
        {
            if (T_Utils::Has_Component(InAttribute, Component))
            { Components.Add(Component); }
        }

        if (Components.IsEmpty())
        { return; }

        const auto CapturedAttribute = InAttribute;

        // Shared between the dropdown and the editor below it, by value, so it lives exactly as long as
        // the two row widgets do and needs no inspector member.
        const auto SelectedIndex = MakeShared<int32>(0);

        if (Components.Num() > 1)
        {
            auto Options = TArray<FText>{};
            Options.Reserve(Components.Num());

            for (const auto Component : Components)
            { Options.Emplace(FText::FromString(Get_ComponentLabel(Component))); }

            InBuilder.AddEnumDropdownRow(
                FText::FromString(ck::Format_UE(TEXT("  ↳ {} component:"), InAttributeName)),
                Options,
                TAttribute<int32>::CreateLambda([SelectedIndex]() { return *SelectedIndex; }),
                [SelectedIndex, ComponentCount = Components.Num()](int32 InIndex)
                {
                    if (InIndex < 0 || InIndex >= ComponentCount)
                    { return; }

                    *SelectedIndex = InIndex;
                },
                ECk_DebugRequest_Requirement::LocalOk);
        }

        Add_ValueRow<ValueType>(
            InBuilder,
            FText::FromString(ck::Format_UE(TEXT("  ↳ {} override:"), InAttributeName)),
            [CapturedAttribute, SelectedIndex, Components]() -> ValueType
            {
                if (ck::Is_NOT_Valid(CapturedAttribute) || NOT Components.IsValidIndex(*SelectedIndex))
                { return {}; }

                return T_Utils::Get_FinalValue(CapturedAttribute, Components[*SelectedIndex]);
            },
            [CapturedAttribute, SelectedIndex, Components](ValueType InValue)
            {
                auto MutableAttribute = CapturedAttribute;

                if (ck::Is_NOT_Valid(MutableAttribute) || NOT Components.IsValidIndex(*SelectedIndex))
                { return; }

                T_Utils::Request_Override(MutableAttribute, InValue, Components[*SelectedIndex], {});
            });
    }

    // ----------------------------------------------------------------------------------------------------------------

    /**
     * Per-modifier delta editors and their Remove buttons, one block per component that actually has
     * modifiers. The modifier list is a STRUCTURAL snapshot taken here, exactly like AddChipsRow — a
     * modifier added or removed after this build shows up on the panel's next rebuild, never from Tick.
     *
     * Remove is offered only for LABELED modifiers. A modifier without a label came from
     * Add_NotRevocable (which passes EmptyTag), and NotRevocable contributions are permanent by design
     * — Request_ClearAllModifiers refuses them for the same reason.
     */
    template <typename T_Utils, typename T_ModifierUtils, typename T_Attribute>
    auto Add_ModifierRows(
        FCkInspectorWidgetBuilder& InBuilder,
        const FString&             InAttributeName,
        T_Attribute                InAttribute) -> void
    {
        using ValueType    = decltype(T_Utils::Get_FinalValue(std::declval<const T_Attribute&>(), ECk_MinMaxCurrent::Current));
        using ModifierType = decltype(T_ModifierUtils::TryGet(std::declval<const T_Attribute&>(), FGameplayTag{}, ECk_MinMaxCurrent::Current));

        if (ck::Is_NOT_Valid(InAttribute))
        { return; }

        const auto CapturedAttribute = InAttribute;

        for (const auto Component : {ECk_MinMaxCurrent::Current, ECk_MinMaxCurrent::Min, ECk_MinMaxCurrent::Max})
        {
            if (NOT T_Utils::Has_Component(InAttribute, Component))
            { continue; }

            auto MutableAttribute = InAttribute;
            auto Modifiers        = TArray<ModifierType>{};

            T_ModifierUtils::ForEach(MutableAttribute, [&Modifiers](ModifierType InModifier)
            {
                if (ck::Is_NOT_Valid(InModifier) || Get_IsInternalReplicationModifier(InModifier))
                { return; }

                Modifiers.Emplace(InModifier);
            }, Component);

            if (Modifiers.IsEmpty())
            { continue; }

            const auto ComponentLabel = Get_ComponentLabel(Component);

            auto Actions = TArray<FCkInspector_Action>{};

            for (const auto& Modifier : Modifiers)
            {
                const auto CapturedModifier = Modifier;
                const auto ModifierTag      = Get_ModifierLabel(Modifier);
                const auto ModifierName     = ModifierTag.IsValid()
                    ? ModifierTag.ToString()
                    : FString{TEXT("(not revocable)")};

                Add_ValueRow<ValueType>(
                    InBuilder,
                    FText::FromString(ck::Format_UE(TEXT("  ↳ {} [{}] {} delta:"), InAttributeName, ComponentLabel, ModifierName)),
                    [CapturedModifier, Component]() -> ValueType
                    {
                        if (ck::Is_NOT_Valid(CapturedModifier))
                        { return {}; }

                        return T_ModifierUtils::Get_Delta(CapturedModifier, Component);
                    },
                    [CapturedModifier](ValueType InValue)
                    {
                        auto MutableModifier = CapturedModifier;

                        if (ck::Is_NOT_Valid(MutableModifier))
                        { return; }

                        T_ModifierUtils::Override(MutableModifier, InValue);
                    });

                if (NOT ModifierTag.IsValid())
                { continue; }

                Actions.Emplace(FCkInspector_Action
                {
                    FText::FromString(ck::Format_UE(TEXT("Remove {}"), ModifierName)),
                    FText::FromString(ck::Format_UE(TEXT("Remove the [{}] modifier from [{}]'s [{}] component."),
                        ModifierName, InAttributeName, ComponentLabel)),
                    [CapturedModifier]()
                    {
                        auto MutableModifier = CapturedModifier;

                        if (ck::Is_NOT_Valid(MutableModifier))
                        { return; }

                        T_ModifierUtils::Remove(MutableModifier);
                    },
                    ECk_DebugRequest_Requirement::LocalOk
                });
            }

            Actions.Emplace(FCkInspector_Action
            {
                FText::FromString(TEXT("Clear All")),
                FText::FromString(ck::Format_UE(
                    TEXT("Request_ClearAllModifiers on [{}]'s [{}] component.\n"
                         "Clears REVOCABLE modifiers ONLY — NotRevocable Add/Multiply contributions are permanent and survive this."),
                    InAttributeName, ComponentLabel)),
                [CapturedAttribute, Component]()
                {
                    auto MutableAttributeToClear = CapturedAttribute;

                    if (ck::Is_NOT_Valid(MutableAttributeToClear))
                    { return; }

                    T_ModifierUtils::Request_ClearAllModifiers(MutableAttributeToClear, Component, {});
                },
                ECk_DebugRequest_Requirement::LocalOk
            });

            InBuilder.AddActionRow(
                FText::FromString(ck::Format_UE(TEXT("  ↳ {} [{}] modifiers:"), InAttributeName, ComponentLabel)),
                Actions);
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    /**
     * Refill Pause/Resume, plus the live state the two buttons act on. Float and Integer attributes
     * only — the Byte family ships no refill Utils at all, which is why this is a separate call
     * instead of a branch inside the shared entry point.
     */
    template <typename T_Utils, typename T_RefillUtils, typename T_Attribute>
    auto Add_RefillRows(
        FCkInspectorWidgetBuilder& InBuilder,
        const FString&             InAttributeName,
        T_Attribute                InAttribute) -> void
    {
        if (ck::Is_NOT_Valid(InAttribute) || NOT T_Utils::Has_RefillAttribute(InAttribute))
        { return; }

        const auto CapturedRefill = T_Utils::TryGet_RefillAttribute(InAttribute);

        if (ck::Is_NOT_Valid(CapturedRefill))
        { return; }

        InBuilder.AddStatusPillRow(
            FText::FromString(ck::Format_UE(TEXT("  ↳ {} refill:"), InAttributeName)),
            TAttribute<FText>::CreateLambda([CapturedRefill]()
            {
                if (ck::Is_NOT_Valid(CapturedRefill))
                { return FText::FromString(TEXT("--")); }

                return FText::FromString(ck::Format_UE(TEXT("{} @ {:.2f}/s"),
                    T_RefillUtils::Get_RefillState(CapturedRefill),
                    T_RefillUtils::Get_FillRate(CapturedRefill)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedRefill]()
            {
                if (ck::Is_NOT_Valid(CapturedRefill))
                { return ECk_Tone::Neutral; }

                return T_RefillUtils::Get_RefillState(CapturedRefill) == ECk_Attribute_RefillState::Running
                    ? ECk_Tone::Ok
                    : ECk_Tone::Warn;
            }));

        InBuilder.AddActionRow(
            FText::FromString(ck::Format_UE(TEXT("  ↳ {} refill control:"), InAttributeName)),
            TArray<FCkInspector_Action>
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Pause")),
                    FText::FromString(ck::Format_UE(TEXT("Request_Pause on [{}]'s refill."), InAttributeName)),
                    [CapturedRefill]()
                    {
                        auto MutableRefill = CapturedRefill;

                        if (ck::Is_NOT_Valid(MutableRefill))
                        { return; }

                        T_RefillUtils::Request_Pause(MutableRefill, {});
                    },
                    ECk_DebugRequest_Requirement::LocalOk
                },
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Resume")),
                    FText::FromString(ck::Format_UE(TEXT("Request_Resume on [{}]'s refill."), InAttributeName)),
                    [CapturedRefill]()
                    {
                        auto MutableRefill = CapturedRefill;

                        if (ck::Is_NOT_Valid(MutableRefill))
                        { return; }

                        T_RefillUtils::Request_Resume(MutableRefill, {});
                    },
                    ECk_DebugRequest_Requirement::LocalOk
                }
            });
    }
}

// ====================================================================================================================
