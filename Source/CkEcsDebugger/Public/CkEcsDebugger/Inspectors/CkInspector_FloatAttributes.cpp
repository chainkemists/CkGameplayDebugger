#include "CkInspector_FloatAttributes.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkAttribute/FloatAttribute/CkFloatAttribute_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_FloatAttributes)

auto FCkInspector_FloatAttributes::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Float Attributes"));
}

auto FCkInspector_FloatAttributes::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_FloatAttribute_UE::Has_Any(Entity);
}

auto FCkInspector_FloatAttributes::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildAttributeGrid(Entity, FString());
}

auto FCkInspector_FloatAttributes::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildAttributeGrid(Entity, InFilter);
}

auto FCkInspector_FloatAttributes::BuildAttributeGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    UCk_Utils_FloatAttribute_UE::ForEach(MutableEntity, [&Builder, WeakSelectionModel](FCk_Handle_FloatAttribute InAttribute)
    {
        const auto AttributeTag = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
        const auto AttributeName = AttributeTag.IsValid()
            ? AttributeTag.ToString()
            : TEXT("Unnamed");

        const auto HasMin = UCk_Utils_FloatAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Min);
        const auto HasMax = UCk_Utils_FloatAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Max);
        const auto CapturedTag = AttributeTag;

        const auto AttributeHandle = FCk_Handle(InAttribute);

        Builder.AddClickableRow(
            FText::FromString(AttributeName),
            [CapturedTag, HasMin, HasMax](const FCk_Handle& E)
            {
                auto MutableE = E;
                const auto Attr = UCk_Utils_FloatAttribute_UE::TryGet(MutableE, CapturedTag);
                if (ck::Is_NOT_Valid(Attr)) { return FText::GetEmpty(); }

                const auto Final = UCk_Utils_FloatAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent::Current);
                const auto Base = UCk_Utils_FloatAttribute_UE::Get_BaseValue(Attr, ECk_MinMaxCurrent::Current);

                auto Result = ck::Format_UE(TEXT("{:.2f}"), Final);

                if (Base != Final)
                {
                    Result += ck::Format_UE(TEXT(" (Base: {:.2f})"), Base);
                }

                if (HasMin && HasMax)
                {
                    const auto MinVal = UCk_Utils_FloatAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent::Min);
                    const auto MaxVal = UCk_Utils_FloatAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent::Max);
                    Result += ck::Format_UE(TEXT("  [{:.0f}..{:.0f}]"), MinVal, MaxVal);
                }
                else if (HasMax)
                {
                    const auto MaxVal = UCk_Utils_FloatAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent::Max);
                    Result += ck::Format_UE(TEXT("  / {:.0f}"), MaxVal);
                }

                const auto PreClamp = UCk_Utils_FloatAttribute_UE::Get_PreClampFinalValue(Attr);
                if (PreClamp > Final)
                { Result += ck::Format_UE(TEXT(" (preclamp max: {:.2f})"), PreClamp); }
                else if (PreClamp < Final)
                { Result += ck::Format_UE(TEXT(" (preclamp min: {:.2f})"), PreClamp); }

                return FText::FromString(Result);
            },
            CkDebugStyle::Attribute(),
            [WeakSelectionModel, AttributeHandle]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(AttributeHandle))
                {
                    WeakSelectionModel->Set_SelectedEntities({ AttributeHandle });
                }
            });
    });

    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_FloatAttributes::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
