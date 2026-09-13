#include "CkInspector_Vfx.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Time/CkTime_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkVfx/Cue/CkVfxCue_Fragment.h"
#include "CkVfx/Cue/CkVfxCue_Utils.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Vfx)

namespace ck_inspector_vfx
{
    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

    auto TryGetCue(const FCk_Handle& InEntity, FCk_Handle_VfxCue& OutCue) -> bool
    {
        OutCue = {};
        if (IsDestroying(InEntity) || NOT InEntity.Has<ck::FFragment_VfxCue_Current>())
        { return false; }
        auto MutableEntity = InEntity;
        OutCue = UCk_Utils_VfxCue_UE::Cast(MutableEntity);
        return ck::IsValid(OutCue);
    }

    auto Get_ElapsedSeconds(const FCk_Handle& InEntity) -> float
    {
        auto Cue = FCk_Handle_VfxCue{};
        if (NOT TryGetCue(InEntity, Cue))
        { return 0.0f; }
        const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InEntity);
        if (ck::Is_NOT_Valid(World))
        { return 0.0f; }
        const auto TimeResult = UCk_Utils_Time_UE::Get_WorldTime(FCk_Utils_Time_GetWorldTime_Params{World});
        const auto StartTime = Cue.Get<ck::FFragment_VfxCue_Current>().Get_EffectStartTime();
        return FMath::Max(0.0f,
            static_cast<float>((TimeResult.Get_WorldTime().Get_Time() - StartTime).Get_Seconds()));
    }

    auto Get_DurationSeconds(const FCk_Handle& InEntity) -> float
    {
        auto Cue = FCk_Handle_VfxCue{};
        return TryGetCue(InEntity, Cue)
            ? static_cast<float>(Cue.Get<ck::FFragment_VfxCue_Current>().Get_EffectDuration().Get_Seconds())
            : 0.0f;
    }

    auto Get_StateTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        auto Cue = FCk_Handle_VfxCue{};
        if (NOT TryGetCue(InEntity, Cue))
        { return ECk_Tone::Neutral; }
        if (Cue.Has<ck::FTag_VfxCue_IsPlaying>())
        { return ECk_Tone::Ok; }
        return Cue.Get<ck::FFragment_VfxCue_Current>().Get_HasFiredFinished()
            ? ECk_Tone::Info : ECk_Tone::Neutral;
    }

    auto Get_RequestGate(const FCk_Handle& InEntity) -> FCk_DebugRequest_GateVerdict
    {
        auto Cue = FCk_Handle_VfxCue{};
        if (NOT TryGetCue(InEntity, Cue))
        { return {false, FText::FromString(TEXT("VFX Cue is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::CosmeticOnly);
    }

    auto DiffColor(const bool bInDiffMarked) -> FLinearColor
    {
        return bInDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

auto SCkInspector_VfxAuthored::Construct(const FArguments& InArgs) -> void
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

SCkInspector_VfxAuthored::~SCkInspector_VfxAuthored()
{
    Release();
}

auto SCkInspector_VfxAuthored::Get_IsAvailable() const -> bool
{
    auto Cue = FCk_Handle_VfxCue{};
    return _Active && ck_inspector_vfx::TryGetCue(_Entity, Cue);
}

auto SCkInspector_VfxAuthored::Get_ComponentText() const -> FString
{
    auto Cue = FCk_Handle_VfxCue{};
    if (NOT _Active || NOT ck_inspector_vfx::TryGetCue(_Entity, Cue))
    { return TEXT("--"); }
    const auto* const Component = Cue.Get<ck::FFragment_VfxCue_Current>().Get_NiagaraComponent().Get();
    return ck::IsValid(Component, ck::IsValid_Policy_NullptrOnly{}) ? TEXT("Valid") : TEXT("None");
}

auto SCkInspector_VfxAuthored::Get_ComponentColor() const -> FLinearColor
{
    auto Cue = FCk_Handle_VfxCue{};
    if (NOT _Active || NOT ck_inspector_vfx::TryGetCue(_Entity, Cue))
    { return CkStyle::None(); }
    const auto* const Component = Cue.Get<ck::FFragment_VfxCue_Current>().Get_NiagaraComponent().Get();
    return ck::IsValid(Component, ck::IsValid_Policy_NullptrOnly{})
        ? CkStyle::Value_Bool_True() : CkStyle::Value_Bool_False();
}

auto SCkInspector_VfxAuthored::Get_StartTimeText() const -> FString
{
    auto Cue = FCk_Handle_VfxCue{};
    return _Active && ck_inspector_vfx::TryGetCue(_Entity, Cue)
        ? ck::Format_UE(TEXT("{}"), Cue.Get<ck::FFragment_VfxCue_Current>().Get_EffectStartTime())
        : TEXT("--");
}

auto SCkInspector_VfxAuthored::Get_HasFiniteDuration() const -> bool
{
    return _Active && Get_IsAvailable() && ck_inspector_vfx::Get_DurationSeconds(_Entity) > 0.0f;
}

auto SCkInspector_VfxAuthored::Get_ElapsedFraction() const -> float
{
    const float Duration = ck_inspector_vfx::Get_DurationSeconds(_Entity);
    return _Active && Duration > 0.0f ? ck_inspector_vfx::Get_ElapsedSeconds(_Entity) / Duration : 0.0f;
}

auto SCkInspector_VfxAuthored::Get_ElapsedDurationText() const -> FString
{
    return Get_HasFiniteDuration()
        ? ck::Format_UE(TEXT("{:.2f} / {:.2f}s"),
            ck_inspector_vfx::Get_ElapsedSeconds(_Entity), ck_inspector_vfx::Get_DurationSeconds(_Entity))
        : TEXT("--");
}

auto SCkInspector_VfxAuthored::Get_DurationText() const -> FString
{
    auto Cue = FCk_Handle_VfxCue{};
    if (NOT _Active || NOT ck_inspector_vfx::TryGetCue(_Entity, Cue))
    { return TEXT("--"); }
    const auto Duration = Cue.Get<ck::FFragment_VfxCue_Current>().Get_EffectDuration();
    return Duration.Get_Seconds() < 0.0 ? TEXT("Infinite") : ck::Format_UE(TEXT("{}"), Duration);
}

auto SCkInspector_VfxAuthored::Get_StateText() const -> FString
{
    auto Cue = FCk_Handle_VfxCue{};
    if (NOT _Active || NOT ck_inspector_vfx::TryGetCue(_Entity, Cue))
    { return TEXT("--"); }
    if (Cue.Has<ck::FTag_VfxCue_IsPlaying>())
    { return TEXT("Playing"); }
    return Cue.Get<ck::FFragment_VfxCue_Current>().Get_HasFiredFinished() ? TEXT("Finished") : TEXT("Idle");
}

auto SCkInspector_VfxAuthored::Get_StateForeground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneColor(ck_inspector_vfx::Get_StateTone(_Entity)) : FLinearColor::Transparent;
}

auto SCkInspector_VfxAuthored::Get_StateBackground() const -> FLinearColor
{
    return _Active ? CkStyle::GetToneDimColor(ck_inspector_vfx::Get_StateTone(_Entity)) : FLinearColor::Transparent;
}

auto SCkInspector_VfxAuthored::Get_CanRequest() const -> bool
{
    return _Active && ck_inspector_vfx::Get_RequestGate(_Entity).IsEnabled;
}

auto SCkInspector_VfxAuthored::Get_RequestDisabledReason() const -> FString
{
    return _Active ? ck_inspector_vfx::Get_RequestGate(_Entity).Reason.ToString() : FString{};
}

auto SCkInspector_VfxAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_VfxAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& InName,
        FString (SCkInspector_VfxAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([Weak, InGetter]()
        {
            const auto Widget = Weak.Pin();
            return FText::FromString(Widget.IsValid() ? (Widget.Get()->*InGetter)() : FString{});
        }));
    };
    BindText(TEXT("vfx-component"), &SCkInspector_VfxAuthored::Get_ComponentText);
    BindText(TEXT("vfx-start-time"), &SCkInspector_VfxAuthored::Get_StartTimeText);
    BindText(TEXT("vfx-elapsed-duration"), &SCkInspector_VfxAuthored::Get_ElapsedDurationText);
    BindText(TEXT("vfx-duration"), &SCkInspector_VfxAuthored::Get_DurationText);
    BindText(TEXT("vfx-state"), &SCkInspector_VfxAuthored::Get_StateText);
    BindText(TEXT("vfx-disabled-reason"), &SCkInspector_VfxAuthored::Get_RequestDisabledReason);
    Data.Text.Add(TEXT("vfx-play-label"), FText::FromString(TEXT("Play")));
    Data.Text.Add(TEXT("vfx-stop-label"), FText::FromString(TEXT("Stop")));
    Data.Text.Add(TEXT("vfx-play-tooltip"),
        FText::FromString(TEXT("Request_Play — (re)activates the cue's Niagara component")));
    Data.Text.Add(TEXT("vfx-stop-tooltip"), FText::FromString(TEXT("Request_Stop — deactivates the cue")));
    Data.Visibility.Add(TEXT("vfx-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("vfx-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("vfx-finite-duration"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_HasFiniteDuration(); }));
    Data.Visibility.Add(TEXT("vfx-nonfinite-duration"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable() && NOT Widget->Get_HasFiniteDuration(); }));
    Data.Visibility.Add(TEXT("vfx-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Data.Number.Add(TEXT("vfx-elapsed-fraction"), TAttribute<float>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_ElapsedFraction() : 0.0f; }));
    Data.Color.Add(TEXT("vfx-component-color"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_ComponentColor() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("vfx-state-foreground"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_StateForeground() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("vfx-state-background"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_StateBackground() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("vfx-meter-fill"), CkStyle::Accent());
    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("component"), TEXT("Component:")}, {TEXT("start-time"), TEXT("Start Time:")},
        {TEXT("elapsed-duration"), TEXT("Elapsed / Duration:")}, {TEXT("duration"), TEXT("Duration:")},
        {TEXT("state"), TEXT("State:")}, {TEXT("playback"), TEXT("Playback:")}})
    {
        Data.Color.Add(TEXT("vfx-") + Pair.Key + TEXT("-diff-color"),
            TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]()
            {
                const auto Widget = Weak.Pin();
                return Widget.IsValid()
                    ? ck_inspector_vfx::DiffColor(Widget->Is_DiffMarked(Label)) : FLinearColor::Transparent;
            }));
    }
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); });

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("vfx-play"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Play(); } }));
    Actions.Add(TEXT("vfx-stop"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Stop(); } }));
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorVfx.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorVfx.ui.css")));
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

