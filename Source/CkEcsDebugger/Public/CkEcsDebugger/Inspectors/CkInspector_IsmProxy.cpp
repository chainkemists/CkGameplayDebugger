#include "CkInspector_IsmProxy.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Fragment.h"
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
    Builder.SetEditGuard(Get_EditGuard());

    auto MutableEntity = Entity;
    const auto ProxyHandle = UCk_Utils_IsmProxy_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(ProxyHandle))
    {
        return Builder.Build(Entity, FString());
    }

    // Mesh name
    const auto CapturedProxy = ProxyHandle;
    const auto CapturedEntity = Entity;

    // Which custom-data index the value editor below addresses. The VALUE is a live read off
    // Get_CustomInstanceData, so only this selector is row-owned state.
    const auto CustomDataIndex = MakeShared<int32>(0);
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

    // ================================================================================================
    // Controls. Both writes are CosmeticOnly — an ISM instance is a render-side thing a dedicated
    // server never has.
    // ================================================================================================

    Builder.AddHeader(FText::FromString(TEXT("Controls")));

    // Enabled state is carried by an ECS tag, not a stored enum, so the switch reads the tag directly
    // (there is no Get_ on the Utils) and writes through the public request.
    Builder.AddToggleRow(
        FText::FromString(TEXT("Enabled:")),
        TAttribute<bool>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity)) { return false; }
            return NOT CapturedEntity.Has<ck::FTag_IsmProxy_Disabled>();
        }),
        [CapturedProxy](bool InIsEnabled)
        {
            auto Mutable = CapturedProxy;
            if (ck::Is_NOT_Valid(Mutable)) { return; }

            UCk_Utils_IsmProxy_UE::Request_EnableDisable(Mutable,
                FCk_Request_IsmProxy_EnableDisable{InIsEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable},
                {});
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    // AddNumericRow/AddIntegerRow carry no per-row tooltip, so the caveat that would otherwise hang off
    // the value editor gets its own dim note row instead of being lost.
    Builder.AddRow(
        FText::FromString(TEXT("Custom Data:")),
        [](const FCk_Handle&)
        { return FText::FromString(TEXT("post-setup writes only reach the GPU on Movable proxies")); },
        CkStyle::TextDim());

    Builder.AddIntegerRow(
        FText::FromString(TEXT("  Index:")),
        TAttribute<int32>::CreateLambda([CustomDataIndex]() { return *CustomDataIndex; }),
        [CustomDataIndex](int32 InIndex) { *CustomDataIndex = InIndex; },
        0,
        TOptional<int32>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("  Value:")),
        TAttribute<float>::CreateLambda([CapturedProxy, CustomDataIndex]()
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return 0.0f; }

            const auto Data = UCk_Utils_IsmProxy_UE::Get_CustomInstanceData(CapturedProxy);
            return Data.IsValidIndex(*CustomDataIndex) ? Data[*CustomDataIndex] : 0.0f;
        }),
        [CapturedProxy, CustomDataIndex](float InValue)
        {
            auto Mutable = CapturedProxy;
            if (ck::Is_NOT_Valid(Mutable)) { return; }

            UCk_Utils_IsmProxy_UE::Request_SetCustomInstanceDataValue(Mutable,
                FCk_Request_IsmProxy_SetCustomInstanceDataValue{*CustomDataIndex, InValue}, {});
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_IsmProxy::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
