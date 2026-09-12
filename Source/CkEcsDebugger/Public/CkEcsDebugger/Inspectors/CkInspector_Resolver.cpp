#include "CkInspector_Resolver.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "ResolverDataBundle/CkResolverDataBundle_Fragment.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Resolver)

namespace ck_inspector_resolver
{
    auto HasCurrent(const FCk_Handle& InEntity) -> bool
    {
        return ck::IsValid(InEntity) && InEntity.Has<ck::FFragment_ResolverDataBundle_Current>();
    }

    auto HasPending(const FCk_Handle& InEntity) -> bool
    {
        return HasCurrent(InEntity) && InEntity.Has<ck::FFragment_ResolverDataBundle_PendingOperations>();
    }

    auto Get_PhaseCount(const FCk_Handle& InEntity) -> int32
    {
        if (NOT HasCurrent(InEntity) || NOT InEntity.Has<ck::FFragment_ResolverDataBundle_Params>())
        { return 0; }
        return InEntity.Get<ck::FFragment_ResolverDataBundle_Params>().Get_Params().Get_Phases().Num();
    }

    auto Get_PhaseIndex(const FCk_Handle& InEntity) -> int32
    {
        if (NOT HasCurrent(InEntity)) { return INDEX_NONE; }
        return InEntity.Get<ck::FFragment_ResolverDataBundle_Current>().Get_CurrentPhaseIndex();
    }

    auto BuildFinalValueText(const FCk_Handle& InEntity) -> FString
    {
        if (NOT HasCurrent(InEntity)) { return TEXT("--"); }
        const float Value = InEntity.Get<ck::FFragment_ResolverDataBundle_Current>().Get_FinalValue();
        return FString::Printf(TEXT("%.3f"), Value);
    }

    auto BuildPhaseFraction(const FCk_Handle& InEntity) -> float
    {
        const int32 PhaseCount = Get_PhaseCount(InEntity);
        const int32 PhaseIndex = Get_PhaseIndex(InEntity);
        if (PhaseCount <= 0 || PhaseIndex < 0 || PhaseIndex >= PhaseCount) { return 0.0f; }
        return static_cast<float>(PhaseIndex + 1) / static_cast<float>(PhaseCount);
    }

    auto BuildPhaseText(const FCk_Handle& InEntity) -> FString
    {
        const int32 PhaseCount = Get_PhaseCount(InEntity);
        const int32 PhaseIndex = Get_PhaseIndex(InEntity);
        if (PhaseCount <= 0 || PhaseIndex < 0 || PhaseIndex >= PhaseCount)
        { return ck::Format_UE(TEXT("-- / {}"), PhaseCount); }
        return ck::Format_UE(TEXT("{} / {}"), PhaseIndex + 1, PhaseCount);
    }

    auto BuildMetadataTagsText(const FCk_Handle& InEntity) -> FString
    {
        if (NOT HasCurrent(InEntity)) { return TEXT("--"); }
        const FGameplayTagContainer& Tags =
            InEntity.Get<ck::FFragment_ResolverDataBundle_Current>().Get_MetadataTags();
        return Tags.IsEmpty() ? TEXT("(none)") : Tags.ToString();
    }

    auto BuildModifierOpsValue(const FCk_Handle& InEntity) -> int32
    {
        if (NOT HasPending(InEntity)) { return 0; }
        return InEntity.Get<ck::FFragment_ResolverDataBundle_PendingOperations>()
            .Get_PendingModifiersOperations().Num();
    }

    auto BuildMetadataOpsValue(const FCk_Handle& InEntity) -> int32
    {
        if (NOT HasPending(InEntity)) { return 0; }
        return InEntity.Get<ck::FFragment_ResolverDataBundle_PendingOperations>()
            .Get_PendingMetadataOperations().Num();
    }

    auto BuildModifierOpsText(const FCk_Handle& InEntity) -> FString
    {
        return HasPending(InEntity) ? LexToString(BuildModifierOpsValue(InEntity)) : TEXT("--");
    }

