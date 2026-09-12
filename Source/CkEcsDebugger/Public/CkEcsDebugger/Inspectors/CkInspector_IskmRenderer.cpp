#include "CkInspector_IskmRenderer.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkIskmRenderer/AnimCollection/CkIskmAnimCollection_Fragment_Data.h"
#include "CkIskmRenderer/Renderer/CkIskmRenderer_Fragment_Data.h"
#include "CkIskmRenderer/Renderer/CkIskmRenderer_Utils.h"

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

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_IskmRenderer)

namespace ck_inspector_iskm_renderer
{
    auto TryGetRenderer(const FCk_Handle& InEntity, FCk_Handle_IskmRenderer& OutRenderer) -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_IskmRenderer_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        const FCk_Handle_IskmRenderer Candidate = UCk_Utils_IskmRenderer_UE::CastChecked(MutableEntity);
        if (ck::Is_NOT_Valid(Candidate)) { return false; }
        OutRenderer = Candidate;
        return true;
    }

    auto GetRendererData(const FCk_Handle& InEntity) -> UCk_IskmRenderer_Data*
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        if (NOT TryGetRenderer(InEntity, Renderer)) { return nullptr; }
        UCk_IskmRenderer_Data* const Data = UCk_Utils_IskmRenderer_UE::Get_RendererData(Renderer);
        return ck::IsValid(Data, ck::IsValid_Policy_NullptrOnly{}) ? Data : nullptr;
    }

    auto GetAnimCollection(const FCk_Handle& InEntity) -> UCk_IskmAnimCollection_Data*
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        if (NOT TryGetRenderer(InEntity, Renderer)) { return nullptr; }
        UCk_IskmAnimCollection_Data* const Collection = UCk_Utils_IskmRenderer_UE::Get_AnimCollection(Renderer);
        return ck::IsValid(Collection, ck::IsValid_Policy_NullptrOnly{}) ? Collection : nullptr;
    }

    auto BuildRendererDataText(const FCk_Handle& InEntity) -> FString
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        if (NOT TryGetRenderer(InEntity, Renderer)) { return TEXT("--"); }
        const UCk_IskmRenderer_Data* const Data = GetRendererData(InEntity);
        return Data != nullptr ? Data->GetName() : TEXT("None");
    }

    auto BuildAnimCollectionText(const FCk_Handle& InEntity) -> FString
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        if (NOT TryGetRenderer(InEntity, Renderer)) { return TEXT("--"); }
        const UCk_IskmAnimCollection_Data* const Collection = GetAnimCollection(InEntity);
        return Collection != nullptr ? Collection->GetName() : TEXT("None");
    }

    auto BuildSequencesText(const FCk_Handle& InEntity) -> FString
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        if (NOT TryGetRenderer(InEntity, Renderer)) { return TEXT("--"); }
        const UCk_IskmAnimCollection_Data* const Collection = GetAnimCollection(InEntity);
        return LexToString(Collection != nullptr ? Collection->Get_Sequences().Num() : 0);
    }

    auto BuildSequencesValue(const FCk_Handle& InEntity) -> int32
    {
        const UCk_IskmAnimCollection_Data* const Collection = GetAnimCollection(InEntity);
        return Collection != nullptr ? Collection->Get_Sequences().Num() : 0;
    }

    auto BuildSubmeshesText(const FCk_Handle& InEntity) -> FString
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        if (NOT TryGetRenderer(InEntity, Renderer)) { return TEXT("--"); }
        const UCk_IskmRenderer_Data* const Data = GetRendererData(InEntity);
        return LexToString(Data != nullptr ? Data->Get_Submeshes().Num() : 0);
    }

    auto BuildSubmeshesValue(const FCk_Handle& InEntity) -> int32
    {
        const UCk_IskmRenderer_Data* const Data = GetRendererData(InEntity);
        return Data != nullptr ? Data->Get_Submeshes().Num() : 0;
    }

    auto BuildDefaultAnimInstanceText(const FCk_Handle& InEntity) -> FString
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        if (NOT TryGetRenderer(InEntity, Renderer)) { return TEXT("--"); }
        const UCk_IskmRenderer_Data* const Data = GetRendererData(InEntity);
        if (Data == nullptr) { return TEXT("--"); }
        const TSoftClassPtr<UAnimInstance>& ClassRef = Data->Get_DefaultAnimInstanceClass();
        return ClassRef.IsNull() ? TEXT("(IskmNotify fallback)") : ClassRef.GetAssetName();
    }

    auto BuildCustomDataSlotsText(const FCk_Handle& InEntity) -> FString
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        if (NOT TryGetRenderer(InEntity, Renderer)) { return TEXT("--"); }
        const UCk_IskmRenderer_Data* const Data = GetRendererData(InEntity);
        return LexToString(Data != nullptr ? Data->Get_NumCustomDataFloat() : 0);
    }

    auto BuildCustomDataSlotsValue(const FCk_Handle& InEntity) -> int32
    {
        const UCk_IskmRenderer_Data* const Data = GetRendererData(InEntity);
        return Data != nullptr ? Data->Get_NumCustomDataFloat() : 0;
    }

    auto BuildCountTone(const FCk_Handle& InEntity) -> ECk_Tone
    {
        auto Renderer = FCk_Handle_IskmRenderer{};
        return TryGetRenderer(InEntity, Renderer) ? ECk_Tone::Info : ECk_Tone::Neutral;
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

auto SCkInspector_IskmRendererAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _RendererDataDiffMarked = InArgs._RendererDataDiffMarked;
    _AnimCollectionDiffMarked = InArgs._AnimCollectionDiffMarked;
    _SequencesDiffMarked = InArgs._SequencesDiffMarked;
    _SubmeshesDiffMarked = InArgs._SubmeshesDiffMarked;
    _DefaultAnimInstanceDiffMarked = InArgs._DefaultAnimInstanceDiffMarked;
    _CustomDataSlotsDiffMarked = InArgs._CustomDataSlotsDiffMarked;

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

SCkInspector_IskmRendererAuthored::~SCkInspector_IskmRendererAuthored()
{
    Release();
}

auto SCkInspector_IskmRendererAuthored::Get_IsAvailable() const -> bool
{
    auto Renderer = FCk_Handle_IskmRenderer{};
    return _Active && ck_inspector_iskm_renderer::TryGetRenderer(_Entity, Renderer);
}

auto SCkInspector_IskmRendererAuthored::Get_RendererDataText() const -> FString
{
    return _Active ? ck_inspector_iskm_renderer::BuildRendererDataText(_Entity) : FString{};
}

auto SCkInspector_IskmRendererAuthored::Get_AnimCollectionText() const -> FString
{
    return _Active ? ck_inspector_iskm_renderer::BuildAnimCollectionText(_Entity) : FString{};
}

auto SCkInspector_IskmRendererAuthored::Get_SequencesText() const -> FString
{
    return _Active ? ck_inspector_iskm_renderer::BuildSequencesText(_Entity) : FString{};
}

auto SCkInspector_IskmRendererAuthored::Get_SubmeshesText() const -> FString
{
    return _Active ? ck_inspector_iskm_renderer::BuildSubmeshesText(_Entity) : FString{};
}

auto SCkInspector_IskmRendererAuthored::Get_DefaultAnimInstanceText() const -> FString
{
    return _Active ? ck_inspector_iskm_renderer::BuildDefaultAnimInstanceText(_Entity) : FString{};
}

auto SCkInspector_IskmRendererAuthored::Get_CustomDataSlotsText() const -> FString
{
    return _Active ? ck_inspector_iskm_renderer::BuildCustomDataSlotsText(_Entity) : FString{};
}

auto SCkInspector_IskmRendererAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_IskmRendererAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    auto BindText = [&Data, WeakWidget](const FString& InName,
        FString (SCkInspector_IskmRendererAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_IskmRendererAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("iskm-renderer-data"), &SCkInspector_IskmRendererAuthored::Get_RendererDataText);
    BindText(TEXT("iskm-anim-collection"), &SCkInspector_IskmRendererAuthored::Get_AnimCollectionText);
    BindText(TEXT("iskm-sequences"), &SCkInspector_IskmRendererAuthored::Get_SequencesText);
    BindText(TEXT("iskm-submeshes"), &SCkInspector_IskmRendererAuthored::Get_SubmeshesText);
    BindText(TEXT("iskm-default-anim-instance"), &SCkInspector_IskmRendererAuthored::Get_DefaultAnimInstanceText);
    BindText(TEXT("iskm-custom-data-slots"), &SCkInspector_IskmRendererAuthored::Get_CustomDataSlotsText);

    Data.Color.Add(TEXT("iskm-count-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_IskmRendererAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? CkStyle::GetToneColor(ck_inspector_iskm_renderer::BuildCountTone(Widget->_Entity))
            : FLinearColor::Transparent;
    }));
    Data.Color.Add(TEXT("iskm-count-background"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_IskmRendererAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? CkStyle::GetToneDimColor(ck_inspector_iskm_renderer::BuildCountTone(Widget->_Entity))
            : FLinearColor::Transparent;
    }));
    auto BindDiffColor = [&Data, WeakWidget](const FString& InName,
        const bool SCkInspector_IskmRendererAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const TSharedPtr<SCkInspector_IskmRendererAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_iskm_renderer::DiffColor(Widget.Get()->*InMember)
                : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("iskm-renderer-data-diff-color"), &SCkInspector_IskmRendererAuthored::_RendererDataDiffMarked);
    BindDiffColor(TEXT("iskm-anim-collection-diff-color"), &SCkInspector_IskmRendererAuthored::_AnimCollectionDiffMarked);
    BindDiffColor(TEXT("iskm-sequences-diff-color"), &SCkInspector_IskmRendererAuthored::_SequencesDiffMarked);
    BindDiffColor(TEXT("iskm-submeshes-diff-color"), &SCkInspector_IskmRendererAuthored::_SubmeshesDiffMarked);
    BindDiffColor(TEXT("iskm-default-anim-instance-diff-color"), &SCkInspector_IskmRendererAuthored::_DefaultAnimInstanceDiffMarked);
    BindDiffColor(TEXT("iskm-custom-data-slots-diff-color"), &SCkInspector_IskmRendererAuthored::_CustomDataSlotsDiffMarked);

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorIskmRenderer.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorIskmRenderer.ui.css")));
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

