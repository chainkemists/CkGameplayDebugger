#include "CkInspector_Jolt.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

// Jolt feature Utils expose the clean accessor surface; the JoltBody fragment (pulled transitively
// via the Utils header) is read directly for the raw JPH::BodyID, mirroring CkInspector_Physics's
// direct-fragment reads.
#include "CkJolt/Body/CkJoltBody_Utils.h"
#include "CkJolt/Body/CkJoltBody_Fragment.h"
#include "CkJolt/Character/CkJoltCharacter_Utils.h"
#include "CkJolt/StaticWorld/CkJoltStaticActor_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiFloatSeries.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Jolt)

// =====================================================================================================================

namespace ck_inspector_jolt
{
    enum : uint8
    {
        BodyBit = 1 << 0,
        CharacterBit = 1 << 1,
        StaticActorBit = 1 << 2,
    };

    static auto Get_StructureMask(const FCk_Handle& InEntity) -> uint8
    {
        if (ck::Is_NOT_Valid(InEntity)) { return 0; }
        uint8 Result = 0;
        if (UCk_Utils_JoltBody_UE::Has(InEntity)) { Result |= BodyBit; }
        if (UCk_Utils_JoltCharacter_UE::Has(InEntity)) { Result |= CharacterBit; }
        if (UCk_Utils_JoltStaticActor_UE::Has(InEntity)) { Result |= StaticActorBit; }
        return Result;
    }

    // Motion type is a simulation CLASS, not a health state: Static is the inert default, Kinematic
    // is externally driven, Dynamic is the one the solver actually integrates.
    static auto Get_MotionTone(ECk_MotionType InMotionType) -> ECk_Tone
    {
        switch (InMotionType)
        {
            case ECk_MotionType::Kinematic: return ECk_Tone::Info;
            case ECk_MotionType::Dynamic:   return ECk_Tone::Accent;
            default:                        return ECk_Tone::Neutral;
        }
    }

    static auto Get_SleepTone(ECk_Jolt_SleepState InSleepState) -> ECk_Tone
    {
        return InSleepState == ECk_Jolt_SleepState::Awake ? ECk_Tone::Ok : ECk_Tone::Neutral;
    }

    // NotSupported means the character will fall through what it is touching — an error, not a warning.
    static auto Get_GroundTone(ECk_JoltCharacter_GroundState InGroundState) -> ECk_Tone
    {
        switch (InGroundState)
        {
            case ECk_JoltCharacter_GroundState::OnGround:      return ECk_Tone::Ok;
            case ECk_JoltCharacter_GroundState::OnSteepSlope:  return ECk_Tone::Warn;
            case ECk_JoltCharacter_GroundState::InAir:         return ECk_Tone::Info;
            case ECk_JoltCharacter_GroundState::NotSupported:  return ECk_Tone::Err;
            default:                                           return ECk_Tone::Neutral;
        }
    }

    static auto Get_DiffColor(const bool bInMarked) -> FLinearColor
    {
        return bInMarked ? CkStyle::Accent() : CkStyle::Text();
    }

    // Three fixed-precision components in X/Y/Z order — same 3 decimals FVector::ToString printed —
    // so the row's axis coloring lines up with the Transform inspector.
    static auto Make_AxisComponents(
        TFunction<FVector()> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InProjector, Axis]()
            {
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), InProjector()[Axis]));
            }));
        }

        return Components;
    }
}

// =====================================================================================================================

