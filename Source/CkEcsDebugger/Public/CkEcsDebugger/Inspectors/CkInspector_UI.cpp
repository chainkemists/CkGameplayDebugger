#include "CkInspector_UI.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkWorldSpaceWidget/CkWorldSpaceWidget_Fragment.h"
#include "CkWorldSpaceWidget/CkWorldSpaceWidget_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

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
        return ck::IsValid(WrapperPtr);
    }

    static auto Get_IsOwningPlayerValid(
        const FCk_Handle& InEntity)
        -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Current>())
        { return false; }

        const auto* ResolvedPlayer = InEntity.Get<ck::FFragment_WorldSpaceWidget_Current>().Get_ResolvedOwningPlayer();
        return ck::IsValid(ResolvedPlayer);
    }

    static auto Get_IsEnabled(
        const FCk_Handle& InEntity)
        -> bool
    {
        const auto Widget = UCk_Utils_WorldSpaceWidget_UE::Cast(InEntity);

        if (ck::Is_NOT_Valid(Widget))
        { return false; }

        return UCk_Utils_WorldSpaceWidget_UE::Get_EnableDisable(Widget) == ECk_EnableDisable::Enable;
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

    static auto Is_Destroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<
                ck::FTag_DestroyEntity_Initiate,
                ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown,
                ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

    static auto Try_GetWidget(
        const FCk_Handle& InEntity,
        FCk_Handle_WorldSpaceWidget& OutWidget) -> bool
    {
        OutWidget = {};
        if (Is_Destroying(InEntity)
            || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Current>()
            || NOT InEntity.Has<ck::FFragment_WorldSpaceWidget_Params>())
        { return false; }
        auto Mutable = InEntity;
        OutWidget = UCk_Utils_WorldSpaceWidget_UE::Cast(Mutable);
        return ck::IsValid(OutWidget);
    }

    static auto TextField(const FString& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    static auto MakeOptions(const TArray<TPair<FString, FString>>& InItems) -> TSharedPtr<FCkUiCollection>
    {
        TSharedPtr<FCkUiCollection> Options;
        const FCkUiLoadResult Schema = FCkUiCollection::TryCreate(
            {{TEXT("label"), ECkUiFieldKind::Text}}, Options);
        if (NOT Schema.Succeeded || NOT Options.IsValid()) { return nullptr; }
        auto Records = TArray<FCkUiRecordData>{};
        Records.Reserve(InItems.Num());
        for (const auto& [Key, Label] : InItems)
        {
            FCkUiRecordData Record;
            Record.Key = Key;
            Record.Fields.Add(TEXT("label"), TextField(Label));
            Records.Add(MoveTemp(Record));
        }
        return Options->TrySetRecords(MoveTemp(Records)).Succeeded ? Options : nullptr;
    }

    static auto ScalingPolicyKey(const ECk_WorldSpaceWidget_Scaling_Policy InPolicy) -> FString
    { return InPolicy == ECk_WorldSpaceWidget_Scaling_Policy::ScaleWithDistance ? TEXT("distance") : TEXT("none"); }

    static auto FadingPolicyKey(const ECk_WorldSpaceWidget_Fading_Policy InPolicy) -> FString
    { return InPolicy == ECk_WorldSpaceWidget_Fading_Policy::FadeWithDistance ? TEXT("distance") : TEXT("none"); }

    static auto OcclusionPolicyKey(const ECk_WorldSpaceWidget_Occlusion_Policy InPolicy) -> FString
    { return InPolicy == ECk_WorldSpaceWidget_Occlusion_Policy::HideWhenOccluded ? TEXT("hide") : TEXT("none"); }
}

// =====================================================================================================================

auto FCkInspector_UI::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("UI"));
}

auto FCkInspector_UI::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck_inspector_ui::Is_Destroying(Entity))
    { return false; }

    return Entity.Has<ck::FFragment_WorldSpaceWidget_Current>();
}

// =====================================================================================================================

auto FCkInspector_UI::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
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

auto SCkInspector_UIAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_UIAuthored::~SCkInspector_UIAuthored()
{
    Release();
}

auto SCkInspector_UIAuthored::Get_IsAvailable() const -> bool
{
    return _Active && NOT ck_inspector_ui::Is_Destroying(_Entity)
        && _Entity.Has<ck::FFragment_WorldSpaceWidget_Current>();
}

auto SCkInspector_UIAuthored::Get_HasControls() const -> bool
{
    FCk_Handle_WorldSpaceWidget Widget;
    return _Active && ck_inspector_ui::Try_GetWidget(_Entity, Widget);
}