auto SCkInspector_IskmRendererAuthored::Tick(
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

auto SCkInspector_IskmRendererAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _View.Reset();
    _Mounted = false;
}

FCkInspector_IskmRenderer::~FCkInspector_IskmRenderer()
{
    OnDeactivated();
}

auto FCkInspector_IskmRenderer::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Iskm Renderer"));
}

auto FCkInspector_IskmRenderer::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Renderer = FCk_Handle_IskmRenderer{};
    return ck_inspector_iskm_renderer::TryGetRenderer(Entity, Renderer);
}

auto FCkInspector_IskmRenderer::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    const FCk_Handle CapturedEntity = Entity;
    Builder.AddRow(FText::FromString(TEXT("RendererData:")),
        [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_iskm_renderer::BuildRendererDataText(CapturedEntity)); },
        CkStyle::Value_Object());
    Builder.AddRow(FText::FromString(TEXT("AnimCollection:")),
        [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_iskm_renderer::BuildAnimCollectionText(CapturedEntity)); },
        CkStyle::Value_Object());
    Builder.AddCountBadgeRow(FText::FromString(TEXT("Sequences:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        { return ck_inspector_iskm_renderer::BuildSequencesValue(CapturedEntity); }), ECk_Tone::Info);
    Builder.AddCountBadgeRow(FText::FromString(TEXT("Submeshes:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        { return ck_inspector_iskm_renderer::BuildSubmeshesValue(CapturedEntity); }), ECk_Tone::Info);
    Builder.AddRow(FText::FromString(TEXT("Default AnimInstance:")),
        [CapturedEntity](const FCk_Handle&)
        { return FText::FromString(ck_inspector_iskm_renderer::BuildDefaultAnimInstanceText(CapturedEntity)); },
        CkStyle::Value_Object());
    Builder.AddCountBadgeRow(FText::FromString(TEXT("Custom Data Slots:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        { return ck_inspector_iskm_renderer::BuildCustomDataSlotsValue(CapturedEntity); }), ECk_Tone::Info);
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_IskmRenderer::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    const TSharedRef<SCkInspector_IskmRendererAuthored> Authored = SNew(SCkInspector_IskmRendererAuthored)
        .Entity(Entity)
        .RendererDataDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("RendererData:")))
        .AnimCollectionDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("AnimCollection:")))
        .SequencesDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Sequences:")))
        .SubmeshesDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Submeshes:")))
        .DefaultAnimInstanceDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Default AnimInstance:")))
        .CustomDataSlotsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Custom Data Slots:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_IskmRenderer::Tick(const FCk_Handle&, const float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_IskmRendererAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_IskmRenderer::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_IskmRendererAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_IskmRendererAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}
