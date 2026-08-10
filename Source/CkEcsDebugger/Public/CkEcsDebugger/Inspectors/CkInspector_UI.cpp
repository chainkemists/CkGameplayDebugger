#include "CkInspector_UI.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkWorldSpaceWidget/CkWorldSpaceWidget_Fragment.h"
#include "CkWorldSpaceWidget/CkWorldSpaceWidget_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_UI)

// =====================================================================================================================

namespace ck_inspector_ui
{
    // Live reads: the row attributes outlive the fragment, so every read re-validates the handle first.
    static auto Get_IsWrapperValid(
        const FCk_Handle& InEntity)
        -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Current>())
        { return false; }

        const auto* WrapperPtr = InEntity.Get<ck::FFragment_WorldSpaceWidget_Current>().Get_WrapperWidget().Get();
        return ck::IsValid(WrapperPtr, ck::IsValid_Policy_NullptrOnly{});
    }

    static auto Get_IsOwningPlayerValid(
        const FCk_Handle& InEntity)
        -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Current>())
        { return false; }

        return InEntity.Get<ck::FFragment_WorldSpaceWidget_Current>().Get_WidgetOwningPlayer().IsValid();
    }

    static auto Get_IsEnabled(
        const FCk_Handle& InEntity)
        -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Current>())
        { return false; }

        return InEntity.Get<ck::FFragment_WorldSpaceWidget_Current>().Get_Enabled();
    }

    // The Params fragment IS the live store for the three info structs: the request handler writes each
    // committed struct back into it (CkWorldSpaceWidget_Processor.cpp:316/345/357). Returned by VALUE so
    // the read-modify-write below never holds a fragment reference across a request.
    static auto Get_Params(
        const FCk_Handle& InEntity)
        -> FCk_Fragment_WorldSpaceWidget_ParamsData
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Params>())
        { return {}; }

        return InEntity.Get<ck::FFragment_WorldSpaceWidget_Params>();
    }

    static auto Get_ScalingInfo(
        const FCk_Handle& InEntity)
        -> FCk_WorldSpaceWidget_ScalingInfo
    {
        return Get_Params(InEntity).Get_ScalingInfo();
    }

    static auto Get_FadingInfo(
        const FCk_Handle& InEntity)
        -> FCk_WorldSpaceWidget_FadingInfo
    {
        return Get_Params(InEntity).Get_FadingInfo();
    }

    static auto Get_OcclusionInfo(
        const FCk_Handle& InEntity)
        -> FCk_WorldSpaceWidget_OcclusionInfo
    {
        return Get_Params(InEntity).Get_OcclusionInfo();
    }

    // The three policy fields are CONSTRUCTION-ONLY (CK_PROPERTY_GET, no setter), so switching a policy
    // rebuilds the struct around the scalars instead of mutating in place.
    static auto Make_ScalingInfo(
        const FCk_WorldSpaceWidget_ScalingInfo&  InSource,
        ECk_WorldSpaceWidget_Scaling_Policy      InPolicy)
        -> FCk_WorldSpaceWidget_ScalingInfo
    {
        return FCk_WorldSpaceWidget_ScalingInfo{InPolicy}
            .Set_MaxScale(InSource.Get_MaxScale())
            .Set_MinScale(InSource.Get_MinScale())
            .Set_ScaleFalloff_StartDistance(InSource.Get_ScaleFalloff_StartDistance())
            .Set_ScaleFalloff_EndDistance(InSource.Get_ScaleFalloff_EndDistance());
    }

    static auto Make_FadingInfo(
        const FCk_WorldSpaceWidget_FadingInfo& InSource,
        ECk_WorldSpaceWidget_Fading_Policy     InPolicy)
        -> FCk_WorldSpaceWidget_FadingInfo
    {
        return FCk_WorldSpaceWidget_FadingInfo{InPolicy}
            .Set_MaxOpacity(InSource.Get_MaxOpacity())
            .Set_MinOpacity(InSource.Get_MinOpacity())
            .Set_FadeFalloff_StartDistance(InSource.Get_FadeFalloff_StartDistance())
            .Set_FadeFalloff_EndDistance(InSource.Get_FadeFalloff_EndDistance());
    }

    static auto Make_OcclusionInfo(
        ECk_WorldSpaceWidget_Occlusion_Policy InPolicy,
        ECollisionChannel                     InChannel)
        -> FCk_WorldSpaceWidget_OcclusionInfo
    {
        return FCk_WorldSpaceWidget_OcclusionInfo{InPolicy}.Set_TraceChannel(InChannel);
    }

    // ECollisionChannel is a plain UENUM, so the channel field earns a real dropdown. The list is
    // reflection-driven rather than curated: a project renames its GameTraceChannels in collision
    // settings, and only the enum's own names are available from here. Built on first use, never at
    // static-init. Terminators and deprecated aliases are dropped by NAME rather than by the Hidden
    // metadata, which UEnum only carries under WITH_EDITOR.
    static auto Get_TraceChannels()
        -> const TArray<ECollisionChannel>&
    {
        static const auto Channels = []
        {
            auto Result = TArray<ECollisionChannel>{};

            const auto* EnumPtr = StaticEnum<ECollisionChannel>();
            if (ck::Is_NOT_Valid(EnumPtr, ck::IsValid_Policy_NullptrOnly{}))
            { return Result; }

            for (auto Index = 0; Index < EnumPtr->NumEnums(); ++Index)
            {
                const auto Name = EnumPtr->GetNameStringByIndex(Index);
                if (Name.EndsWith(TEXT("_MAX")) || Name.EndsWith(TEXT("_Deprecated")))
                { continue; }

                const auto Value = static_cast<ECollisionChannel>(EnumPtr->GetValueByIndex(Index));
                if (Value == ECollisionChannel::ECC_MAX)
                { continue; }

                Result.Add(Value);
            }

            return Result;
        }();

        return Channels;
    }

    static auto Get_TraceChannelOptions()
        -> const TArray<FText>&
    {
        static const auto Options = []
        {
            auto Result = TArray<FText>{};

            const auto* EnumPtr = StaticEnum<ECollisionChannel>();
            for (const auto Channel : Get_TraceChannels())
            { Result.Add(EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Channel))); }

            return Result;
        }();

        return Options;
    }
}