auto SCkInspector_UIAuthored::Get_CanEdit() const -> bool
{
    return Get_HasControls()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled;
}

auto SCkInspector_UIAuthored::Get_EditDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("UI state is unavailable."); }
    if (NOT Get_HasControls()) { return TEXT("World-space widget controls require Params and Current."); }
    return ck::DebugRequestGate::Evaluate(
        _Entity, ECk_DebugRequest_Requirement::CosmeticOnly).Reason.ToString();
}

auto SCkInspector_UIAuthored::Get_Text(const FString& InKey) const -> FString
{
    if (InKey == TEXT("wrapper"))
    { return ck_inspector_ui::Get_IsWrapperValid(_Entity) ? TEXT("Valid") : TEXT("Invalid"); }
    if (InKey == TEXT("owner"))
    { return ck_inspector_ui::Get_IsOwningPlayerValid(_Entity) ? TEXT("Valid") : TEXT("Invalid"); }
    if (NOT Get_HasControls()) { return {}; }
    if (InKey == TEXT("scaling-policy"))
    { return ck_inspector_ui::ScalingPolicyKey(ck_inspector_ui::Get_ScalingInfo(_Entity).Get_ScalingPolicy()); }
    if (InKey == TEXT("fading-policy"))
    { return ck_inspector_ui::FadingPolicyKey(ck_inspector_ui::Get_FadingInfo(_Entity).Get_FadingPolicy()); }
    if (InKey == TEXT("occlusion-policy"))
    { return ck_inspector_ui::OcclusionPolicyKey(ck_inspector_ui::Get_OcclusionInfo(_Entity).Get_OcclusionPolicy()); }
    if (InKey == TEXT("trace-channel"))
    { return FString::FromInt(static_cast<int32>(ck_inspector_ui::Get_OcclusionInfo(_Entity).Get_TraceChannel().GetValue())); }
    return {};
}

auto SCkInspector_UIAuthored::Get_Number(const FString& InKey) const -> float
{
    if (NOT Get_HasControls()) { return 0.0f; }
    const auto Scaling = ck_inspector_ui::Get_ScalingInfo(_Entity);
    const auto Fading = ck_inspector_ui::Get_FadingInfo(_Entity);
    if (InKey == TEXT("max-scale")) { return Scaling.Get_MaxScale(); }
    if (InKey == TEXT("min-scale")) { return Scaling.Get_MinScale(); }
    if (InKey == TEXT("scale-start")) { return Scaling.Get_ScaleFalloff_StartDistance(); }
    if (InKey == TEXT("scale-end")) { return Scaling.Get_ScaleFalloff_EndDistance(); }
    if (InKey == TEXT("max-opacity")) { return Fading.Get_MaxOpacity(); }
    if (InKey == TEXT("min-opacity")) { return Fading.Get_MinOpacity(); }
    if (InKey == TEXT("fade-start")) { return Fading.Get_FadeFalloff_StartDistance(); }
    if (InKey == TEXT("fade-end")) { return Fading.Get_FadeFalloff_EndDistance(); }
    return 0.0f;
}

auto SCkInspector_UIAuthored::Get_Bool(const FString& InKey) const -> bool
{
    return InKey == TEXT("enabled") && Get_HasControls() && ck_inspector_ui::Get_IsEnabled(_Entity);
}

auto SCkInspector_UIAuthored::Get_Color(const FString& InKey) const -> FLinearColor
{
    if (InKey == TEXT("wrapper-status"))
    { return ck_inspector_ui::Get_IsWrapperValid(_Entity) ? CkStyle::Ok() : CkStyle::Err(); }
    if (InKey == TEXT("owner-status"))
    { return ck_inspector_ui::Get_IsOwningPlayerValid(_Entity) ? CkStyle::Ok() : CkStyle::TextMute(); }
    return _DiffLabels.Contains(InKey) ? CkStyle::Accent() : CkStyle::Text();
}

