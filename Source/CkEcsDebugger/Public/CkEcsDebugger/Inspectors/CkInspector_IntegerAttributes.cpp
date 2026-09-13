#include "CkInspector_IntegerAttributes.h"

#include "CkAttribute/FloatAttribute/CkFloatAttribute_Fragment.h"
#include "CkAttribute/IntegerAttribute/CkIntegerAttribute_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorAttributeRows.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IntegerAttributes)

namespace ck_inspector_integer_attributes
{
    struct FRecordProjection
    {
        FString Key;
        FString Name;
        FString Summary;
        FString RowLabel;
        FString RowValue;
        FString CurrentText;
        FString MinText;
        FString MaxText;
        FString RefillLabel;
        FString RefillControlLabel;
        FString RefillState;
        FString PauseTooltip;
        FString ResumeTooltip;
        FString ActionTooltip;
        int32 Current = 0;
        int32 Min = 0;
        int32 Max = 0;
        int32 ModifierDelta = 0;
        float Fraction = 0.0f;
        bool DiffMarked = false;
        bool AttributeDiffMarked = false;
        bool OverrideDiffMarked = false;
        bool RefillStatusDiffMarked = false;
        bool RefillControlDiffMarked = false;
        bool RecordVisible = false;
        bool AttributeVisible = false;
        bool CurrentVisible = false;
        bool MinVisible = false;
        bool MaxVisible = false;
        bool MeterVisible = false;
        bool PlainVisible = false;
        bool RefillStatusVisible = false;
        bool RefillControlVisible = false;
        bool RefillActionsVisible = false;
        bool RefillRunning = false;
        bool ModifierVisible = false;
        bool ClearVisible = false;
        bool EditVisible = false;
        bool ReadVisible = false;
        bool RemoveVisible = false;
    };

