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

namespace ck_inspector_resolver
{
    auto Get_PhaseCount(
        const FCk_Handle& InEntity)
        -> int32
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_ResolverDataBundle_Params>())
        { return 0; }

        return InEntity.Get<ck::FFragment_ResolverDataBundle_Params>().Get_Params().Get_Phases().Num();
    }

    // INDEX_NONE until the bundle enters its first phase.
    auto Get_PhaseIndex(
        const FCk_Handle& InEntity)
        -> int32
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_ResolverDataBundle_Current>())
        { return INDEX_NONE; }

        return InEntity.Get<ck::FFragment_ResolverDataBundle_Current>().Get_CurrentPhaseIndex();
    }
}

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

    // Ordinal, not the raw index the row used to print: "3 / 4" is how far through the configured phase list the
    // bundle has resolved.
    Builder.AddMeterRow(
        FText::FromString(TEXT("Phase:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        {
            const auto PhaseCount = ck_inspector_resolver::Get_PhaseCount(CapturedEntity);
            const auto PhaseIndex = ck_inspector_resolver::Get_PhaseIndex(CapturedEntity);

            if (PhaseCount <= 0 || PhaseIndex < 0)
            { return 0.0f; }

            return static_cast<float>(PhaseIndex + 1) / static_cast<float>(PhaseCount);
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            const auto PhaseCount = ck_inspector_resolver::Get_PhaseCount(CapturedEntity);
            const auto PhaseIndex = ck_inspector_resolver::Get_PhaseIndex(CapturedEntity);

            if (PhaseIndex < 0)
            { return FText::FromString(ck::Format_UE(TEXT("-- / {}"), PhaseCount)); }

            return FText::FromString(ck::Format_UE(TEXT("{} / {}"), PhaseIndex + 1, PhaseCount));
        }));

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

        Builder.AddCountBadgeRow(
            FText::FromString(TEXT("Modifier Ops:")),
            TAttribute<int32>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_ResolverDataBundle_PendingOperations>())
                { return 0; }
                return CapturedEntity.Get<ck::FFragment_ResolverDataBundle_PendingOperations>().Get_PendingModifiersOperations().Num();
            }),
            ECk_Tone::Info);

        Builder.AddCountBadgeRow(
            FText::FromString(TEXT("Metadata Ops:")),
            TAttribute<int32>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_ResolverDataBundle_PendingOperations>())
                { return 0; }
                return CapturedEntity.Get<ck::FFragment_ResolverDataBundle_PendingOperations>().Get_PendingMetadataOperations().Num();
            }),
            ECk_Tone::Info);
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Resolver::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