auto SCkInspector_UIAuthored::Build_AuthoredView() -> bool
{
    _ScalingPolicies = ck_inspector_ui::MakeOptions({{TEXT("none"), TEXT("None")}, {TEXT("distance"), TEXT("ScaleWithDistance")}});
    _FadingPolicies = ck_inspector_ui::MakeOptions({{TEXT("none"), TEXT("None")}, {TEXT("distance"), TEXT("FadeWithDistance")}});
    _OcclusionPolicies = ck_inspector_ui::MakeOptions({{TEXT("none"), TEXT("None")}, {TEXT("hide"), TEXT("HideWhenOccluded")}});
    auto ChannelItems = TArray<TPair<FString, FString>>{};
    const auto& Channels = ck_inspector_ui::Get_TraceChannels();
    const auto& ChannelLabels = ck_inspector_ui::Get_TraceChannelOptions();
    for (int32 Index = 0; Index < Channels.Num() && Index < ChannelLabels.Num(); ++Index)
    {
        ChannelItems.Emplace(
            FString::FromInt(static_cast<int32>(Channels[Index])),
            ChannelLabels[Index].ToString());
    }
    _TraceChannels = ck_inspector_ui::MakeOptions(ChannelItems);

    auto Registry = TSharedPtr<const FCkUiWidgetRegistrySnapshot>{};
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid()
        || NOT _ScalingPolicies.IsValid() || NOT _FadingPolicies.IsValid()
        || NOT _OcclusionPolicies.IsValid() || NOT _TraceChannels.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        if (NOT _ScalingPolicies.IsValid() || NOT _FadingPolicies.IsValid()
            || NOT _OcclusionPolicies.IsValid() || NOT _TraceChannels.IsValid())
        { Errors.Add(TEXT("UI inspector options are unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_UIAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Visibility.Add(TEXT("ui-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("ui-unavailable"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("ui-controls-visible"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasControls(); }));
    Data.Visibility.Add(TEXT("ui-controls-missing"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable() && NOT Widget->Get_HasControls(); }));
    Data.Visibility.Add(TEXT("ui-can-edit"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanEdit(); }));
    Data.Visibility.Add(TEXT("ui-read-only"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_CanEdit(); }));
    Data.Text.Add(TEXT("ui-disabled"), TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return FText::FromString(Widget.IsValid() ? Widget->Get_EditDisabledReason() : FString{}); }));
    for (const FString Key : {FString{TEXT("wrapper")}, FString{TEXT("owner")}})
    {
        Data.Text.Add(TEXT("ui-") + Key, TAttribute<FText>::CreateLambda([WeakWidget, Key]()
        { const auto Widget = WeakWidget.Pin(); return FText::FromString(Widget.IsValid() ? Widget->Get_Text(Key) : FString{}); }));
    }
    for (const FString Key : {FString{TEXT("scaling-policy")}, FString{TEXT("fading-policy")},
        FString{TEXT("occlusion-policy")}, FString{TEXT("trace-channel")}})
    {
        Data.String.Add(TEXT("ui-") + Key, TAttribute<FString>::CreateLambda([WeakWidget, Key]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Text(Key) : FString{}; }));
    }
    for (const FString Key : {FString{TEXT("max-scale")}, FString{TEXT("min-scale")}, FString{TEXT("scale-start")},
        FString{TEXT("scale-end")}, FString{TEXT("max-opacity")}, FString{TEXT("min-opacity")},
        FString{TEXT("fade-start")}, FString{TEXT("fade-end")}})
    {
        Data.Number.Add(TEXT("ui-") + Key, TAttribute<float>::CreateLambda([WeakWidget, Key]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Number(Key) : 0.0f; }));
        Data.NumberCommitted.Add(TEXT("ui-") + Key + TEXT("-committed"), FCkUiOnNumberCommitted::CreateLambda(
            [WeakWidget, Key](const float Value, ETextCommit::Type)
            { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Number(Key, Value); } }));
    }
    Data.Color.Add(TEXT("ui-wrapper-status-color"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Color(TEXT("wrapper-status")) : CkStyle::TextMute(); }));
    Data.Color.Add(TEXT("ui-owner-status-color"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Color(TEXT("owner-status")) : CkStyle::TextMute(); }));
    for (const TPair<FString, FString>& Entry : TArray<TPair<FString, FString>>{
        {TEXT("enabled"), TEXT("Enabled:")}, {TEXT("scaling-policy"), TEXT("Scaling Policy:")},
        {TEXT("max-scale"), TEXT("Max Scale:")}, {TEXT("min-scale"), TEXT("Min Scale:")},
        {TEXT("scale-start"), TEXT("Scale Falloff Start:")}, {TEXT("scale-end"), TEXT("Scale Falloff End:")},
        {TEXT("fading-policy"), TEXT("Fading Policy:")}, {TEXT("max-opacity"), TEXT("Max Opacity:")},
        {TEXT("min-opacity"), TEXT("Min Opacity:")}, {TEXT("fade-start"), TEXT("Fade Falloff Start:")},
        {TEXT("fade-end"), TEXT("Fade Falloff End:")}, {TEXT("occlusion-policy"), TEXT("Occlusion Policy:")},
        {TEXT("trace-channel"), TEXT("Trace Channel:")}})
    {
        Data.Color.Add(TEXT("ui-diff-") + Entry.Key, TAttribute<FLinearColor>::CreateLambda([WeakWidget, Label = Entry.Value]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Color(Label) : CkStyle::Text(); }));
    }
    Data.BoolChanged.Add(TEXT("ui-enabled-changed"), FCkUiOnBoolChanged::CreateLambda([WeakWidget](const bool Value)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Change_Enabled(Value); } }));
    Data.Visibility.Add(TEXT("ui-enabled"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_Bool(TEXT("enabled")); }));
    Data.StringChanged.Add(TEXT("ui-scaling-policy-changed"), FCkUiOnStringChanged::CreateLambda([WeakWidget](const FString& Value)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Change_Policy(TEXT("scaling"), Value); } }));
    Data.StringChanged.Add(TEXT("ui-fading-policy-changed"), FCkUiOnStringChanged::CreateLambda([WeakWidget](const FString& Value)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Change_Policy(TEXT("fading"), Value); } }));
    Data.StringChanged.Add(TEXT("ui-occlusion-policy-changed"), FCkUiOnStringChanged::CreateLambda([WeakWidget](const FString& Value)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Change_Policy(TEXT("occlusion"), Value); } }));
    Data.StringChanged.Add(TEXT("ui-trace-channel-changed"), FCkUiOnStringChanged::CreateLambda([WeakWidget](const FString& Value)
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Change_TraceChannel(Value); } }));
    Data.Collections.Add(TEXT("ui-scaling-options"), _ScalingPolicies);
    Data.Collections.Add(TEXT("ui-fading-options"), _FadingPolicies);
    Data.Collections.Add(TEXT("ui-occlusion-options"), _OcclusionPolicies);
    Data.Collections.Add(TEXT("ui-trace-options"), _TraceChannels);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(Root, TEXT("EcsInspectorUI.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorUI.ui.css")));
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