    auto Make_TextField(const FString& InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)};
    }

    auto Make_BoolField(const bool InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = InValue};
    }

    auto Make_NumberField(const float InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Number, .Number = InValue};
    }

    auto Make_IntegerField(const int32 InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Integer, .Integer = InValue};
    }

    auto Make_ColorField(const bool bInDiffMarked) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{
            .Kind = ECkUiFieldKind::Color,
            .Color = bInDiffMarked ? CkStyle::Accent() : CkStyle::Text()};
    }

    auto Make_Record(const FRecordProjection& InProjection) -> FCkUiRecordData
    {
        auto Record = FCkUiRecordData{};
        Record.Key = InProjection.Key;
        Record.Fields.Add(TEXT("name"), Make_TextField(InProjection.Name));
        Record.Fields.Add(TEXT("summary"), Make_TextField(InProjection.Summary));
        Record.Fields.Add(TEXT("row-label"), Make_TextField(InProjection.RowLabel));
        Record.Fields.Add(TEXT("row-value"), Make_TextField(InProjection.RowValue));
        Record.Fields.Add(TEXT("current-text"), Make_TextField(InProjection.CurrentText));
        Record.Fields.Add(TEXT("min-text"), Make_TextField(InProjection.MinText));
        Record.Fields.Add(TEXT("max-text"), Make_TextField(InProjection.MaxText));
        Record.Fields.Add(TEXT("refill-label"), Make_TextField(InProjection.RefillLabel));
        Record.Fields.Add(TEXT("refill-control-label"), Make_TextField(InProjection.RefillControlLabel));
        Record.Fields.Add(TEXT("refill-state"), Make_TextField(InProjection.RefillState));
        Record.Fields.Add(TEXT("pause-label"), Make_TextField(TEXT("Pause")));
        Record.Fields.Add(TEXT("resume-label"), Make_TextField(TEXT("Resume")));
        Record.Fields.Add(TEXT("pause-tooltip"), Make_TextField(InProjection.PauseTooltip));
        Record.Fields.Add(TEXT("resume-tooltip"), Make_TextField(InProjection.ResumeTooltip));
        Record.Fields.Add(TEXT("action-tooltip"), Make_TextField(InProjection.ActionTooltip));
        Record.Fields.Add(TEXT("remove-label"), Make_TextField(TEXT("Remove")));
        Record.Fields.Add(TEXT("clear-label"), Make_TextField(TEXT("Clear All")));
        Record.Fields.Add(TEXT("current"), Make_IntegerField(InProjection.Current));
        Record.Fields.Add(TEXT("min"), Make_IntegerField(InProjection.Min));
        Record.Fields.Add(TEXT("max"), Make_IntegerField(InProjection.Max));
        Record.Fields.Add(TEXT("modifier-delta"), Make_IntegerField(InProjection.ModifierDelta));
        Record.Fields.Add(TEXT("fraction"), Make_NumberField(InProjection.Fraction));
        Record.Fields.Add(TEXT("diff-color"), Make_ColorField(InProjection.DiffMarked));
        Record.Fields.Add(TEXT("attribute-diff-color"), Make_ColorField(InProjection.AttributeDiffMarked));
        Record.Fields.Add(TEXT("override-diff-color"), Make_ColorField(InProjection.OverrideDiffMarked));
        Record.Fields.Add(TEXT("refill-status-diff-color"), Make_ColorField(InProjection.RefillStatusDiffMarked));
        Record.Fields.Add(TEXT("refill-control-diff-color"), Make_ColorField(InProjection.RefillControlDiffMarked));
        const auto RefillTone = InProjection.RefillRunning ? ECk_Tone::Ok : ECk_Tone::Warn;
        Record.Fields.Add(TEXT("refill-foreground"), FCkUiFieldValue{
            .Kind = ECkUiFieldKind::Color,
            .Color = CkStyle::GetToneColor(RefillTone)});
        Record.Fields.Add(TEXT("refill-background"), FCkUiFieldValue{
            .Kind = ECkUiFieldKind::Color,
            .Color = CkStyle::GetToneDimColor(RefillTone)});
        Record.Fields.Add(TEXT("meter-color"), FCkUiFieldValue{
            .Kind = ECkUiFieldKind::Color,
            .Color = CkStyle::Accent()});
        Record.Fields.Add(TEXT("record-visible"), Make_BoolField(InProjection.RecordVisible));
        Record.Fields.Add(TEXT("attribute-visible"), Make_BoolField(InProjection.AttributeVisible));
        Record.Fields.Add(TEXT("current-visible"), Make_BoolField(InProjection.CurrentVisible));
        Record.Fields.Add(TEXT("min-visible"), Make_BoolField(InProjection.MinVisible));
        Record.Fields.Add(TEXT("max-visible"), Make_BoolField(InProjection.MaxVisible));
        Record.Fields.Add(TEXT("meter-visible"), Make_BoolField(InProjection.MeterVisible));
        Record.Fields.Add(TEXT("plain-visible"), Make_BoolField(InProjection.PlainVisible));
        Record.Fields.Add(TEXT("refill-status-visible"), Make_BoolField(InProjection.RefillStatusVisible));
        Record.Fields.Add(TEXT("refill-control-visible"), Make_BoolField(InProjection.RefillControlVisible));
        Record.Fields.Add(TEXT("refill-actions-visible"), Make_BoolField(InProjection.RefillActionsVisible));
        Record.Fields.Add(TEXT("modifier-visible"), Make_BoolField(InProjection.ModifierVisible));
        Record.Fields.Add(TEXT("clear-visible"), Make_BoolField(InProjection.ClearVisible));
        Record.Fields.Add(TEXT("edit-visible"), Make_BoolField(InProjection.EditVisible));
        Record.Fields.Add(TEXT("read-visible"), Make_BoolField(InProjection.ReadVisible));
        Record.Fields.Add(TEXT("remove-visible"), Make_BoolField(InProjection.RemoveVisible));
        return Record;
    }

    auto Get_StableKey(const FCk_Handle& InHandle) -> FString
    {
        return ck::IsValid(InHandle) ? ck::Format_UE(TEXT("{}"), InHandle.Get_Entity()) : FString{};
    }

    auto Get_ComponentName(const ECk_MinMaxCurrent InComponent) -> FString
    {
        return ck::Format_UE(TEXT("{}"), InComponent);
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

    auto Is_AttributeLive(
        const FCk_Handle& InOwner,
        const FCk_Handle_IntegerAttribute& InAttribute) -> bool
    {
        return NOT Is_Destroying(InOwner)
            && UCk_Utils_IntegerAttribute_UE::Has_Any(InOwner)
            && NOT Is_Destroying(FCk_Handle{InAttribute})
            && UCk_Utils_IntegerAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Current);
    }

    auto Get_Fraction(const FCk_Handle_IntegerAttribute& InAttribute) -> float
    {
        if (ck::Is_NOT_Valid(InAttribute)
            || NOT UCk_Utils_IntegerAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Min)
            || NOT UCk_Utils_IntegerAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Max))
        { return 0.0f; }

        const auto MinValue = UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
            InAttribute, ECk_MinMaxCurrent::Min);
        const auto MaxValue = UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
            InAttribute, ECk_MinMaxCurrent::Max);
        const auto Current = UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
            InAttribute, ECk_MinMaxCurrent::Current);
        const double Range = static_cast<double>(MaxValue) - static_cast<double>(MinValue);
        if (Range == 0.0) { return 0.0f; }

        return static_cast<float>((static_cast<double>(Current) - static_cast<double>(MinValue)) / Range);
    }

    auto Get_Annotations(const FCk_Handle_IntegerAttribute& InAttribute) -> FString
    {
        if (ck::Is_NOT_Valid(InAttribute)) { return FString{}; }

        const auto Final = UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
            InAttribute, ECk_MinMaxCurrent::Current);
        const auto Base = UCk_Utils_IntegerAttribute_UE::Get_BaseValue(
            InAttribute, ECk_MinMaxCurrent::Current);
        auto Result = FString{};
        if (Base != Final)
        { Result += ck::Format_UE(TEXT(" (Base: {})"), Base); }

        const auto PreClamp = UCk_Utils_IntegerAttribute_UE::Get_PreClampFinalValue(InAttribute);
        if (PreClamp > Final)
        { Result += ck::Format_UE(TEXT(" (preclamp max: {})"), PreClamp); }
        else if (PreClamp < Final)
        { Result += ck::Format_UE(TEXT(" (preclamp min: {})"), PreClamp); }
        return Result;
    }

    auto Get_Summary(
        const FCk_Handle_IntegerAttribute& InAttribute,
        const bool bInHasMax,
        const bool bInMeter) -> FString
    {
        if (ck::Is_NOT_Valid(InAttribute)) { return TEXT("--"); }

        const auto Current = UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
            InAttribute, ECk_MinMaxCurrent::Current);
        auto Result = ck::Format_UE(TEXT("{}"), Current);
        if (bInHasMax)
        {
            const auto MaxValue = UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
                InAttribute, ECk_MinMaxCurrent::Max);
            Result += bInMeter
                ? ck::Format_UE(TEXT(" / {}"), MaxValue)
                : ck::Format_UE(TEXT("  / {}"), MaxValue);
        }
        return Result + Get_Annotations(InAttribute);
    }

    auto Matches_Row(
        const FString& InFilter,
        const FString& InAttributeName,
        const FString& InRowLabel,
        const FString& InValue = FString{}) -> bool
    {
        return FCkInspectorWidgetBuilder::Matches_Filter(
            InFilter,
            ck::Format_UE(TEXT("{} {}"), InAttributeName, InRowLabel),
            InValue);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkInspector_IntegerAttributesAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _Filter = InArgs._Filter;
    _DiffLabels = InArgs._DiffLabels;
    _SelectionModel = InArgs._SelectionModel;

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

SCkInspector_IntegerAttributesAuthored::~SCkInspector_IntegerAttributesAuthored()
{
    Release();
}

auto SCkInspector_IntegerAttributesAuthored::Get_IsAvailable() const -> bool
{
    return _Active
        && NOT ck_inspector_integer_attributes::Is_Destroying(_Entity)
        && UCk_Utils_IntegerAttribute_UE::Has_Any(_Entity);
}

auto SCkInspector_IntegerAttributesAuthored::Get_CanRequest() const -> bool
{
    return Get_IsAvailable()
        && ck::DebugRequestGate::Evaluate(
            _Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_IntegerAttributesAuthored::Get_RequestDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("Integer attributes are unavailable."); }
    return ck::DebugRequestGate::Evaluate(
        _Entity, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString();
}

auto SCkInspector_IntegerAttributesAuthored::Refresh_Records() -> bool
{
    if (NOT _Records.IsValid())
    {
        const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreate({
            {TEXT("name"), ECkUiFieldKind::Text},
            {TEXT("summary"), ECkUiFieldKind::Text},
            {TEXT("row-label"), ECkUiFieldKind::Text},
            {TEXT("row-value"), ECkUiFieldKind::Text},
            {TEXT("current-text"), ECkUiFieldKind::Text},
            {TEXT("min-text"), ECkUiFieldKind::Text},
            {TEXT("max-text"), ECkUiFieldKind::Text},
            {TEXT("refill-label"), ECkUiFieldKind::Text},
            {TEXT("refill-control-label"), ECkUiFieldKind::Text},
            {TEXT("refill-state"), ECkUiFieldKind::Text},
            {TEXT("pause-label"), ECkUiFieldKind::Text},
            {TEXT("resume-label"), ECkUiFieldKind::Text},
            {TEXT("pause-tooltip"), ECkUiFieldKind::Text},
            {TEXT("resume-tooltip"), ECkUiFieldKind::Text},
            {TEXT("action-tooltip"), ECkUiFieldKind::Text},
            {TEXT("remove-label"), ECkUiFieldKind::Text},
            {TEXT("clear-label"), ECkUiFieldKind::Text},
            {TEXT("current"), ECkUiFieldKind::Integer},
            {TEXT("min"), ECkUiFieldKind::Integer},
            {TEXT("max"), ECkUiFieldKind::Integer},
            {TEXT("modifier-delta"), ECkUiFieldKind::Integer},
            {TEXT("fraction"), ECkUiFieldKind::Number},
            {TEXT("diff-color"), ECkUiFieldKind::Color},
            {TEXT("attribute-diff-color"), ECkUiFieldKind::Color},
            {TEXT("override-diff-color"), ECkUiFieldKind::Color},
            {TEXT("refill-status-diff-color"), ECkUiFieldKind::Color},
            {TEXT("refill-control-diff-color"), ECkUiFieldKind::Color},
            {TEXT("refill-foreground"), ECkUiFieldKind::Color},
            {TEXT("refill-background"), ECkUiFieldKind::Color},
            {TEXT("meter-color"), ECkUiFieldKind::Color},
            {TEXT("record-visible"), ECkUiFieldKind::Bool},
            {TEXT("attribute-visible"), ECkUiFieldKind::Bool},
            {TEXT("current-visible"), ECkUiFieldKind::Bool},
            {TEXT("min-visible"), ECkUiFieldKind::Bool},
            {TEXT("max-visible"), ECkUiFieldKind::Bool},
            {TEXT("meter-visible"), ECkUiFieldKind::Bool},
            {TEXT("plain-visible"), ECkUiFieldKind::Bool},
            {TEXT("refill-status-visible"), ECkUiFieldKind::Bool},
            {TEXT("refill-control-visible"), ECkUiFieldKind::Bool},
            {TEXT("refill-actions-visible"), ECkUiFieldKind::Bool},
            {TEXT("modifier-visible"), ECkUiFieldKind::Bool},
            {TEXT("clear-visible"), ECkUiFieldKind::Bool},
            {TEXT("edit-visible"), ECkUiFieldKind::Bool},
            {TEXT("read-visible"), ECkUiFieldKind::Bool},
            {TEXT("remove-visible"), ECkUiFieldKind::Bool},
        }, _Records);
        if (NOT CreateResult.Succeeded || NOT _Records.IsValid())
        {
            _LoadError = FString::Join(CreateResult.Errors, TEXT("\n"));
            return false;
        }
    }

    auto Records = TArray<FCkUiRecordData>{};
    auto Attributes = TMap<FString, FCk_Handle_IntegerAttribute>{};
    auto Modifiers = TMap<FString, FCk_Handle_IntegerAttributeModifier>{};
    auto Refills = TMap<FString, FCk_Handle_IntegerAttributeRefill>{};
    auto RouteAttributeKeys = TMap<FString, FString>{};
    auto RouteComponents = TMap<FString, ECk_MinMaxCurrent>{};
    const auto bShowEditControls = ck::debug_axes::EditControls_AreVisible(
        UCkDebuggerStyleSettings::Get_Selection());

    if (Get_IsAvailable())
    {
        auto MutableOwner = _Entity;
        UCk_Utils_IntegerAttribute_UE::ForEach(
            MutableOwner,
            [this, bShowEditControls, &Records, &Attributes, &Modifiers, &Refills,
             &RouteAttributeKeys, &RouteComponents](FCk_Handle_IntegerAttribute InAttribute)
            {
                if (NOT ck_inspector_integer_attributes::Is_AttributeLive(_Entity, InAttribute))
                { return; }

                const auto AttributeTag = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
                const auto AttributeName = AttributeTag.IsValid()
                    ? AttributeTag.ToString()
                    : FString{TEXT("Unnamed")};
                const auto AttributeKey = ck_inspector_integer_attributes::Get_StableKey(InAttribute);
                if (AttributeKey.IsEmpty() || Attributes.Contains(AttributeKey)) { return; }

                const auto bHasMin = UCk_Utils_IntegerAttribute_UE::Has_Component(
                    InAttribute, ECk_MinMaxCurrent::Min);
                const auto bHasMax = UCk_Utils_IntegerAttribute_UE::Has_Component(
                    InAttribute, ECk_MinMaxCurrent::Max);
                const auto bMeter = bHasMin && bHasMax;
                const auto Current = UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
                    InAttribute, ECk_MinMaxCurrent::Current);
                const auto Summary = ck_inspector_integer_attributes::Get_Summary(
                    InAttribute, bHasMax, bMeter);
                const auto ComponentLabel = ck::Format_UE(
                    TEXT("  ↳ {} component:"), AttributeName);
                const auto OverrideLabel = ck::Format_UE(
                    TEXT("  ↳ {} override:"), AttributeName);
                const auto RefillLabel = ck::Format_UE(
                    TEXT("  ↳ {} refill:"), AttributeName);
                const auto RefillControlLabel = ck::Format_UE(
                    TEXT("  ↳ {} refill control:"), AttributeName);
                auto Refill = FCk_Handle_IntegerAttributeRefill{};
                if (UCk_Utils_IntegerAttribute_UE::Has_RefillAttribute(InAttribute))
                { Refill = UCk_Utils_IntegerAttribute_UE::TryGet_RefillAttribute(InAttribute); }
                const auto bHasRefill = ck::IsValid(Refill)
                    && NOT ck_inspector_integer_attributes::Is_Destroying(FCk_Handle{Refill})
                    && FCk_Handle{Refill}.Has<ck::FFragment_FloatAttribute_Current>();
                const auto RefillState = bHasRefill
                    ? ck::Format_UE(TEXT("{} @ {:.2f}/s"),
                        UCk_Utils_IntegerAttributeRefill_UE::Get_RefillState(Refill),
                        UCk_Utils_IntegerAttributeRefill_UE::Get_FillRate(Refill))
                    : FString{};
                const auto bHasMultipleComponents = bHasMin || bHasMax;
                const auto bComponentFilterVisible = bHasMultipleComponents
                    && FCkInspectorWidgetBuilder::Matches_Filter(
                        _Filter, ComponentLabel, FString{});
                const auto bOverrideFilterVisible = FCkInspectorWidgetBuilder::Matches_Filter(
                    _Filter, OverrideLabel, FString{});
                const auto bRefillStatusVisible = bHasRefill
                    && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, RefillLabel, RefillState);
                const auto bRefillControlVisible = bHasRefill && bShowEditControls
                    && FCkInspectorWidgetBuilder::Matches_Filter(
                        _Filter, RefillControlLabel, TEXT("Pause Resume"));

                const auto bAttributeVisible = ck_inspector_integer_attributes::Matches_Row(
                    _Filter, AttributeName, AttributeName, Summary);
                const auto bCurrentVisible = bComponentFilterVisible || bOverrideFilterVisible
                    || ck_inspector_integer_attributes::Matches_Row(
                         _Filter, AttributeName, TEXT("Current override"), ck::Format_UE(TEXT("{}"), Current));
                const auto bMinVisible = bHasMin && (bComponentFilterVisible || bOverrideFilterVisible
                    || ck_inspector_integer_attributes::Matches_Row(
                         _Filter, AttributeName, TEXT("Min override"), ck::Format_UE(TEXT("{}"),
                            UCk_Utils_IntegerAttribute_UE::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Min))));
                const auto bMaxVisible = bHasMax && (bComponentFilterVisible || bOverrideFilterVisible
                    || ck_inspector_integer_attributes::Matches_Row(
                         _Filter, AttributeName, TEXT("Max override"), ck::Format_UE(TEXT("{}"),
                            UCk_Utils_IntegerAttribute_UE::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Max))));

                Records.Add(ck_inspector_integer_attributes::Make_Record({
                    .Key = AttributeKey,
                    .Name = AttributeName,
                    .Summary = Summary,
                    .CurrentText = ck::Format_UE(TEXT("{}"), Current),
                    .MinText = bHasMin ? ck::Format_UE(TEXT("{}"),
                        UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
                            InAttribute, ECk_MinMaxCurrent::Min)) : FString{},
                    .MaxText = bHasMax ? ck::Format_UE(TEXT("{}"),
                        UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
                        InAttribute, ECk_MinMaxCurrent::Max)) : FString{},
                    .RefillLabel = RefillLabel,
                    .RefillControlLabel = RefillControlLabel,
                    .RefillState = RefillState,
                    .PauseTooltip = ck::Format_UE(TEXT("Request_Pause on [{}]'s refill."), AttributeName),
                    .ResumeTooltip = ck::Format_UE(TEXT("Request_Resume on [{}]'s refill."), AttributeName),
                    .Current = Current,
                    .Min = bHasMin ? UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
                        InAttribute, ECk_MinMaxCurrent::Min) : 0,
                    .Max = bHasMax ? UCk_Utils_IntegerAttribute_UE::Get_FinalValue(
                        InAttribute, ECk_MinMaxCurrent::Max) : 0,
                    .Fraction = ck_inspector_integer_attributes::Get_Fraction(InAttribute),
                    .AttributeDiffMarked = _DiffLabels.Contains(AttributeName),
                    .OverrideDiffMarked = _DiffLabels.Contains(ComponentLabel)
                        || _DiffLabels.Contains(OverrideLabel),
                    .RefillStatusDiffMarked = _DiffLabels.Contains(RefillLabel),
                    .RefillControlDiffMarked = _DiffLabels.Contains(RefillControlLabel),
                    .RecordVisible = bAttributeVisible || bCurrentVisible || bMinVisible || bMaxVisible
                        || bRefillStatusVisible || bRefillControlVisible,
                    .AttributeVisible = bAttributeVisible,
                    .CurrentVisible = bCurrentVisible,
                    .MinVisible = bMinVisible,
                    .MaxVisible = bMaxVisible,
                    .MeterVisible = bMeter,
                    .PlainVisible = NOT bMeter,
                    .RefillStatusVisible = bRefillStatusVisible,
                    .RefillControlVisible = bRefillControlVisible,
                    .RefillActionsVisible = bRefillControlVisible,
                    .RefillRunning = bHasRefill
                        && UCk_Utils_IntegerAttributeRefill_UE::Get_RefillState(Refill)
                            == ECk_Attribute_RefillState::Running,
                    .EditVisible = bShowEditControls,
                    .ReadVisible = NOT bShowEditControls,
                }));
                Attributes.Add(AttributeKey, InAttribute);
                if (bHasRefill) { Refills.Add(AttributeKey, Refill); }
                RouteAttributeKeys.Add(AttributeKey, AttributeKey);
                RouteComponents.Add(AttributeKey, ECk_MinMaxCurrent::Current);

                for (const auto Component : {
                    ECk_MinMaxCurrent::Current,
                    ECk_MinMaxCurrent::Min,
                    ECk_MinMaxCurrent::Max})
                {
                    if (NOT UCk_Utils_IntegerAttribute_UE::Has_Component(InAttribute, Component))
                    { continue; }

                    auto ComponentModifiers = TArray<FCk_Handle_IntegerAttributeModifier>{};
                    auto MutableAttribute = InAttribute;
                    UCk_Utils_IntegerAttributeModifier_UE::ForEach(
                        MutableAttribute,
                        [&ComponentModifiers](FCk_Handle_IntegerAttributeModifier InModifier)
                        {
                            if (ck::IsValid(InModifier)
                                && NOT ck_inspector_attribute_rows::Get_IsInternalReplicationModifier(InModifier))
                            { ComponentModifiers.Add(InModifier); }
                        },
                        Component);

                    const auto ComponentName = ck_inspector_integer_attributes::Get_ComponentName(Component);
                    for (const auto& Modifier : ComponentModifiers)
                    {
                        const auto ModifierTag = ck_inspector_attribute_rows::Get_ModifierLabel(Modifier);
                        const auto ModifierName = ModifierTag.IsValid()
                            ? ModifierTag.ToString()
                            : FString{TEXT("(not revocable)")};
                        const auto RowLabel = ck::Format_UE(
                            TEXT("  ↳ {} [{}] {} delta:"),
                            AttributeName, ComponentName, ModifierName);
                        const auto Delta = UCk_Utils_IntegerAttributeModifier_UE::Get_Delta(
                            Modifier, Component);
                        const auto bModifierVisible = ck_inspector_integer_attributes::Matches_Row(
                            _Filter, AttributeName, RowLabel, ck::Format_UE(TEXT("{} Remove"), Delta));
                        const auto ModifierKey = ck::Format_UE(
                            TEXT("modifier/{}/{}/{}"),
                            AttributeKey,
                            ComponentName,
                            ck_inspector_integer_attributes::Get_StableKey(Modifier));

                        Records.Add(ck_inspector_integer_attributes::Make_Record({
                            .Key = ModifierKey,
                            .Name = AttributeName,
                            .RowLabel = RowLabel,
                            .RowValue = ck::Format_UE(TEXT("{}"), Delta),
                            .ActionTooltip = ck::Format_UE(
                                TEXT("Remove the [{}] modifier from [{}]'s [{}] component."),
                                ModifierName, AttributeName, ComponentName),
                            .ModifierDelta = Delta,
                            .DiffMarked = _DiffLabels.Contains(RowLabel),
                            .RecordVisible = bModifierVisible,
                            .ModifierVisible = bModifierVisible,
                            .EditVisible = bShowEditControls,
                            .ReadVisible = NOT bShowEditControls,
                            .RemoveVisible = bShowEditControls && ModifierTag.IsValid(),
                        }));
                        Modifiers.Add(ModifierKey, Modifier);
                        RouteAttributeKeys.Add(ModifierKey, AttributeKey);
                        RouteComponents.Add(ModifierKey, Component);
                    }

                    if (ComponentModifiers.IsEmpty() || NOT bShowEditControls) { continue; }

                    const auto RowLabel = ck::Format_UE(
                        TEXT("  ↳ {} [{}] modifiers:"), AttributeName, ComponentName);
                    const auto bClearVisible = ck_inspector_integer_attributes::Matches_Row(
                        _Filter, AttributeName, RowLabel, TEXT("Clear All"));
                    const auto ClearKey = ck::Format_UE(
                        TEXT("clear/{}/{}"), AttributeKey, ComponentName);
                    Records.Add(ck_inspector_integer_attributes::Make_Record({
                        .Key = ClearKey,
                        .Name = AttributeName,
                        .RowLabel = RowLabel,
                        .ActionTooltip = ck::Format_UE(
                            TEXT("Request_ClearAllModifiers on [{}]'s [{}] component. "
                                 "Clears revocable modifiers only."),
                            AttributeName, ComponentName),
                        .DiffMarked = _DiffLabels.Contains(RowLabel),
                        .RecordVisible = bClearVisible,
                        .ClearVisible = bClearVisible,
                    }));
                    RouteAttributeKeys.Add(ClearKey, AttributeKey);
                    RouteComponents.Add(ClearKey, Component);
                }
            });
    }

    const FCkUiLoadResult RecordsResult = _Records->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded)
    {
        _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n"));
        return false;
    }

    _Attributes = MoveTemp(Attributes);
    _Modifiers = MoveTemp(Modifiers);
    _Refills = MoveTemp(Refills);
    _RouteAttributeKeys = MoveTemp(RouteAttributeKeys);
    _RouteComponents = MoveTemp(RouteComponents);
    return true;
}