auto SCkInspector_VfxAuthored::Request_Play() -> void
{
    auto Cue = FCk_Handle_VfxCue{};
    if (_Active && ck_inspector_vfx::TryGetCue(_Entity, Cue)
        && ck_inspector_vfx::Get_RequestGate(_Entity).IsEnabled)
    { UCk_Utils_VfxCue_UE::Request_Play(Cue, FCk_Request_VfxCue_Play{}, {}); }
}

auto SCkInspector_VfxAuthored::Request_Stop() -> void
{
    auto Cue = FCk_Handle_VfxCue{};
    if (_Active && ck_inspector_vfx::TryGetCue(_Entity, Cue)
        && ck_inspector_vfx::Get_RequestGate(_Entity).IsEnabled)
    { UCk_Utils_VfxCue_UE::Request_Stop(Cue, {}); }
}

auto SCkInspector_VfxAuthored::Tick(
    const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid())
    { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else
    { _LoadError.Reset(); }
}

auto SCkInspector_VfxAuthored::Release() -> void
{
    if (NOT _Active)
    { return; }
    _Active = false;
    _Entity = {};
    _DiffLabels.Reset();
    _View.Reset();
    _Mounted = false;
}

FCkInspector_Vfx::~FCkInspector_Vfx()
{
    OnDeactivated();
}