auto SCkInspector_JoltAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView())
    {
        Push_SpeedSample();
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_JoltAuthored::~SCkInspector_JoltAuthored()
{
    Release();
}

auto SCkInspector_JoltAuthored::Get_IsAvailable() const -> bool
{
    return _Active && ck_inspector_jolt::Get_StructureMask(_Entity) != 0;
}

auto SCkInspector_JoltAuthored::Get_HasBody() const -> bool
{
    return _Active && ck::IsValid(_Entity) && UCk_Utils_JoltBody_UE::Has(_Entity);
}

auto SCkInspector_JoltAuthored::Get_HasCharacter() const -> bool
{
    return _Active && ck::IsValid(_Entity) && UCk_Utils_JoltCharacter_UE::Has(_Entity);
}

auto SCkInspector_JoltAuthored::Get_HasStaticActor() const -> bool
{
    return _Active && ck::IsValid(_Entity) && UCk_Utils_JoltStaticActor_UE::Has(_Entity);
}

auto SCkInspector_JoltAuthored::Get_BodyIdText() const -> FString
{
    if (NOT Get_HasBody() || NOT _Entity.Has<ck::FFragment_JoltBody_Current>()) { return TEXT("--"); }
    return ck::Format_UE(TEXT("{}"),
        _Entity.Get<ck::FFragment_JoltBody_Current>().Get_BodyId().GetIndexAndSequenceNumber());
}

auto SCkInspector_JoltAuthored::Get_MotionTypeText() const -> FString
{
    if (NOT Get_HasBody()) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    return ck::IsValid(Body) ? ck::Format_UE(TEXT("{}"), UCk_Utils_JoltBody_UE::Get_MotionType(Body)) : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_MotionTypeForeground() const -> FLinearColor
{
    if (NOT Get_HasBody()) { return CkStyle::GetToneColor(ECk_Tone::Neutral); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    const ECk_Tone Tone = ck::IsValid(Body)
        ? ck_inspector_jolt::Get_MotionTone(UCk_Utils_JoltBody_UE::Get_MotionType(Body)) : ECk_Tone::Neutral;
    return CkStyle::GetToneColor(Tone);
}

auto SCkInspector_JoltAuthored::Get_MotionTypeBackground() const -> FLinearColor
{
    if (NOT Get_HasBody()) { return CkStyle::GetToneDimColor(ECk_Tone::Neutral); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    const ECk_Tone Tone = ck::IsValid(Body)
        ? ck_inspector_jolt::Get_MotionTone(UCk_Utils_JoltBody_UE::Get_MotionType(Body)) : ECk_Tone::Neutral;
    return CkStyle::GetToneDimColor(Tone);
}

auto SCkInspector_JoltAuthored::Get_SleepStateText() const -> FString
{
    if (NOT Get_HasBody()) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    return ck::IsValid(Body) ? ck::Format_UE(TEXT("{}"), UCk_Utils_JoltBody_UE::Get_SleepState(Body)) : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_SleepStateForeground() const -> FLinearColor
{
    if (NOT Get_HasBody()) { return CkStyle::GetToneColor(ECk_Tone::Neutral); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    const ECk_Tone Tone = ck::IsValid(Body)
        ? ck_inspector_jolt::Get_SleepTone(UCk_Utils_JoltBody_UE::Get_SleepState(Body)) : ECk_Tone::Neutral;
    return CkStyle::GetToneColor(Tone);
}

auto SCkInspector_JoltAuthored::Get_SleepStateBackground() const -> FLinearColor
{
    if (NOT Get_HasBody()) { return CkStyle::GetToneDimColor(ECk_Tone::Neutral); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    const ECk_Tone Tone = ck::IsValid(Body)
        ? ck_inspector_jolt::Get_SleepTone(UCk_Utils_JoltBody_UE::Get_SleepState(Body)) : ECk_Tone::Neutral;
    return CkStyle::GetToneDimColor(Tone);
}

auto SCkInspector_JoltAuthored::Get_BodyAddedText() const -> FString
{
    if (NOT Get_HasBody()) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    return ck::IsValid(Body) && UCk_Utils_JoltBody_UE::Get_IsBodyAdded(Body) ? TEXT("Yes") : TEXT("No");
}

auto SCkInspector_JoltAuthored::Get_BodyAddedForeground() const -> FLinearColor
{
    const ECk_Tone Tone = Get_HasBody() && Get_BodyAddedText() == TEXT("Yes") ? ECk_Tone::Ok : ECk_Tone::Warn;
    return CkStyle::GetToneColor(Tone);
}

auto SCkInspector_JoltAuthored::Get_BodyAddedBackground() const -> FLinearColor
{
    const ECk_Tone Tone = Get_HasBody() && Get_BodyAddedText() == TEXT("Yes") ? ECk_Tone::Ok : ECk_Tone::Warn;
    return CkStyle::GetToneDimColor(Tone);
}

auto SCkInspector_JoltAuthored::Get_LinearVelocityAxisText(const int32 InAxis) const -> FString
{
    if (NOT Get_HasBody() || InAxis < 0 || InAxis > 2) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    return ck::IsValid(Body)
        ? ck::Format_UE(TEXT("{:.3f}"), UCk_Utils_JoltBody_UE::Get_LinearVelocity(Body)[InAxis]) : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_LinearSpeedText() const -> FString
{
    if (NOT Get_HasBody()) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
    return ck::IsValid(Body)
        ? ck::Format_UE(TEXT("{:.2f}"), UCk_Utils_JoltBody_UE::Get_LinearVelocity(Body).Size()) : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_GroundStateText() const -> FString
{
    if (NOT Get_HasCharacter()) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltCharacter Character = UCk_Utils_JoltCharacter_UE::Cast(Mutable);
    return ck::IsValid(Character)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_JoltCharacter_UE::Get_GroundState(Character)) : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_GroundStateForeground() const -> FLinearColor
{
    if (NOT Get_HasCharacter()) { return CkStyle::GetToneColor(ECk_Tone::Neutral); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltCharacter Character = UCk_Utils_JoltCharacter_UE::Cast(Mutable);
    const ECk_Tone Tone = ck::IsValid(Character)
        ? ck_inspector_jolt::Get_GroundTone(UCk_Utils_JoltCharacter_UE::Get_GroundState(Character)) : ECk_Tone::Neutral;
    return CkStyle::GetToneColor(Tone);
}

auto SCkInspector_JoltAuthored::Get_GroundStateBackground() const -> FLinearColor
{
    if (NOT Get_HasCharacter()) { return CkStyle::GetToneDimColor(ECk_Tone::Neutral); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltCharacter Character = UCk_Utils_JoltCharacter_UE::Cast(Mutable);
    const ECk_Tone Tone = ck::IsValid(Character)
        ? ck_inspector_jolt::Get_GroundTone(UCk_Utils_JoltCharacter_UE::Get_GroundState(Character)) : ECk_Tone::Neutral;
    return CkStyle::GetToneDimColor(Tone);
}

auto SCkInspector_JoltAuthored::Get_GroundNormalAxisText(const int32 InAxis) const -> FString
{
    if (NOT Get_HasCharacter() || InAxis < 0 || InAxis > 2) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltCharacter Character = UCk_Utils_JoltCharacter_UE::Cast(Mutable);
    return ck::IsValid(Character)
        ? ck::Format_UE(TEXT("{:.3f}"), UCk_Utils_JoltCharacter_UE::Get_GroundNormal(Character)[InAxis]) : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_GroundVelocityAxisText(const int32 InAxis) const -> FString
{
    if (NOT Get_HasCharacter() || InAxis < 0 || InAxis > 2) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltCharacter Character = UCk_Utils_JoltCharacter_UE::Cast(Mutable);
    return ck::IsValid(Character)
        ? ck::Format_UE(TEXT("{:.3f}"), UCk_Utils_JoltCharacter_UE::Get_GroundVelocity(Character)[InAxis]) : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_SourceActorText() const -> FString
{
    if (NOT Get_HasStaticActor()) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltStaticActor StaticActor = UCk_Utils_JoltStaticActor_UE::Cast(Mutable);
    return ck::IsValid(StaticActor) ? UCk_Utils_JoltStaticActor_UE::Get_SourceActorName(StaticActor).ToString() : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_NumBodiesText() const -> FString
{
    if (NOT Get_HasStaticActor()) { return TEXT("--"); }
    auto Mutable = _Entity;
    const FCk_Handle_JoltStaticActor StaticActor = UCk_Utils_JoltStaticActor_UE::Cast(Mutable);
    return ck::IsValid(StaticActor)
        ? ck::Format_UE(TEXT("{}"), UCk_Utils_JoltStaticActor_UE::Get_NumBodies(StaticActor)) : TEXT("--");
}

auto SCkInspector_JoltAuthored::Get_NumBodiesForeground() const -> FLinearColor
{
    return CkStyle::GetToneColor(ECk_Tone::Info);
}

auto SCkInspector_JoltAuthored::Get_NumBodiesBackground() const -> FLinearColor
{
    return CkStyle::GetToneDimColor(ECk_Tone::Info);
}

auto SCkInspector_JoltAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const FCkUiLoadResult SeriesResult = FCkUiFloatSeries::TryCreate({}, _SpeedSeries);
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid() || NOT SeriesResult.Succeeded)
    {
        auto Errors = RegistryResult.Errors;
        Errors.Append(SeriesResult.Errors);
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_JoltAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& InName,
        FString (SCkInspector_JoltAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([Weak, InGetter]()
        {
            const auto Widget = Weak.Pin();
            return FText::FromString(Widget.IsValid() ? (Widget.Get()->*InGetter)() : FString{});
        }));
    };
    BindText(TEXT("jolt-body-id"), &SCkInspector_JoltAuthored::Get_BodyIdText);
    BindText(TEXT("jolt-motion-type"), &SCkInspector_JoltAuthored::Get_MotionTypeText);
    BindText(TEXT("jolt-sleep-state"), &SCkInspector_JoltAuthored::Get_SleepStateText);
    BindText(TEXT("jolt-body-added"), &SCkInspector_JoltAuthored::Get_BodyAddedText);
    BindText(TEXT("jolt-linear-speed"), &SCkInspector_JoltAuthored::Get_LinearSpeedText);
    BindText(TEXT("jolt-ground-state"), &SCkInspector_JoltAuthored::Get_GroundStateText);
    BindText(TEXT("jolt-source-actor"), &SCkInspector_JoltAuthored::Get_SourceActorText);
    BindText(TEXT("jolt-num-bodies"), &SCkInspector_JoltAuthored::Get_NumBodiesText);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const TCHAR AxisName = TEXT("xyz")[Axis];
        Data.Text.Add(FString::Printf(TEXT("jolt-linear-velocity-%c"), AxisName),
            TAttribute<FText>::CreateLambda([Weak, Axis]()
            { const auto Widget = Weak.Pin(); return FText::FromString(Widget.IsValid()
                ? Widget->Get_LinearVelocityAxisText(Axis) : FString{}); }));
        Data.Text.Add(FString::Printf(TEXT("jolt-ground-normal-%c"), AxisName),
            TAttribute<FText>::CreateLambda([Weak, Axis]()
            { const auto Widget = Weak.Pin(); return FText::FromString(Widget.IsValid()
                ? Widget->Get_GroundNormalAxisText(Axis) : FString{}); }));
        Data.Text.Add(FString::Printf(TEXT("jolt-ground-velocity-%c"), AxisName),
            TAttribute<FText>::CreateLambda([Weak, Axis]()
            { const auto Widget = Weak.Pin(); return FText::FromString(Widget.IsValid()
                ? Widget->Get_GroundVelocityAxisText(Axis) : FString{}); }));
    }
    Data.Visibility.Add(TEXT("jolt-body-visible"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_HasBody(); }));
    Data.Visibility.Add(TEXT("jolt-character-visible"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_HasCharacter(); }));
    Data.Visibility.Add(TEXT("jolt-static-actor-visible"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_HasStaticActor(); }));
    Data.FloatSeries.Add(TEXT("jolt-linear-speed-series"), _SpeedSeries);

    const auto BindColor = [&Data, Weak](const FString& InName,
        FLinearColor (SCkInspector_JoltAuthored::* InGetter)() const)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([Weak, InGetter]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid() ? (Widget.Get()->*InGetter)() : FLinearColor::Transparent;
        }));
    };
    BindColor(TEXT("jolt-motion-type-foreground"), &SCkInspector_JoltAuthored::Get_MotionTypeForeground);
    BindColor(TEXT("jolt-motion-type-background"), &SCkInspector_JoltAuthored::Get_MotionTypeBackground);
    BindColor(TEXT("jolt-sleep-state-foreground"), &SCkInspector_JoltAuthored::Get_SleepStateForeground);
    BindColor(TEXT("jolt-sleep-state-background"), &SCkInspector_JoltAuthored::Get_SleepStateBackground);
    BindColor(TEXT("jolt-body-added-foreground"), &SCkInspector_JoltAuthored::Get_BodyAddedForeground);
    BindColor(TEXT("jolt-body-added-background"), &SCkInspector_JoltAuthored::Get_BodyAddedBackground);
    BindColor(TEXT("jolt-ground-state-foreground"), &SCkInspector_JoltAuthored::Get_GroundStateForeground);
    BindColor(TEXT("jolt-ground-state-background"), &SCkInspector_JoltAuthored::Get_GroundStateBackground);
    BindColor(TEXT("jolt-num-bodies-foreground"), &SCkInspector_JoltAuthored::Get_NumBodiesForeground);
    BindColor(TEXT("jolt-num-bodies-background"), &SCkInspector_JoltAuthored::Get_NumBodiesBackground);
    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("body-id"), TEXT("Body Id:")}, {TEXT("motion-type"), TEXT("Motion Type:")},
        {TEXT("sleep-state"), TEXT("Sleep State:")}, {TEXT("body-added"), TEXT("Body Added:")},
        {TEXT("linear-velocity"), TEXT("Linear Velocity:")}, {TEXT("linear-speed"), TEXT("Linear Speed:")},
        {TEXT("ground-state"), TEXT("Ground State:")}, {TEXT("ground-normal"), TEXT("Ground Normal:")},
        {TEXT("ground-velocity"), TEXT("Ground Velocity:")}, {TEXT("source-actor"), TEXT("Source Actor:")},
        {TEXT("num-bodies"), TEXT("Num Bodies:")}})
    {
        Data.Color.Add(TEXT("jolt-") + Pair.Key + TEXT("-diff-color"),
            TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]()
            {
                const auto Widget = Weak.Pin();
                return Widget.IsValid() && NOT Widget->Is_Inert()
                    ? ck_inspector_jolt::Get_DiffColor(Widget->Is_DiffMarked(Label)) : FLinearColor::Transparent;
            }));
    }
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorJolt.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorJolt.ui.css")));
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

auto SCkInspector_JoltAuthored::Push_SpeedSample() -> void
{
    constexpr int32 MaxSamples = 60;
    if (Get_HasBody())
    {
        auto Mutable = _Entity;
        const FCk_Handle_JoltBody Body = UCk_Utils_JoltBody_UE::Cast(Mutable);
        _SpeedSamples.Add(ck::IsValid(Body)
            ? static_cast<float>(UCk_Utils_JoltBody_UE::Get_LinearVelocity(Body).Size()) : 0.0f);
        if (_SpeedSamples.Num() > MaxSamples)
        { _SpeedSamples.RemoveAt(0, _SpeedSamples.Num() - MaxSamples); }
    }
    else
    {
        _SpeedSamples.Reset();
    }
    if (_SpeedSeries.IsValid()) { _SpeedSeries->TrySetSamples(_SpeedSamples); }
}

auto SCkInspector_JoltAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    Push_SpeedSample();
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_JoltAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _SpeedSamples.Reset();
    _SpeedSeries.Reset();
    _View.Reset();
    _Mounted = false;
}

