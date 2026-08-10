#include "CkInspector_IsmProxy.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/StaticMesh.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IsmProxy)

// =====================================================================================================================

namespace ck_inspector_ism_proxy
{
    // Three fixed-precision components in X/Y/Z order so AddAlignedNumericRow's index-based axis
    // coloring lines up with the axis each number belongs to.
    static auto Make_AxisComponents(
        const FCk_Handle_IsmProxy& InProxy,
        const TCHAR* InFormat,
        TFunction<FVector(const FCk_Handle_IsmProxy&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InProxy, InFormat, InProjector, Axis]()
            {
                if (ck::Is_NOT_Valid(InProxy))
                { return FText::FromString(TEXT("--")); }

                return FText::FromString(ck::Format_UE(InFormat, InProjector(InProxy)[Axis]));
            }));
        }

        return Components;
    }
}

// =====================================================================================================================

auto FCkInspector_IsmProxy::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("ISM Proxy"));
}

auto FCkInspector_IsmProxy::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_IsmProxy_UE::Has(Entity);
}

auto FCkInspector_IsmProxy::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildIsmProxyGrid(Entity);
}

// =====================================================================================================================

auto FCkInspector_IsmProxy::BuildIsmProxyGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto ProxyHandle = UCk_Utils_IsmProxy_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(ProxyHandle))
    {
        return Builder.Build(Entity, FString());
    }

    // Mesh name
    const auto CapturedProxy = ProxyHandle;
    Builder.AddRow(
        FText::FromString(TEXT("Mesh:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto* Mesh = UCk_Utils_IsmProxy_UE::Get_Mesh(CapturedProxy);
            if (NOT ck::IsValid(Mesh)) { return FText::FromString(TEXT("None")); }
            return FText::FromString(Mesh->GetName());
        },
        CkStyle::Value_Object());

    // Mobility
    Builder.AddRow(
        FText::FromString(TEXT("Mobility:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Mobility = UCk_Utils_IsmProxy_UE::Get_Mobility(CapturedProxy);
            switch (Mobility)
            {
                case ECk_Mobility::Static:     return FText::FromString(TEXT("Static"));
                case ECk_Mobility::Movable:    return FText::FromString(TEXT("Movable"));
                case ECk_Mobility::Stationary: return FText::FromString(TEXT("Stationary"));
                default:                       return FText::FromString(TEXT("Unknown"));
            }
        },
        CkStyle::Value_Enum());

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Location Offset:")),
        ck_inspector_ism_proxy::Make_AxisComponents(CapturedProxy, TEXT("{:.1f}"),
            [](const FCk_Handle_IsmProxy& InProxy)
            { return UCk_Utils_IsmProxy_UE::Get_LocalLocationOffset(InProxy); }));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Rotation Offset (R,P,Y):")),
        ck_inspector_ism_proxy::Make_AxisComponents(CapturedProxy, TEXT("{:.2f}"),
            [](const FCk_Handle_IsmProxy& InProxy)
            {
                // Reordered to the axis each angle turns about (Roll=X, Pitch=Y, Yaw=Z) so the
                // row's X/Y/Z coloring agrees with every other axis row. The label states the order.
                const auto Rotation = UCk_Utils_IsmProxy_UE::Get_LocalRotationOffset(InProxy);
                return FVector{Rotation.Roll, Rotation.Pitch, Rotation.Yaw};
            }));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Scale Multiplier:")),
        ck_inspector_ism_proxy::Make_AxisComponents(CapturedProxy, TEXT("{:.2f}"),
            [](const FCk_Handle_IsmProxy& InProxy)
            { return UCk_Utils_IsmProxy_UE::Get_ScaleMultiplier(InProxy); }));

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_IsmProxy::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
