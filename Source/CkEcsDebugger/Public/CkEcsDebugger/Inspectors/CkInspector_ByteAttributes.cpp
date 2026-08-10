#include "CkInspector_ByteAttributes.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkAttribute/ByteAttribute/CkByteAttribute_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ByteAttributes)

namespace ck_inspector_byte_attributes
{
    // A meter only means something when BOTH bounds exist: the fill is current-in-range, so an
    // attribute floored at 20 reads empty at 20 rather than two-thirds full. Max-only and unbounded
    // attributes keep the plain text row — there is no range to fill.
    auto Get_Fraction(FCk_Handle_ByteAttribute InAttribute) -> float
    {
        if (ck::Is_NOT_Valid(InAttribute)) { return 0.0f; }

        const auto MinValue = static_cast<int32>(UCk_Utils_ByteAttribute_UE::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Min));
        const auto MaxValue = static_cast<int32>(UCk_Utils_ByteAttribute_UE::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Max));
        const auto Current  = static_cast<int32>(UCk_Utils_ByteAttribute_UE::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Current));

        const auto Range = MaxValue - MinValue;
        if (Range == 0) { return 0.0f; }

        return static_cast<float>(Current - MinValue) / static_cast<float>(Range);
    }

    // Base and pre-clamp only appear when they DISAGREE with the final value — otherwise every row
    // would carry two redundant numbers.
    auto Get_Annotations(FCk_Handle_ByteAttribute InAttribute) -> FString
    {
        if (ck::Is_NOT_Valid(InAttribute)) { return FString{}; }

        const auto Final = UCk_Utils_ByteAttribute_UE::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Current);
        const auto Base  = UCk_Utils_ByteAttribute_UE::Get_BaseValue(InAttribute, ECk_MinMaxCurrent::Current);

        auto Result = FString{};

        if (Base != Final)
        { Result += ck::Format_UE(TEXT(" (Base: {})"), Base); }

        const auto PreClamp = UCk_Utils_ByteAttribute_UE::Get_PreClampFinalValue(InAttribute);
        if (PreClamp > Final)
        { Result += ck::Format_UE(TEXT(" (preclamp max: {})"), PreClamp); }
        else if (PreClamp < Final)
        { Result += ck::Format_UE(TEXT(" (preclamp min: {})"), PreClamp); }

        return Result;
    }
}

auto FCkInspector_ByteAttributes::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Byte Attributes"));
}

auto FCkInspector_ByteAttributes::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_ByteAttribute_UE::Has_Any(Entity);
}

auto FCkInspector_ByteAttributes::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildAttributeGrid(Entity, FString());
}

auto FCkInspector_ByteAttributes::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildAttributeGrid(Entity, InFilter);
}

auto FCkInspector_ByteAttributes::BuildAttributeGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    UCk_Utils_ByteAttribute_UE::ForEach(MutableEntity, [&Builder, WeakSelectionModel](FCk_Handle_ByteAttribute InAttribute)
    {
        const auto AttributeTag = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
        const auto AttributeName = AttributeTag.IsValid()
            ? AttributeTag.ToString()
            : TEXT("Unnamed");

        const auto HasMin = UCk_Utils_ByteAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Min);
        const auto HasMax = UCk_Utils_ByteAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Max);
        const auto CapturedTag = AttributeTag;

        const auto AttributeHandle = FCk_Handle(InAttribute);

        const auto OnClicked = FCkInspectorWidgetBuilder::FOnClicked{
            [WeakSelectionModel, AttributeHandle]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(AttributeHandle))
                {
                    WeakSelectionModel->Set_SelectedEntities({ AttributeHandle });
                }
            }};

        if (HasMin && HasMax)
        {
            const auto CapturedAttribute = InAttribute;

            Builder.AddMeterRow(
                FText::FromString(AttributeName),
                TAttribute<float>::CreateLambda([CapturedAttribute]()
                {
                    return ck_inspector_byte_attributes::Get_Fraction(CapturedAttribute);
                }),
                ECk_Tone::Accent,
                TAttribute<FText>::CreateLambda([CapturedAttribute]()
                {
                    if (ck::Is_NOT_Valid(CapturedAttribute)) { return FText::FromString(TEXT("--")); }

                    const auto Current  = UCk_Utils_ByteAttribute_UE::Get_FinalValue(CapturedAttribute, ECk_MinMaxCurrent::Current);
                    const auto MaxValue = UCk_Utils_ByteAttribute_UE::Get_FinalValue(CapturedAttribute, ECk_MinMaxCurrent::Max);

                    return FText::FromString(
                        ck::Format_UE(TEXT("{} / {}"), Current, MaxValue)
                        + ck_inspector_byte_attributes::Get_Annotations(CapturedAttribute));
                }),
                OnClicked);

            return;
        }

        Builder.AddClickableRow(
            FText::FromString(AttributeName),
            [CapturedTag, HasMax](const FCk_Handle& E)
            {
                auto MutableE = E;
                const auto Attr = UCk_Utils_ByteAttribute_UE::TryGet(MutableE, CapturedTag);
                if (ck::Is_NOT_Valid(Attr)) { return FText::GetEmpty(); }

                const auto Final = UCk_Utils_ByteAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent::Current);

                auto Result = ck::Format_UE(TEXT("{}"), Final);

                if (HasMax)
                {
                    const auto MaxVal = UCk_Utils_ByteAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent::Max);
                    Result += ck::Format_UE(TEXT("  / {}"), MaxVal);
                }

                Result += ck_inspector_byte_attributes::Get_Annotations(Attr);

                return FText::FromString(Result);
            },
            CkStyle::Attribute(),
            OnClicked);
    });

    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_ByteAttributes::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