// =====================================================================================================================

auto FCkInspector_Jolt::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Jolt"));
}

auto FCkInspector_Jolt::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return UCk_Utils_JoltBody_UE::Has(Entity)
        || UCk_Utils_JoltCharacter_UE::Has(Entity)
        || UCk_Utils_JoltStaticActor_UE::Has(Entity);
}

// =====================================================================================================================

auto FCkInspector_Jolt::Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    if (ck::Is_NOT_Valid(Entity)) { return Builder.Build(Entity); }

    // ---- JoltBody ----
    if (UCk_Utils_JoltBody_UE::Has(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Jolt Body")));

        auto        MutableEntity = Entity;
        const auto  CapturedBody  = UCk_Utils_JoltBody_UE::CastChecked(MutableEntity);
        const auto  CapturedEntity = Entity;

        Builder.AddRow(
            FText::FromString(TEXT("Body Id:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_JoltBody_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Raw = CapturedEntity.Get<ck::FFragment_JoltBody_Current>().Get_BodyId().GetIndexAndSequenceNumber();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Raw));
            },
            CkStyle::Value_Numeric());

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Motion Type:")),
            TAttribute<FText>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltBody_UE::Get_MotionType(CapturedBody)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return ECk_Tone::Neutral; }
                return ck_inspector_jolt::Get_MotionTone(UCk_Utils_JoltBody_UE::Get_MotionType(CapturedBody));
            }));

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Sleep State:")),
            TAttribute<FText>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltBody_UE::Get_SleepState(CapturedBody)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return ECk_Tone::Neutral; }
                return ck_inspector_jolt::Get_SleepTone(UCk_Utils_JoltBody_UE::Get_SleepState(CapturedBody));
            }));

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Body Added:")),
            TAttribute<FText>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(UCk_Utils_JoltBody_UE::Get_IsBodyAdded(CapturedBody) ? TEXT("Yes") : TEXT("No"));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return ECk_Tone::Neutral; }
                // A body that never made it into the Jolt world simulates nothing — that is a defect, not a mode.
                return UCk_Utils_JoltBody_UE::Get_IsBodyAdded(CapturedBody) ? ECk_Tone::Ok : ECk_Tone::Warn;
            }));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Linear Velocity:")),
            ck_inspector_jolt::Make_AxisComponents([CapturedBody]() -> FVector
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FVector::ZeroVector; }
                return UCk_Utils_JoltBody_UE::Get_LinearVelocity(CapturedBody);
            }));

        Builder.AddSparklineRow(
            FText::FromString(TEXT("Linear Speed:")),
            TAttribute<float>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return 0.0f; }
                return static_cast<float>(UCk_Utils_JoltBody_UE::Get_LinearVelocity(CapturedBody).Size());
            }),
            ECk_Tone::Accent,
            TAttribute<FText>::CreateLambda([CapturedBody]()
            {
                if (ck::Is_NOT_Valid(CapturedBody)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{:.2f}"), UCk_Utils_JoltBody_UE::Get_LinearVelocity(CapturedBody).Size()));
            }));
    }

    // ---- JoltCharacter ----
    if (UCk_Utils_JoltCharacter_UE::Has(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Jolt Character")));

        auto       MutableEntity     = Entity;
        const auto CapturedCharacter = UCk_Utils_JoltCharacter_UE::CastChecked(MutableEntity);

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Ground State:")),
            TAttribute<FText>::CreateLambda([CapturedCharacter]()
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_JoltCharacter_UE::Get_GroundState(CapturedCharacter)));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedCharacter]()
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return ECk_Tone::Neutral; }
                return ck_inspector_jolt::Get_GroundTone(UCk_Utils_JoltCharacter_UE::Get_GroundState(CapturedCharacter));
            }));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Ground Normal:")),
            ck_inspector_jolt::Make_AxisComponents([CapturedCharacter]() -> FVector
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FVector::ZeroVector; }
                return UCk_Utils_JoltCharacter_UE::Get_GroundNormal(CapturedCharacter);
            }));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Ground Velocity:")),
            ck_inspector_jolt::Make_AxisComponents([CapturedCharacter]() -> FVector
            {
                if (ck::Is_NOT_Valid(CapturedCharacter)) { return FVector::ZeroVector; }
                return UCk_Utils_JoltCharacter_UE::Get_GroundVelocity(CapturedCharacter);
            }));
    }

    // ---- JoltStaticActor ----
    if (UCk_Utils_JoltStaticActor_UE::Has(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Jolt Static Actor")));

        auto       MutableEntity       = Entity;
        const auto CapturedStaticActor = UCk_Utils_JoltStaticActor_UE::CastChecked(MutableEntity);

        Builder.AddRow(
            FText::FromString(TEXT("Source Actor:")),
            [CapturedStaticActor](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedStaticActor)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(UCk_Utils_JoltStaticActor_UE::Get_SourceActorName(CapturedStaticActor).ToString());
            },
            CkStyle::Value_Object());

        Builder.AddCountBadgeRow(
            FText::FromString(TEXT("Num Bodies:")),
            TAttribute<int32>::CreateLambda([CapturedStaticActor]()
            {
                if (ck::Is_NOT_Valid(CapturedStaticActor)) { return 0; }
                return UCk_Utils_JoltStaticActor_UE::Get_NumBodies(CapturedStaticActor);
            }),
            ECk_Tone::Info);
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Jolt::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    _StructureMask = ck_inspector_jolt::Get_StructureMask(Entity);

    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : {TEXT("Body Id:"), TEXT("Motion Type:"), TEXT("Sleep State:"),
        TEXT("Body Added:"), TEXT("Linear Velocity:"), TEXT("Linear Speed:"), TEXT("Ground State:"),
        TEXT("Ground Normal:"), TEXT("Ground Velocity:"), TEXT("Source Actor:"), TEXT("Num Bodies:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); }
    }

    const TSharedRef<SCkInspector_JoltAuthored> Authored = SNew(SCkInspector_JoltAuthored)
        .Entity(Entity)
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

// =====================================================================================================================

auto FCkInspector_Jolt::Tick(const FCk_Handle& Entity, const float InDeltaTime) -> void
{
    static_cast<void>(InDeltaTime);
    const uint8 StructureMask = ck_inspector_jolt::Get_StructureMask(Entity);
    if (StructureMask != _StructureMask)
    {
        _StructureMask = StructureMask;
        RequestRebuild();
    }
    _LastAuthoredLoadError.Reset();
    for (const TWeakPtr<SCkInspector_JoltAuthored>& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        {
            _LastAuthoredLoadError = Instance->Get_LoadError();
            break;
        }
    }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_JoltAuthored>& Instance)
    { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); });
}

FCkInspector_Jolt::~FCkInspector_Jolt()
{
    OnDeactivated();
}

auto FCkInspector_Jolt::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_JoltAuthored>& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}

// =====================================================================================================================