    auto BuildMetadataOpsText(const FCk_Handle& InEntity) -> FString
    {
        return HasPending(InEntity) ? LexToString(BuildMetadataOpsValue(InEntity)) : TEXT("--");
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

auto SCkInspector_ResolverAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _FinalValueDiffMarked = InArgs._FinalValueDiffMarked;
    _PhaseDiffMarked = InArgs._PhaseDiffMarked;
    _MetadataTagsDiffMarked = InArgs._MetadataTagsDiffMarked;
    _ModifierOpsDiffMarked = InArgs._ModifierOpsDiffMarked;
    _MetadataOpsDiffMarked = InArgs._MetadataOpsDiffMarked;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_AuthoredView())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_ResolverAuthored::~SCkInspector_ResolverAuthored()
{
    Release();
}

auto SCkInspector_ResolverAuthored::Get_IsAvailable() const -> bool
{
    return _Active && ck_inspector_resolver::HasCurrent(_Entity);
}

auto SCkInspector_ResolverAuthored::Get_FinalValueText() const -> FString
{
    return _Active ? ck_inspector_resolver::BuildFinalValueText(_Entity) : FString{};
}

auto SCkInspector_ResolverAuthored::Get_PhaseText() const -> FString
{
    return _Active ? ck_inspector_resolver::BuildPhaseText(_Entity) : FString{};
}

auto SCkInspector_ResolverAuthored::Get_MetadataTagsText() const -> FString
{
    return _Active ? ck_inspector_resolver::BuildMetadataTagsText(_Entity) : FString{};
}

auto SCkInspector_ResolverAuthored::Get_ModifierOpsText() const -> FString
{
    return _Active ? ck_inspector_resolver::BuildModifierOpsText(_Entity) : FString{};
}

auto SCkInspector_ResolverAuthored::Get_MetadataOpsText() const -> FString
{
    return _Active ? ck_inspector_resolver::BuildMetadataOpsText(_Entity) : FString{};
}

auto SCkInspector_ResolverAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_ResolverAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    auto BindText = [&Data, WeakWidget](const FString& InName,
        FString (SCkInspector_ResolverAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_ResolverAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("resolver-final-value"), &SCkInspector_ResolverAuthored::Get_FinalValueText);
    BindText(TEXT("resolver-phase"), &SCkInspector_ResolverAuthored::Get_PhaseText);
    BindText(TEXT("resolver-metadata-tags"), &SCkInspector_ResolverAuthored::Get_MetadataTagsText);
    BindText(TEXT("resolver-modifier-ops"), &SCkInspector_ResolverAuthored::Get_ModifierOpsText);
    BindText(TEXT("resolver-metadata-ops"), &SCkInspector_ResolverAuthored::Get_MetadataOpsText);
    Data.Number.Add(TEXT("resolver-phase-fraction"), TAttribute<float>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ResolverAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? ck_inspector_resolver::BuildPhaseFraction(Widget->_Entity) : 0.0f;
    }));
    Data.Visibility.Add(TEXT("resolver-pending-visible"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ResolverAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            && ck_inspector_resolver::HasPending(Widget->_Entity);
    }));
    Data.Color.Add(TEXT("resolver-phase-fill"), CkStyle::GetToneColor(ECk_Tone::Accent));
    Data.Color.Add(TEXT("resolver-count-foreground"), CkStyle::GetToneColor(ECk_Tone::Info));
    Data.Color.Add(TEXT("resolver-count-background"), CkStyle::GetToneDimColor(ECk_Tone::Info));

    auto BindDiffColor = [&Data, WeakWidget](const FString& InName,
        const bool SCkInspector_ResolverAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_ResolverAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_resolver::DiffColor(Widget.Get()->*InMember)
                : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("resolver-final-value-diff-color"), &SCkInspector_ResolverAuthored::_FinalValueDiffMarked);
    BindDiffColor(TEXT("resolver-phase-diff-color"), &SCkInspector_ResolverAuthored::_PhaseDiffMarked);
    BindDiffColor(TEXT("resolver-metadata-tags-diff-color"), &SCkInspector_ResolverAuthored::_MetadataTagsDiffMarked);
    BindDiffColor(TEXT("resolver-modifier-ops-diff-color"), &SCkInspector_ResolverAuthored::_ModifierOpsDiffMarked);
    BindDiffColor(TEXT("resolver-metadata-ops-diff-color"), &SCkInspector_ResolverAuthored::_MetadataOpsDiffMarked);

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorResolver.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorResolver.ui.css")));
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

auto SCkInspector_ResolverAuthored::Tick(
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

auto SCkInspector_ResolverAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _View.Reset();
    _Mounted = false;
}

FCkInspector_Resolver::~FCkInspector_Resolver()
{
    OnDeactivated();
}

auto FCkInspector_Resolver::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Resolver"));
}

auto FCkInspector_Resolver::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck_inspector_resolver::HasCurrent(Entity);
}

auto FCkInspector_Resolver::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    if (NOT ck_inspector_resolver::HasCurrent(Entity)) { return Builder.Build(Entity); }
    const FCk_Handle CapturedEntity = Entity;
    Builder.AddHeader(FText::FromString(TEXT("Resolution")));
    Builder.AddRow(FText::FromString(TEXT("Final Value:")),
        [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_resolver::BuildFinalValueText(CapturedEntity)); },
        CkStyle::Value_Numeric());
    Builder.AddMeterRow(FText::FromString(TEXT("Phase:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        { return ck_inspector_resolver::BuildPhaseFraction(CapturedEntity); }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        { return FText::FromString(ck_inspector_resolver::BuildPhaseText(CapturedEntity)); }));
    Builder.AddRow(FText::FromString(TEXT("Metadata Tags:")),
        [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_resolver::BuildMetadataTagsText(CapturedEntity)); },
        CkStyle::Value_Tag());
    if (ck_inspector_resolver::HasPending(Entity))
    {
        Builder.AddHeader(FText::FromString(TEXT("Pending")));
        Builder.AddCountBadgeRow(FText::FromString(TEXT("Modifier Ops:")),
            TAttribute<int32>::CreateLambda([CapturedEntity]()
            { return ck_inspector_resolver::BuildModifierOpsValue(CapturedEntity); }), ECk_Tone::Info);
        Builder.AddCountBadgeRow(FText::FromString(TEXT("Metadata Ops:")),
            TAttribute<int32>::CreateLambda([CapturedEntity]()
            { return ck_inspector_resolver::BuildMetadataOpsValue(CapturedEntity); }), ECk_Tone::Info);
    }
    return Builder.Build(Entity);
}

auto FCkInspector_Resolver::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    const TSharedRef<SCkInspector_ResolverAuthored> Authored = SNew(SCkInspector_ResolverAuthored)
        .Entity(Entity)
        .FinalValueDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Final Value:")))
        .PhaseDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Phase:")))
        .MetadataTagsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Metadata Tags:")))
        .ModifierOpsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Modifier Ops:")))
        .MetadataOpsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Metadata Ops:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Resolver::Tick(const FCk_Handle&, const float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_ResolverAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_Resolver::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_ResolverAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_ResolverAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}