auto SCkInspector_IntegerAttributesAuthored::Resolve_Attribute(
    const FString& InKey,
    FCk_Handle_IntegerAttribute& OutAttribute) const -> bool
{
    OutAttribute = {};
    const auto* Candidate = _Attributes.Find(InKey);
    if (Candidate == nullptr
        || NOT ck_inspector_integer_attributes::Is_AttributeLive(_Entity, *Candidate))
    { return false; }

    auto MutableOwner = _Entity;
    auto bFound = false;
    UCk_Utils_IntegerAttribute_UE::ForEach(
        MutableOwner,
        [Candidate, &bFound](FCk_Handle_IntegerAttribute InCurrent)
        {
            if (InCurrent == *Candidate) { bFound = true; }
        });
    if (NOT bFound) { return false; }

    OutAttribute = *Candidate;
    return true;
}

auto SCkInspector_IntegerAttributesAuthored::Resolve_Modifier(
    const FString& InKey,
    FCk_Handle_IntegerAttributeModifier& OutModifier) const -> bool
{
    OutModifier = {};
    const auto* Candidate = _Modifiers.Find(InKey);
    const auto* AttributeKey = _RouteAttributeKeys.Find(InKey);
    const auto* Component = _RouteComponents.Find(InKey);
    if (Candidate == nullptr || AttributeKey == nullptr || Component == nullptr)
    { return false; }

    auto Attribute = FCk_Handle_IntegerAttribute{};
    if (NOT Resolve_Attribute(*AttributeKey, Attribute)
        || NOT UCk_Utils_IntegerAttribute_UE::Has_Component(Attribute, *Component)
        || ck_inspector_integer_attributes::Is_Destroying(FCk_Handle{*Candidate}))
    { return false; }
    if (ck_inspector_attribute_rows::Get_IsInternalReplicationModifier(*Candidate))
    { return false; }

    auto MutableAttribute = Attribute;
    auto bFound = false;
    UCk_Utils_IntegerAttributeModifier_UE::ForEach(
        MutableAttribute,
        [Candidate, &bFound](FCk_Handle_IntegerAttributeModifier InCurrent)
        {
            if (InCurrent == *Candidate) { bFound = true; }
        },
        *Component);
    if (NOT bFound) { return false; }

    OutModifier = *Candidate;
    return true;
}