auto SCkInspector_UIAuthored::Change_Enabled(const bool bInEnabled) -> void
{
    FCk_Handle_WorldSpaceWidget Widget;
    if (NOT ck_inspector_ui::Try_GetWidget(_Entity, Widget) || NOT Get_CanEdit()) { return; }
    UCk_Utils_WorldSpaceWidget_UE::Request_SetEnabled(Widget, bInEnabled, {});
}

auto SCkInspector_UIAuthored::Change_Policy(const FString& InKey, const FString& InValue) -> void
{
    FCk_Handle_WorldSpaceWidget Widget;
    if (NOT ck_inspector_ui::Try_GetWidget(_Entity, Widget) || NOT Get_CanEdit()) { return; }
    if (InKey == TEXT("scaling") && (InValue == TEXT("none") || InValue == TEXT("distance")))
    {
        UCk_Utils_WorldSpaceWidget_UE::Request_SetScalingInfo(Widget,
            ck_inspector_ui::Make_ScalingInfo(ck_inspector_ui::Get_ScalingInfo(_Entity),
                InValue == TEXT("distance") ? ECk_WorldSpaceWidget_Scaling_Policy::ScaleWithDistance
                                            : ECk_WorldSpaceWidget_Scaling_Policy::None), {});
    }
    else if (InKey == TEXT("fading") && (InValue == TEXT("none") || InValue == TEXT("distance")))
    {
        UCk_Utils_WorldSpaceWidget_UE::Request_SetFadingInfo(Widget,
            ck_inspector_ui::Make_FadingInfo(ck_inspector_ui::Get_FadingInfo(_Entity),
                InValue == TEXT("distance") ? ECk_WorldSpaceWidget_Fading_Policy::FadeWithDistance
                                            : ECk_WorldSpaceWidget_Fading_Policy::None), {});
    }
    else if (InKey == TEXT("occlusion") && (InValue == TEXT("none") || InValue == TEXT("hide")))
    {
        const auto Current = ck_inspector_ui::Get_OcclusionInfo(_Entity);
        UCk_Utils_WorldSpaceWidget_UE::Request_SetOcclusionInfo(Widget,
            ck_inspector_ui::Make_OcclusionInfo(
                InValue == TEXT("hide") ? ECk_WorldSpaceWidget_Occlusion_Policy::HideWhenOccluded
                                        : ECk_WorldSpaceWidget_Occlusion_Policy::None,
                Current.Get_TraceChannel()), {});
    }
}

