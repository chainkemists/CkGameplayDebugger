#include "CkInspector_Resolver.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "ResolverDataBundle/CkResolverDataBundle_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Resolver)

// =====================================================================================================================

auto FCkInspector_Resolver::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Resolver"));
}

auto FCkInspector_Resolver::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && Entity.Has<ck::FFragment_ResolverDataBundle_Current>();
}

// =====================================================================================================================

auto FCkInspector_Resolver::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    if (NOT Entity.Has<ck::FFragment_ResolverDataBundle_Current>())
    { return Builder.Build(Entity); }

    // ---- Resolution result ----
    Builder.AddHeader(FText::FromString(TEXT("Resolution")));

    const auto CapturedEntity = Entity;

    Builder.AddRow(
        FText::FromString(TEXT("Final Value:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_ResolverDataBundle_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Value = CapturedEntity.Get<ck::FFragment_ResolverDataBundle_Current>().Get_FinalValue();
            return FText::FromString(FString::Printf(TEXT("%.3f"), Value));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Phase:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_ResolverDataBundle_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Phase = CapturedEntity.Get<ck::FFragment_ResolverDataBundle_Current>().Get_CurrentPhaseIndex();
            return FText::FromString(ck::Format_UE(TEXT("{}"), Phase));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Metadata Tags:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_ResolverDataBundle_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto& Tags = CapturedEntity.Get<ck::FFragment_ResolverDataBundle_Current>().Get_MetadataTags();
            return FText::FromString(Tags.IsEmpty()
                ? FString(TEXT("(none)"))
                : Tags.ToString());
        },
        CkStyle::Value_Tag());

    // ---- Pending operations ----
    if (Entity.Has<ck::FFragment_ResolverDataBundle_PendingOperations>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Pending")));

        Builder.AddRow(
            FText::FromString(TEXT("Modifier Ops:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_ResolverDataBundle_PendingOperations>())
                { return FText::FromString(TEXT("--")); }
                const auto Count = CapturedEntity.Get<ck::FFragment_ResolverDataBundle_PendingOperations>().Get_PendingModifiersOperations().Num();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Count));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Metadata Ops:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_ResolverDataBundle_PendingOperations>())
                { return FText::FromString(TEXT("--")); }
                const auto Count = CapturedEntity.Get<ck::FFragment_ResolverDataBundle_PendingOperations>().Get_PendingMetadataOperations().Num();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Count));
            },
            CkStyle::Value_Numeric());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Resolver::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