auto SCkInspector_IntegerAttributesAuthored::Resolve_Refill(
    const FString& InKey,
    FCk_Handle_IntegerAttributeRefill& OutRefill) const -> bool
{
    OutRefill = {};
    const auto* Candidate = _Refills.Find(InKey);
    if (Candidate == nullptr
        || ck_inspector_integer_attributes::Is_Destroying(FCk_Handle{*Candidate}))
    { return false; }

    auto Attribute = FCk_Handle_IntegerAttribute{};
    if (NOT Resolve_Attribute(InKey, Attribute)
        || NOT UCk_Utils_IntegerAttribute_UE::Has_RefillAttribute(Attribute))
    { return false; }

    const auto Current = UCk_Utils_IntegerAttribute_UE::TryGet_RefillAttribute(Attribute);
    if (ck::Is_NOT_Valid(Current) || Current != *Candidate
        || NOT FCk_Handle{Current}.Has<ck::FFragment_FloatAttribute_Current>())
    { return false; }

    OutRefill = *Candidate;
    return true;
}

auto SCkInspector_IntegerAttributesAuthored::Commit_Override(
    const FString& InKey,
    const int32 InValue,
    const ECk_MinMaxCurrent InComponent) -> void
{
    auto Attribute = FCk_Handle_IntegerAttribute{};
    if (NOT Get_CanRequest()
        || NOT Resolve_Attribute(InKey, Attribute)
        || NOT UCk_Utils_IntegerAttribute_UE::Has_Component(Attribute, InComponent))
    { return; }

    UCk_Utils_IntegerAttribute_UE::Request_Override(
        Attribute,
        InValue,
        InComponent,
        {});
}