auto SCkInspector_UIAuthored::Commit_Number(const FString& InKey, const float InValue) -> void
{
    FCk_Handle_WorldSpaceWidget Widget;
    if (NOT FMath::IsFinite(InValue) || NOT ck_inspector_ui::Try_GetWidget(_Entity, Widget) || NOT Get_CanEdit()) { return; }
    if (InKey == TEXT("max-scale") || InKey == TEXT("min-scale")
        || InKey == TEXT("scale-start") || InKey == TEXT("scale-end"))
    {
        auto Info = ck_inspector_ui::Get_ScalingInfo(_Entity);
        if (InKey == TEXT("max-scale")) { Info.Set_MaxScale(InValue); }
        else if (InKey == TEXT("min-scale")) { Info.Set_MinScale(FMath::Max(0.0f, InValue)); }
        else if (InKey == TEXT("scale-start")) { Info.Set_ScaleFalloff_StartDistance(InValue); }
        else { Info.Set_ScaleFalloff_EndDistance(InValue); }
        UCk_Utils_WorldSpaceWidget_UE::Request_SetScalingInfo(Widget, Info, {});
        return;
    }
    auto Info = ck_inspector_ui::Get_FadingInfo(_Entity);
    if (InKey == TEXT("max-opacity")) { Info.Set_MaxOpacity(FMath::Clamp(InValue, 0.0f, 1.0f)); }
    else if (InKey == TEXT("min-opacity")) { Info.Set_MinOpacity(FMath::Clamp(InValue, 0.0f, 1.0f)); }
    else if (InKey == TEXT("fade-start")) { Info.Set_FadeFalloff_StartDistance(InValue); }
    else if (InKey == TEXT("fade-end")) { Info.Set_FadeFalloff_EndDistance(InValue); }
    else { return; }
    UCk_Utils_WorldSpaceWidget_UE::Request_SetFadingInfo(Widget, Info, {});
}

auto SCkInspector_UIAuthored::Change_TraceChannel(const FString& InValue) -> void
{
    FCk_Handle_WorldSpaceWidget Widget;
    if (NOT ck_inspector_ui::Try_GetWidget(_Entity, Widget) || NOT Get_CanEdit()) { return; }
    const auto& Channels = ck_inspector_ui::Get_TraceChannels();
    const ECollisionChannel* Channel = Channels.FindByPredicate([&InValue](const ECollisionChannel InChannel)
    { return InValue == FString::FromInt(static_cast<int32>(InChannel)); });
    if (Channel == nullptr) { return; }
    const auto Current = ck_inspector_ui::Get_OcclusionInfo(_Entity);
    UCk_Utils_WorldSpaceWidget_UE::Request_SetOcclusionInfo(Widget,
        ck_inspector_ui::Make_OcclusionInfo(Current.Get_OcclusionPolicy(), *Channel), {});
}

auto SCkInspector_UIAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_UIAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _DiffLabels.Reset();
    _ScalingPolicies.Reset();
    _FadingPolicies.Reset();
    _OcclusionPolicies.Reset();
    _TraceChannels.Reset();
    _View.Reset();
    _Mounted = false;
}

auto FCkInspector_UI::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> Native = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return Native; }
    auto DiffLabels = TSet<FString>{};
    for (const TCHAR* Label : {
        TEXT("Wrapper Widget:"), TEXT("Owning Player:"), TEXT("Enabled:"), TEXT("Scaling Policy:"),
        TEXT("Max Scale:"), TEXT("Min Scale:"), TEXT("Scale Falloff Start:"), TEXT("Scale Falloff End:"),
        TEXT("Fading Policy:"), TEXT("Max Opacity:"), TEXT("Min Opacity:"), TEXT("Fade Falloff Start:"),
        TEXT("Fade Falloff End:"), TEXT("Occlusion Policy:"), TEXT("Trace Channel:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); }
    }
    const TSharedRef<SCkInspector_UIAuthored> Authored = SNew(SCkInspector_UIAuthored)
        .Entity(Entity)
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return Native;
    }
    _LastAuthoredLoadError.Reset();
    _Instances.Add(Authored);
    return Authored;
}

FCkInspector_UI::~FCkInspector_UI()
{
    OnDeactivated();
}

auto FCkInspector_UI::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _Instances.RemoveAll([](const TWeakPtr<SCkInspector_UIAuthored>& InInstance)
    {
        const auto Instance = InInstance.Pin();
        return NOT Instance.IsValid() || Instance->Is_Inert();
    });
}

auto FCkInspector_UI::OnDeactivated() -> void
{
    for (const auto& WeakInstance : _Instances)
    {
        if (const auto Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); }
    }
    _Instances.Reset();
}

// =====================================================================================================================
