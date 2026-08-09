#include "CkInspector_IskmRenderer.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkIskmRenderer/AnimCollection/CkIskmAnimCollection_Fragment_Data.h"
#include "CkIskmRenderer/Renderer/CkIskmRenderer_Fragment_Data.h"
#include "CkIskmRenderer/Renderer/CkIskmRenderer_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IskmRenderer)

// =====================================================================================================================

auto FCkInspector_IskmRenderer::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Iskm Renderer"));
}

auto FCkInspector_IskmRenderer::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_IskmRenderer_UE::Has(Entity);
}

auto FCkInspector_IskmRenderer::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildIskmRendererGrid(Entity);
}

// =====================================================================================================================

auto FCkInspector_IskmRenderer::BuildIskmRendererGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto RendererHandle = UCk_Utils_IskmRenderer_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(RendererHandle))
    {
        return Builder.Build(Entity, FString());
    }

    const auto CapturedRenderer = RendererHandle;

    // RendererData asset name
    Builder.AddRow(
        FText::FromString(TEXT("RendererData:")),
        [CapturedRenderer](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedRenderer)) { return FText::FromString(TEXT("--")); }
            const auto* Data = UCk_Utils_IskmRenderer_UE::Get_RendererData(CapturedRenderer);
            if (NOT ck::IsValid(Data, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("None")); }
            return FText::FromString(Data->GetName());
        },
        CkStyle::Value_Object());

    // AnimCollection asset name
    Builder.AddRow(
        FText::FromString(TEXT("AnimCollection:")),
        [CapturedRenderer](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedRenderer)) { return FText::FromString(TEXT("--")); }
            const auto* Collection = UCk_Utils_IskmRenderer_UE::Get_AnimCollection(CapturedRenderer);
            if (NOT ck::IsValid(Collection, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("None")); }
            return FText::FromString(Collection->GetName());
        },
        CkStyle::Value_Object());

    // Sequences count
    Builder.AddRow(
        FText::FromString(TEXT("Sequences (count):")),
        [CapturedRenderer](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedRenderer)) { return FText::FromString(TEXT("--")); }
            const auto* Collection = UCk_Utils_IskmRenderer_UE::Get_AnimCollection(CapturedRenderer);
            if (NOT ck::IsValid(Collection, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("--")); }
            return FText::FromString(FString::FromInt(Collection->Get_Sequences().Num()));
        },
        CkStyle::Value_Numeric());

    // Submeshes count
    Builder.AddRow(
        FText::FromString(TEXT("Submeshes (count):")),
        [CapturedRenderer](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedRenderer)) { return FText::FromString(TEXT("--")); }
            const auto* Data = UCk_Utils_IskmRenderer_UE::Get_RendererData(CapturedRenderer);
            if (NOT ck::IsValid(Data, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("--")); }
            return FText::FromString(FString::FromInt(Data->Get_Submeshes().Num()));
        },
        CkStyle::Value_Numeric());

    // Default AnimInstance class
    Builder.AddRow(
        FText::FromString(TEXT("Default AnimInstance:")),
        [CapturedRenderer](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedRenderer)) { return FText::FromString(TEXT("--")); }
            const auto* Data = UCk_Utils_IskmRenderer_UE::Get_RendererData(CapturedRenderer);
            if (NOT ck::IsValid(Data, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("--")); }
            const auto& ClassRef = Data->Get_DefaultAnimInstanceClass();
            if (ClassRef.IsNull()) { return FText::FromString(TEXT("(IskmNotify fallback)")); }
            return FText::FromString(ClassRef.GetAssetName());
        },
        CkStyle::Value_Object());

    // Custom Data Slots count
    Builder.AddRow(
        FText::FromString(TEXT("Custom Data Slots:")),
        [CapturedRenderer](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedRenderer)) { return FText::FromString(TEXT("--")); }
            const auto* Data = UCk_Utils_IskmRenderer_UE::Get_RendererData(CapturedRenderer);
            if (NOT ck::IsValid(Data, ck::IsValid_Policy_NullptrOnly{})) { return FText::FromString(TEXT("--")); }
            return FText::FromString(FString::FromInt(Data->Get_NumCustomDataFloat()));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================

auto FCkInspector_IskmRenderer::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