auto SCkInspector_IntegerAttributesAuthored::Commit_Modifier(
    const FString& InKey,
    const int32 InValue) -> void
{
    auto Modifier = FCk_Handle_IntegerAttributeModifier{};
    if (NOT Get_CanRequest()
        || NOT Resolve_Modifier(InKey, Modifier))
    { return; }
    UCk_Utils_IntegerAttributeModifier_UE::Override(Modifier, InValue);
}

auto SCkInspector_IntegerAttributesAuthored::Remove_Modifier(const FString& InKey) -> void
{
    auto Modifier = FCk_Handle_IntegerAttributeModifier{};
    if (NOT Get_CanRequest() || NOT Resolve_Modifier(InKey, Modifier)) { return; }
    if (NOT ck_inspector_attribute_rows::Get_ModifierLabel(Modifier).IsValid()) { return; }
    UCk_Utils_IntegerAttributeModifier_UE::Remove(Modifier);
}

auto SCkInspector_IntegerAttributesAuthored::Clear_Component(const FString& InKey) -> void
{
    const auto* AttributeKey = _RouteAttributeKeys.Find(InKey);
    const auto* Component = _RouteComponents.Find(InKey);
    if (NOT Get_CanRequest() || AttributeKey == nullptr || Component == nullptr) { return; }

    auto Attribute = FCk_Handle_IntegerAttribute{};
    if (NOT Resolve_Attribute(*AttributeKey, Attribute)
        || NOT UCk_Utils_IntegerAttribute_UE::Has_Component(Attribute, *Component))
    { return; }

    auto MutableAttribute = Attribute;
    auto bHasVisibleModifier = false;
    UCk_Utils_IntegerAttributeModifier_UE::ForEach(
        MutableAttribute,
        [&bHasVisibleModifier](FCk_Handle_IntegerAttributeModifier InModifier)
        {
            if (ck::IsValid(InModifier)
                && NOT ck_inspector_attribute_rows::Get_IsInternalReplicationModifier(InModifier))
            { bHasVisibleModifier = true; }
        },
        *Component);
    if (NOT bHasVisibleModifier) { return; }

    UCk_Utils_IntegerAttributeModifier_UE::Request_ClearAllModifiers(
        Attribute, *Component, {});
}

