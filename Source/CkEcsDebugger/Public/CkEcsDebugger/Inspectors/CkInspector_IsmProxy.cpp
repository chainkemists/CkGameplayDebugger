#include "CkInspector_IsmProxy.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Engine/StaticMesh.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IsmProxy)

static const FLinearColor Color_Mesh     = FLinearColor(0.55f, 0.78f, 0.95f);
static const FLinearColor Color_Mobility = FLinearColor(0.85f, 0.75f, 0.55f);
static const FLinearColor Color_Offset   = FLinearColor(0.75f, 0.85f, 0.55f);

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
        Color_Mesh);

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
        Color_Mobility);

    // Location offset
    Builder.AddRow(
        FText::FromString(TEXT("Location Offset:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Offset = UCk_Utils_IsmProxy_UE::Get_LocalLocationOffset(CapturedProxy);
            return FText::FromString(ck::Format_UE(TEXT("X:{:.1f}, Y:{:.1f}, Z:{:.1f}"), Offset.X, Offset.Y, Offset.Z));
        },
        Color_Offset);

    // Rotation offset
    Builder.AddRow(
        FText::FromString(TEXT("Rotation Offset:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Rotation = UCk_Utils_IsmProxy_UE::Get_LocalRotationOffset(CapturedProxy);
            return FText::FromString(ck::Format_UE(TEXT("P:{:.2f}, Y:{:.2f}, R:{:.2f}"), Rotation.Pitch, Rotation.Yaw, Rotation.Roll));
        },
        Color_Offset);

    // Scale multiplier
    Builder.AddRow(
        FText::FromString(TEXT("Scale Multiplier:")),
        [CapturedProxy](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedProxy)) { return FText::FromString(TEXT("--")); }
            const auto Scale = UCk_Utils_IsmProxy_UE::Get_ScaleMultiplier(CapturedProxy);
            return FText::FromString(ck::Format_UE(TEXT("X:{:.2f}, Y:{:.2f}, Z:{:.2f}"), Scale.X, Scale.Y, Scale.Z));
        },
        Color_Offset);

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_IsmProxy::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