auto FCkInspector_Vfx::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("VFX"));
}

auto FCkInspector_Vfx::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Cue = FCk_Handle_VfxCue{};
    return ck_inspector_vfx::TryGetCue(Entity, Cue);
}

auto FCkInspector_Vfx::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return NativeBody; }
    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : TArray<FString>{
        TEXT("Component:"), TEXT("Start Time:"), TEXT("Elapsed / Duration:"),
        TEXT("Duration:"), TEXT("State:"), TEXT("Playback:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }
    const TSharedRef<SCkInspector_VfxAuthored> Authored =
        SNew(SCkInspector_VfxAuthored).Entity(Entity).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Vfx::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    Builder.SetEditGuard(Get_EditGuard());
    auto Cue = FCk_Handle_VfxCue{};
    if (NOT ck_inspector_vfx::TryGetCue(Entity, Cue))
    { return Builder.Build(Entity, FString{}); }
    Builder.AddHeader(FText::FromString(TEXT("VFX Cue")));
    const auto CapturedEntity = Entity;
    Builder.AddConditionalRow(FText::FromString(TEXT("Component:")),
        [CapturedEntity](const FCk_Handle&)
        {
            auto CurrentCue = FCk_Handle_VfxCue{};
            if (NOT ck_inspector_vfx::TryGetCue(CapturedEntity, CurrentCue))
            { return FText::FromString(TEXT("--")); }
            const auto* Component = CurrentCue.Get<ck::FFragment_VfxCue_Current>().Get_NiagaraComponent().Get();
            return FText::FromString(ck::IsValid(Component, ck::IsValid_Policy_NullptrOnly{})
                ? TEXT("Valid") : TEXT("None"));
        },
        [CapturedEntity](const FCk_Handle&) -> FLinearColor
        {
            auto CurrentCue = FCk_Handle_VfxCue{};
            if (NOT ck_inspector_vfx::TryGetCue(CapturedEntity, CurrentCue))
            { return CkStyle::None(); }
            const auto* Component = CurrentCue.Get<ck::FFragment_VfxCue_Current>().Get_NiagaraComponent().Get();
            return ck::IsValid(Component, ck::IsValid_Policy_NullptrOnly{})
                ? CkStyle::Value_Bool_True() : CkStyle::Value_Bool_False();
        });
    Builder.AddRow(FText::FromString(TEXT("Start Time:")),
        [CapturedEntity](const FCk_Handle&)
        {
            auto CurrentCue = FCk_Handle_VfxCue{};
            return FText::FromString(ck_inspector_vfx::TryGetCue(CapturedEntity, CurrentCue)
                ? ck::Format_UE(TEXT("{}"), CurrentCue.Get<ck::FFragment_VfxCue_Current>().Get_EffectStartTime())
                : TEXT("--"));
        }, CkStyle::Value_Numeric());
    if (ck_inspector_vfx::Get_DurationSeconds(Entity) > 0.0f)
    {
        Builder.AddMeterRow(FText::FromString(TEXT("Elapsed / Duration:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            {
                const float Duration = ck_inspector_vfx::Get_DurationSeconds(CapturedEntity);
                return Duration > 0.0f ? ck_inspector_vfx::Get_ElapsedSeconds(CapturedEntity) / Duration : 0.0f;
            }), ECk_Tone::Accent, TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                auto CurrentCue = FCk_Handle_VfxCue{};
                return FText::FromString(ck_inspector_vfx::TryGetCue(CapturedEntity, CurrentCue)
                    ? ck::Format_UE(TEXT("{:.2f} / {:.2f}s"), ck_inspector_vfx::Get_ElapsedSeconds(CapturedEntity),
                        ck_inspector_vfx::Get_DurationSeconds(CapturedEntity))
                    : TEXT("--"));
            }));
    }
    else
    {
        Builder.AddRow(FText::FromString(TEXT("Duration:")),
            [CapturedEntity](const FCk_Handle&)
            {
                auto CurrentCue = FCk_Handle_VfxCue{};
                if (NOT ck_inspector_vfx::TryGetCue(CapturedEntity, CurrentCue))
                { return FText::FromString(TEXT("--")); }
                const auto Duration = CurrentCue.Get<ck::FFragment_VfxCue_Current>().Get_EffectDuration();
                return FText::FromString(Duration.Get_Seconds() < 0.0
                    ? FString{TEXT("Infinite")} : ck::Format_UE(TEXT("{}"), Duration));
            }, CkStyle::Value_Numeric());
    }
    Builder.AddStatusPillRow(FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            auto CurrentCue = FCk_Handle_VfxCue{};
            if (NOT ck_inspector_vfx::TryGetCue(CapturedEntity, CurrentCue))
            { return FText::FromString(TEXT("--")); }
            if (CurrentCue.Has<ck::FTag_VfxCue_IsPlaying>())
            { return FText::FromString(TEXT("Playing")); }
            return FText::FromString(CurrentCue.Get<ck::FFragment_VfxCue_Current>().Get_HasFiredFinished()
                ? TEXT("Finished") : TEXT("Idle"));
        }), TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        { return ck_inspector_vfx::Get_StateTone(CapturedEntity); }));
    Builder.AddActionRow(FText::FromString(TEXT("Playback:")), {
        FCkInspector_Action{FText::FromString(TEXT("Play")),
            FText::FromString(TEXT("Request_Play — (re)activates the cue's Niagara component")),
            [CapturedEntity]()
            {
                auto CurrentCue = FCk_Handle_VfxCue{};
                if (ck_inspector_vfx::TryGetCue(CapturedEntity, CurrentCue))
                { UCk_Utils_VfxCue_UE::Request_Play(CurrentCue, FCk_Request_VfxCue_Play{}, {}); }
            }, ECk_DebugRequest_Requirement::CosmeticOnly},
        FCkInspector_Action{FText::FromString(TEXT("Stop")),
            FText::FromString(TEXT("Request_Stop — deactivates the cue")),
            [CapturedEntity]()
            {
                auto CurrentCue = FCk_Handle_VfxCue{};
                if (ck_inspector_vfx::TryGetCue(CapturedEntity, CurrentCue))
                { UCk_Utils_VfxCue_UE::Request_Stop(CurrentCue, {}); }
            }, ECk_DebugRequest_Requirement::CosmeticOnly}});
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_Vfx::Tick(const FCk_Handle&, float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_VfxAuthored>& Weak)
    { const auto Instance = Weak.Pin(); return NOT Instance.IsValid() || Instance->Is_Inert(); });
    _LastAuthoredLoadError.Reset();
    for (const auto& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        { _LastAuthoredLoadError = Instance->Get_LoadError(); break; }
    }
}

auto FCkInspector_Vfx::OnDeactivated() -> void
{
    for (const auto& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
    _LastAuthoredLoadError.Reset();
}