auto SCkInspector_IntegerAttributesAuthored::Pause_Refill(const FString& InKey) -> void
{
    auto Refill = FCk_Handle_IntegerAttributeRefill{};
    if (NOT Get_CanRequest() || NOT Resolve_Refill(InKey, Refill)
        || UCk_Utils_IntegerAttributeRefill_UE::Get_RefillState(Refill)
            != ECk_Attribute_RefillState::Running)
    { return; }
    UCk_Utils_IntegerAttributeRefill_UE::Request_Pause(Refill, {});
}

auto SCkInspector_IntegerAttributesAuthored::Resume_Refill(const FString& InKey) -> void
{
    auto Refill = FCk_Handle_IntegerAttributeRefill{};
    if (NOT Get_CanRequest() || NOT Resolve_Refill(InKey, Refill)
        || UCk_Utils_IntegerAttributeRefill_UE::Get_RefillState(Refill)
            != ECk_Attribute_RefillState::Paused)
    { return; }
    UCk_Utils_IntegerAttributeRefill_UE::Request_Resume(Refill, {});
}

auto SCkInspector_IntegerAttributesAuthored::Navigate(const FString& InKey) -> void
{
    auto Attribute = FCk_Handle_IntegerAttribute{};
    if (NOT Resolve_Attribute(InKey, Attribute)) { return; }
    if (const auto SelectionModel = _SelectionModel.Pin(); SelectionModel.IsValid())
    { SelectionModel->Set_SelectedEntities({FCk_Handle{Attribute}}); }
}