// =====================================================================================================================

auto FCkInspector_UI::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("UI"));
}

auto FCkInspector_UI::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has<ck::FFragment_WorldSpaceWidget_Current>();
}

// =====================================================================================================================

auto FCkInspector_UI::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    const auto CapturedEntity = Entity;

    auto MutableEntity = Entity;
    const auto CapturedWidget = UCk_Utils_WorldSpaceWidget_UE::Cast(MutableEntity);

    Builder.AddHeader(FText::FromString(TEXT("World Space Widget")));

    // Both were snapshotted at compose time before, so a widget that went away mid-session kept reading "Valid"
    // until the panel happened to rebuild. Pills read the fragment live instead.
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Wrapper Widget:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        { return FText::FromString(ck_inspector_ui::Get_IsWrapperValid(CapturedEntity) ? TEXT("Valid") : TEXT("Invalid")); }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_IsWrapperValid(CapturedEntity) ? ECk_Tone::Ok : ECk_Tone::Err; }));

    // An absent owning player is normal for a server-side or shared widget, so it reads Neutral rather than Err.
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Owning Player:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        { return FText::FromString(ck_inspector_ui::Get_IsOwningPlayerValid(CapturedEntity) ? TEXT("Valid") : TEXT("Invalid")); }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_IsOwningPlayerValid(CapturedEntity) ? ECk_Tone::Ok : ECk_Tone::Neutral; }));

    if (ck::Is_NOT_Valid(CapturedWidget))
    { return Builder.Build(Entity); }

    // ---- Controls ----
    //
    // Everything below is CosmeticOnly: a world-space widget is client presentation and never exists on
    // a dedicated server, so the controls grey out there with the reason in their tooltip instead of
    // enqueueing requests nobody will ever see. Public Utils only, typed handle captured BY VALUE and
    // re-validated on fire.
    //
    // Each scalar row is a read-modify-write of the WHOLE info struct, because the Utils surface only
    // takes complete structs. The read comes from the live Params fragment; the info-struct requests are
    // QUEUED, so committing two fields inside a single frame would read the first back stale. Commit is
    // on enter / lost focus, which makes that window a deliberate two-edits-in-one-frame, not a hazard
    // the normal flow hits. Request_SetEnabled below is an immediate mutator and has no such window.

    Builder.AddToggleRow(
        FText::FromString(TEXT("Enabled:")),
        TAttribute<bool>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_IsEnabled(CapturedEntity); }),
        [CapturedWidget](bool InIsEnabled)
        {
            auto Widget = CapturedWidget;
            if (ck::Is_NOT_Valid(Widget))
            { return; }

            UCk_Utils_WorldSpaceWidget_UE::Request_SetEnabled(Widget, InIsEnabled, {});
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    // ---- Scaling ----

    const auto Commit_ScalingInfo =
        [CapturedWidget](const FCk_WorldSpaceWidget_ScalingInfo& InInfo)
        {
            auto Widget = CapturedWidget;
            if (ck::Is_NOT_Valid(Widget))
            { return; }

            UCk_Utils_WorldSpaceWidget_UE::Request_SetScalingInfo(Widget, InInfo, {});
        };

    Builder.AddHeader(FText::FromString(TEXT("Scaling")));

    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Scaling Policy:")),
        {
            FText::FromString(TEXT("None")),
            FText::FromString(TEXT("ScaleWithDistance"))
        },
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        { return static_cast<int32>(ck_inspector_ui::Get_ScalingInfo(CapturedEntity).Get_ScalingPolicy()); }),
        [CapturedEntity, Commit_ScalingInfo](int32 InIndex)
        {
            Commit_ScalingInfo(ck_inspector_ui::Make_ScalingInfo(
                ck_inspector_ui::Get_ScalingInfo(CapturedEntity),
                static_cast<ECk_WorldSpaceWidget_Scaling_Policy>(InIndex)));
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Max Scale:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_ScalingInfo(CapturedEntity).Get_MaxScale(); }),
        [CapturedEntity, Commit_ScalingInfo](float InValue)
        {
            auto Info = ck_inspector_ui::Get_ScalingInfo(CapturedEntity);
            Commit_ScalingInfo(Info.Set_MaxScale(InValue));
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Min Scale:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_ScalingInfo(CapturedEntity).Get_MinScale(); }),
        [CapturedEntity, Commit_ScalingInfo](float InValue)
        {
            auto Info = ck_inspector_ui::Get_ScalingInfo(CapturedEntity);
            Commit_ScalingInfo(Info.Set_MinScale(InValue));
        },
        TOptional<float>{0.0f},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Scale Falloff Start:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_ScalingInfo(CapturedEntity).Get_ScaleFalloff_StartDistance(); }),
        [CapturedEntity, Commit_ScalingInfo](float InValue)
        {
            auto Info = ck_inspector_ui::Get_ScalingInfo(CapturedEntity);
            Commit_ScalingInfo(Info.Set_ScaleFalloff_StartDistance(InValue));
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Scale Falloff End:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_ScalingInfo(CapturedEntity).Get_ScaleFalloff_EndDistance(); }),
        [CapturedEntity, Commit_ScalingInfo](float InValue)
        {
            auto Info = ck_inspector_ui::Get_ScalingInfo(CapturedEntity);
            Commit_ScalingInfo(Info.Set_ScaleFalloff_EndDistance(InValue));
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    // ---- Fading ----

    const auto Commit_FadingInfo =
        [CapturedWidget](const FCk_WorldSpaceWidget_FadingInfo& InInfo)
        {
            auto Widget = CapturedWidget;
            if (ck::Is_NOT_Valid(Widget))
            { return; }

            UCk_Utils_WorldSpaceWidget_UE::Request_SetFadingInfo(Widget, InInfo, {});
        };

    Builder.AddHeader(FText::FromString(TEXT("Fading")));

    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Fading Policy:")),
        {
            FText::FromString(TEXT("None")),
            FText::FromString(TEXT("FadeWithDistance"))
        },
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        { return static_cast<int32>(ck_inspector_ui::Get_FadingInfo(CapturedEntity).Get_FadingPolicy()); }),
        [CapturedEntity, Commit_FadingInfo](int32 InIndex)
        {
            Commit_FadingInfo(ck_inspector_ui::Make_FadingInfo(
                ck_inspector_ui::Get_FadingInfo(CapturedEntity),
                static_cast<ECk_WorldSpaceWidget_Fading_Policy>(InIndex)));
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Max Opacity:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_FadingInfo(CapturedEntity).Get_MaxOpacity(); }),
        [CapturedEntity, Commit_FadingInfo](float InValue)
        {
            auto Info = ck_inspector_ui::Get_FadingInfo(CapturedEntity);
            Commit_FadingInfo(Info.Set_MaxOpacity(InValue));
        },
        TOptional<float>{0.0f},
        TOptional<float>{1.0f},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Min Opacity:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_FadingInfo(CapturedEntity).Get_MinOpacity(); }),
        [CapturedEntity, Commit_FadingInfo](float InValue)
        {
            auto Info = ck_inspector_ui::Get_FadingInfo(CapturedEntity);
            Commit_FadingInfo(Info.Set_MinOpacity(InValue));
        },
        TOptional<float>{0.0f},
        TOptional<float>{1.0f},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Fade Falloff Start:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_FadingInfo(CapturedEntity).Get_FadeFalloff_StartDistance(); }),
        [CapturedEntity, Commit_FadingInfo](float InValue)
        {
            auto Info = ck_inspector_ui::Get_FadingInfo(CapturedEntity);
            Commit_FadingInfo(Info.Set_FadeFalloff_StartDistance(InValue));
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Fade Falloff End:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_ui::Get_FadingInfo(CapturedEntity).Get_FadeFalloff_EndDistance(); }),
        [CapturedEntity, Commit_FadingInfo](float InValue)
        {
            auto Info = ck_inspector_ui::Get_FadingInfo(CapturedEntity);
            Commit_FadingInfo(Info.Set_FadeFalloff_EndDistance(InValue));
        },
        TOptional<float>{},
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    // ---- Occlusion ----

    const auto Commit_OcclusionInfo =
        [CapturedWidget](const FCk_WorldSpaceWidget_OcclusionInfo& InInfo)
        {
            auto Widget = CapturedWidget;
            if (ck::Is_NOT_Valid(Widget))
            { return; }

            UCk_Utils_WorldSpaceWidget_UE::Request_SetOcclusionInfo(Widget, InInfo, {});
        };

    Builder.AddHeader(FText::FromString(TEXT("Occlusion")));

    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Occlusion Policy:")),
        {
            FText::FromString(TEXT("None")),
            FText::FromString(TEXT("HideWhenOccluded"))
        },
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        { return static_cast<int32>(ck_inspector_ui::Get_OcclusionInfo(CapturedEntity).Get_OcclusionPolicy()); }),
        [CapturedEntity, Commit_OcclusionInfo](int32 InIndex)
        {
            const auto Info = ck_inspector_ui::Get_OcclusionInfo(CapturedEntity);
            Commit_OcclusionInfo(ck_inspector_ui::Make_OcclusionInfo(
                static_cast<ECk_WorldSpaceWidget_Occlusion_Policy>(InIndex),
                Info.Get_TraceChannel()));
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Trace Channel:")),
        ck_inspector_ui::Get_TraceChannelOptions(),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        {
            const auto Channel = ck_inspector_ui::Get_OcclusionInfo(CapturedEntity).Get_TraceChannel().GetValue();
            return ck_inspector_ui::Get_TraceChannels().IndexOfByKey(Channel);
        }),
        [CapturedEntity, Commit_OcclusionInfo](int32 InIndex)
        {
            const auto& Channels = ck_inspector_ui::Get_TraceChannels();
            if (NOT Channels.IsValidIndex(InIndex))
            { return; }

            const auto Info = ck_inspector_ui::Get_OcclusionInfo(CapturedEntity);
            Commit_OcclusionInfo(ck_inspector_ui::Make_OcclusionInfo(
                Info.Get_OcclusionPolicy(),
                Channels[InIndex]));
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_UI::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