auto SCkInspector_IntegerAttributesAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_IntegerAttributesAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Visibility.Add(TEXT("integer-attributes-available"),
        TAttribute<bool>::CreateLambda([WeakWidget]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() && Widget->Get_IsAvailable();
        }));
    Data.Visibility.Add(TEXT("integer-attributes-can-request"),
        TAttribute<bool>::CreateLambda([WeakWidget]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() && Widget->Get_CanRequest();
        }));
    Data.Text.Add(TEXT("integer-attributes-disabled-reason"),
        TAttribute<FText>::CreateLambda([WeakWidget]()
        {
            const auto Widget = WeakWidget.Pin();
            return FText::FromString(
                Widget.IsValid() ? Widget->Get_RequestDisabledReason() : FString{});
        }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    Data.Collections.Add(TEXT("integer-attributes"), _Records);
    Data.ItemActions.Add(TEXT("integer-attribute-navigate"),
        FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InKey)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Navigate(InKey); }
        }));
    Data.ItemActions.Add(TEXT("integer-attribute-remove"),
        FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InKey)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Remove_Modifier(InKey); }
        }));
    Data.ItemActions.Add(TEXT("integer-attribute-clear"),
        FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InKey)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Clear_Component(InKey); }
        }));
    Data.ItemActions.Add(TEXT("integer-attribute-refill-pause"),
        FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InKey)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Pause_Refill(InKey); }
        }));
    Data.ItemActions.Add(TEXT("integer-attribute-refill-resume"),
        FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InKey)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Resume_Refill(InKey); }
        }));
    Data.ItemIntegerCommitted.Add(TEXT("integer-attribute-current-override"),
        FCkUiOnItemIntegerCommitted::CreateLambda(
            [WeakWidget](const FString& InKey, const int32 InValue, ETextCommit::Type)
            {
                if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
                { Widget->Commit_Override(InKey, InValue, ECk_MinMaxCurrent::Current); }
            }));
    Data.ItemIntegerCommitted.Add(TEXT("integer-attribute-min-override"),
        FCkUiOnItemIntegerCommitted::CreateLambda(
            [WeakWidget](const FString& InKey, const int32 InValue, ETextCommit::Type)
            {
                if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
                { Widget->Commit_Override(InKey, InValue, ECk_MinMaxCurrent::Min); }
            }));
    Data.ItemIntegerCommitted.Add(TEXT("integer-attribute-max-override"),
        FCkUiOnItemIntegerCommitted::CreateLambda(
            [WeakWidget](const FString& InKey, const int32 InValue, ETextCommit::Type)
            {
                if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
                { Widget->Commit_Override(InKey, InValue, ECk_MinMaxCurrent::Max); }
            }));
    Data.ItemIntegerCommitted.Add(TEXT("integer-attribute-modifier"),
        FCkUiOnItemIntegerCommitted::CreateLambda(
            [WeakWidget](const FString& InKey, const int32 InValue, ETextCommit::Type)
            {
                if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
                { Widget->Commit_Modifier(InKey, InValue); }
            }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorIntegerAttributes.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorIntegerAttributes.ui.css")));
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

auto SCkInspector_IntegerAttributesAuthored::Tick(
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
    else
    { _LoadError.Reset(); }
}

auto SCkInspector_IntegerAttributesAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _SelectionModel.Reset();
    _DiffLabels.Reset();
    _Attributes.Reset();
    _Modifiers.Reset();
    _Refills.Reset();
    _RouteAttributeKeys.Reset();
    _RouteComponents.Reset();
    _Records.Reset();
    _View.Reset();
    _Mounted = false;
}

// --------------------------------------------------------------------------------------------------------------------

FCkInspector_IntegerAttributes::~FCkInspector_IntegerAttributes()
{
    OnDeactivated();
}

auto FCkInspector_IntegerAttributes::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Integer Attributes"));
}

auto FCkInspector_IntegerAttributes::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return NOT ck_inspector_integer_attributes::Is_Destroying(Entity)
        && UCk_Utils_IntegerAttribute_UE::Has_Any(Entity);
}

auto FCkInspector_IntegerAttributes::Build_NativeBody(
    const FCk_Handle& InEntity,
    const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    Builder.SetEditGuard(Get_EditGuard());
    const TWeakPtr<FCkDebuggerModel_EntitySelection> WeakSelectionModel = SelectionModel;

    if (ck_inspector_integer_attributes::Is_Destroying(InEntity)
        || NOT UCk_Utils_IntegerAttribute_UE::Has_Any(InEntity))
    { return Builder.Build(InEntity, InFilter); }

    auto MutableEntity = InEntity;
    UCk_Utils_IntegerAttribute_UE::ForEach(
        MutableEntity,
        [&Builder, WeakSelectionModel](FCk_Handle_IntegerAttribute InAttribute)
        {
            const auto AttributeTag = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
            const auto AttributeName = AttributeTag.IsValid()
                ? AttributeTag.ToString()
                : FString{TEXT("Unnamed")};
            const auto bHasMin = UCk_Utils_IntegerAttribute_UE::Has_Component(
                InAttribute, ECk_MinMaxCurrent::Min);
            const auto bHasMax = UCk_Utils_IntegerAttribute_UE::Has_Component(
                InAttribute, ECk_MinMaxCurrent::Max);
            const FCk_Handle AttributeHandle{InAttribute};
            const auto OnClicked = FCkInspectorWidgetBuilder::FOnClicked{
                [WeakSelectionModel, AttributeHandle]()
                {
                    if (const auto CurrentSelectionModel = WeakSelectionModel.Pin();
                        CurrentSelectionModel.IsValid() && ck::IsValid(AttributeHandle))
                    { CurrentSelectionModel->Set_SelectedEntities({AttributeHandle}); }
                }};

            if (bHasMin && bHasMax)
            {
                Builder.AddMeterRow(
                    FText::FromString(AttributeName),
                    TAttribute<float>::CreateLambda([InAttribute]()
                    { return ck_inspector_integer_attributes::Get_Fraction(InAttribute); }),
                    ECk_Tone::Accent,
                    TAttribute<FText>::CreateLambda([InAttribute]()
                    {
                        return FText::FromString(ck_inspector_integer_attributes::Get_Summary(
                            InAttribute, true, true));
                    }),
                    OnClicked);
            }
            else
            {
                Builder.AddClickableRow(
                    FText::FromString(AttributeName),
                    [InAttribute, bHasMax](const FCk_Handle&)
                    {
                        return FText::FromString(ck_inspector_integer_attributes::Get_Summary(
                            InAttribute, bHasMax, false));
                    },
                    CkStyle::Attribute(),
                    OnClicked);
            }

            ck_inspector_attribute_rows::Add_OverrideRows<UCk_Utils_IntegerAttribute_UE>(
                Builder, AttributeName, InAttribute);
            ck_inspector_attribute_rows::Add_RefillRows<
                UCk_Utils_IntegerAttribute_UE,
                UCk_Utils_IntegerAttributeRefill_UE>(Builder, AttributeName, InAttribute);
            ck_inspector_attribute_rows::Add_ModifierRows<
                UCk_Utils_IntegerAttribute_UE,
                UCk_Utils_IntegerAttributeModifier_UE>(Builder, AttributeName, InAttribute);
        });
    return Builder.Build(InEntity, InFilter);
}

auto FCkInspector_IntegerAttributes::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return Build_Inspector(Entity, FString{});
}

auto FCkInspector_IntegerAttributes::Build_Inspector(
    const FCk_Handle& Entity,
    const FString& InFilter) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity, InFilter);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    auto DiffLabels = TSet<FString>{};
    const auto CaptureDiff = [&DiffLabels](const FString& InLabel)
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(InLabel))
        { DiffLabels.Add(InLabel); }
    };
    if (CanInspect(Entity))
    {
        auto MutableEntity = Entity;
        UCk_Utils_IntegerAttribute_UE::ForEach(
            MutableEntity,
            [&CaptureDiff](FCk_Handle_IntegerAttribute InAttribute)
            {
                const auto AttributeTag = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
                const auto AttributeName = AttributeTag.IsValid()
                    ? AttributeTag.ToString()
                    : FString{TEXT("Unnamed")};
                CaptureDiff(AttributeName);

                auto ComponentCount = 0;
                for (const auto Component : {
                    ECk_MinMaxCurrent::Current,
                    ECk_MinMaxCurrent::Min,
                    ECk_MinMaxCurrent::Max})
                {
                    ComponentCount += UCk_Utils_IntegerAttribute_UE::Has_Component(
                        InAttribute, Component) ? 1 : 0;
                }
                if (ComponentCount > 1)
                {
                    CaptureDiff(ck::Format_UE(
                        TEXT("  ↳ {} component:"), AttributeName));
                }
                CaptureDiff(ck::Format_UE(TEXT("  ↳ {} override:"), AttributeName));
                if (UCk_Utils_IntegerAttribute_UE::Has_RefillAttribute(InAttribute))
                {
                    CaptureDiff(ck::Format_UE(TEXT("  ↳ {} refill:"), AttributeName));
                    CaptureDiff(ck::Format_UE(TEXT("  ↳ {} refill control:"), AttributeName));
                }

                for (const auto Component : {
                    ECk_MinMaxCurrent::Current,
                    ECk_MinMaxCurrent::Min,
                    ECk_MinMaxCurrent::Max})
                {
                    if (NOT UCk_Utils_IntegerAttribute_UE::Has_Component(InAttribute, Component))
                    { continue; }
                    const auto ComponentName = ck_inspector_integer_attributes::Get_ComponentName(Component);
                    auto MutableAttribute = InAttribute;
                    UCk_Utils_IntegerAttributeModifier_UE::ForEach(
                        MutableAttribute,
                        [&CaptureDiff, &AttributeName, &ComponentName](
                            FCk_Handle_IntegerAttributeModifier InModifier)
                        {
                            if (ck::Is_NOT_Valid(InModifier)
                                || ck_inspector_attribute_rows::Get_IsInternalReplicationModifier(InModifier))
                            { return; }
                            const auto ModifierTag = ck_inspector_attribute_rows::Get_ModifierLabel(InModifier);
                            const auto ModifierName = ModifierTag.IsValid()
                                ? ModifierTag.ToString()
                                : FString{TEXT("(not revocable)")};
                            CaptureDiff(ck::Format_UE(
                                TEXT("  ↳ {} [{}] {} delta:"),
                                AttributeName, ComponentName, ModifierName));
                        },
                        Component);
                    CaptureDiff(ck::Format_UE(
                        TEXT("  ↳ {} [{}] modifiers:"), AttributeName, ComponentName));
                }
            });
    }

    const TSharedRef<SCkInspector_IntegerAttributesAuthored> Authored =
        SNew(SCkInspector_IntegerAttributesAuthored)
            .Entity(Entity)
            .Filter(InFilter)
            .DiffLabels(MoveTemp(DiffLabels))
            .SelectionModel(SelectionModel);
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _Instances.Add(Authored);
    return Authored;
}

auto FCkInspector_IntegerAttributes::Tick(const FCk_Handle&, float) -> void
{
    _Instances.RemoveAll([](const TWeakPtr<SCkInspector_IntegerAttributesAuthored>& InInstance)
    {
        const auto Instance = InInstance.Pin();
        return NOT Instance.IsValid() || Instance->Is_Inert();
    });
}

auto FCkInspector_IntegerAttributes::OnDeactivated() -> void
{
    for (const auto& WeakInstance : _Instances)
    {
        if (const auto Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _Instances.Reset();
}
