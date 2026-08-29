#include "CkTextureDebugger/Window/SCkTextureDebuggerWindow.h"

#include "CkTextureDebugger/CkTextureDebugger_Log.h"

#include "CkTextureDebugger/Analysis/CkTextureDebugger_MaterialAnalysis.h"
#include "CkTextureDebugger/Analysis/CkTextureDebugger_SurfaceAnalysis.h"
#include "CkTextureDebugger/Analysis/CkTextureDebugger_UvDensity.h"
#include "CkTextureDebugger/Data/CkTextureDebugger_LoadedWorldCollector.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_DiagnosticPages.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_SceneAuditTable.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportComponentPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportComponentPickerControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
    #include "Editor.h"
    #include "UObject/ObjectSaveContext.h"
#endif

#define LOCTEXT_NAMESPACE "SCkTextureDebuggerWindow"

namespace ck_texture_debugger_window
{
    struct FCheckerDesc { FName Id; const TCHAR* Label; const TCHAR* TexturePath; };
    constexpr auto CheckerMaterialPath = TEXT("/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker.M_CkTextureChecker");
    constexpr auto CheckerTextureParameter = TEXT("CheckerTexture");
    const FCheckerDesc Checkers[] = {
        {TEXT("ColorGrid2K"), TEXT("Color Grid 2K"), TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_ColorGrid_2K.T_CkTextureChecker_ColorGrid_2K")},
        {TEXT("ColorGrid4K"), TEXT("Color Grid 4K"), TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_ColorGrid_4K.T_CkTextureChecker_ColorGrid_4K")},
        {TEXT("GoldGray4K"), TEXT("Gold / Gray 4K"), TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_GoldGray_4K.T_CkTextureChecker_GoldGray_4K")},
        {TEXT("RoundedSpectrum4K"), TEXT("Rounded Spectrum 4K"), TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_RoundedSpectrum_4K.T_CkTextureChecker_RoundedSpectrum_4K")},
        {TEXT("DirectionalMono4K"), TEXT("Directional Mono 4K"), TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_DirectionalMono_4K.T_CkTextureChecker_DirectionalMono_4K")},
    };

    auto Get_PageIndex(FName InId) -> int32
    {
        if (InId == TEXT("Checker")) { return 0; }
        if (InId == TEXT("Health")) { return 1; }
        if (InId == TEXT("UvDensity")) { return 2; }
        if (InId == TEXT("MaterialInputs")) { return 3; }
        if (InId == TEXT("SurfaceLighting")) { return 4; }
        if (InId == TEXT("SceneAudit")) { return 5; }
        return 0;
    }

    auto FindChecker(FName InId) -> const FCheckerDesc*
    {
        for (const auto& Checker : Checkers) { if (Checker.Id == InId) { return &Checker; } }
        return nullptr;
    }

    auto StreamingLabel(ECkTextureDebugger_StreamingAvailability InValue) -> FString
    {
        switch (InValue)
        {
            case ECkTextureDebugger_StreamingAvailability::Available: return TEXT("Available");
            case ECkTextureDebugger_StreamingAvailability::ManagerUnavailable: return TEXT("Unavailable: streaming manager is not running");
            case ECkTextureDebugger_StreamingAvailability::StreamingDisabled: return TEXT("Unavailable: texture streaming is disabled");
            case ECkTextureDebugger_StreamingAvailability::NotStreamable: return TEXT("Not streamable");
            case ECkTextureDebugger_StreamingAvailability::ResourceNotCreated: return TEXT("Unavailable: render resource is not created");
        }
        return TEXT("Unknown");
    }
}

SCkTextureDebuggerWindow::~SCkTextureDebuggerWindow()
{
    if (_WorldCleanupHandle.IsValid()) { FWorldDelegates::OnWorldCleanup.Remove(_WorldCleanupHandle); }
    if (_WorldChangedHandle.IsValid() && _WorldModel.IsValid()) { _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }
    if (_SessionInvalidatedHandle.IsValid()) { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }
#if WITH_EDITOR
    if (_PreSaveWorldHandle.IsValid()) { FEditorDelegates::PreSaveWorldWithContext.Remove(_PreSaveWorldHandle); }
    if (_PostSaveWorldHandle.IsValid()) { FEditorDelegates::PostSaveWorldWithContext.Remove(_PostSaveWorldHandle); }
#endif
    if (IsEngineExitRequested())
    {
        _OverrideSession.ReleaseForWorldCleanup(_OverrideSession.GetWorld());
    }
    else
    {
        RemoveCheckerFromLiveWorld();
    }
}

auto
    SCkTextureDebuggerWindow::
    Construct(
        const FArguments&)
    -> void
{
    Register_WithGate();
    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();
    _WorldModel->Set_IncludeEditorWorld(true);
    _WorldModel->Ensure_AutoSelect();
    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(this, &SCkTextureDebuggerWindow::OnModelWorldChanged);
    _WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddSP(this, &SCkTextureDebuggerWindow::OnWorldCleanup);
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(this, &SCkTextureDebuggerWindow::OnSessionInvalidated);
#if WITH_EDITOR
    _PreSaveWorldHandle = FEditorDelegates::PreSaveWorldWithContext.AddSP(this, &SCkTextureDebuggerWindow::OnPreSaveWorld);
    _PostSaveWorldHandle = FEditorDelegates::PostSaveWorldWithContext.AddSP(this, &SCkTextureDebuggerWindow::OnPostSaveWorld);
#endif

    _ComponentPicker = MakeShared<FCkDebug_ViewportComponentPicker>();
    _ComponentPicker->Construct({
        [WeakModel = TWeakPtr<FCkDebuggerModel_WorldSelector>(_WorldModel)]() -> UWorld*
        {
            const auto Model = WeakModel.Pin();
            return Model.IsValid() ? Model->Get_SelectedWorld() : nullptr;
        },
        [WeakWindow = TWeakPtr<SCkTextureDebuggerWindow>(SharedThis(this))](const FCkDebug_ComponentPickResult& InPick)
        {
            const auto Window = WeakWindow.Pin();
            if (Window.IsValid()) { Window->OnComponentPicked(InPick); }
        },
        [](const UPrimitiveComponent& InComponent) { return Cast<UMeshComponent>(&InComponent) != nullptr; }
    });

    _CheckerTextureRoots.Reserve(UE_ARRAY_COUNT(ck_texture_debugger_window::Checkers));
    _CheckerBrushes.Reserve(UE_ARRAY_COUNT(ck_texture_debugger_window::Checkers));
    auto LoadedCheckerCount = 0;
    for (const auto& Checker : ck_texture_debugger_window::Checkers)
    {
        auto* Texture = LoadObject<UTexture>(nullptr, Checker.TexturePath);
        if (Texture == nullptr)
        {
            UE_LOG(LogCkTextureDebugger, Error, TEXT("Checker texture failed to load: %s"), Checker.TexturePath);
        }
        else
        {
            ++LoadedCheckerCount;
        }
        _CheckerTextureRoots.Emplace(Texture);
        auto& Brush = _CheckerBrushes.AddDefaulted_GetRef();
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.ImageSize = FVector2D{96.0f, 96.0f};
        Brush.SetResourceObject(Texture);
    }
    RefreshSnapshot();

    const auto* CheckerMaterial = TryLoadCheckerMaterial();
    if (CheckerMaterial == nullptr)
    {
        UE_LOG(
            LogCkTextureDebugger,
            Error,
            TEXT("Texture debugger runtime assets: checker textures %d/%d, master material missing."),
            LoadedCheckerCount,
            UE_ARRAY_COUNT(ck_texture_debugger_window::Checkers));
    }
    else
    {
        UE_LOG(
            LogCkTextureDebugger,
            Display,
            TEXT("Texture debugger runtime assets: checker textures %d/%d, master material loaded."),
            LoadedCheckerCount,
            UE_ARRAY_COUNT(ck_texture_debugger_window::Checkers));
    }

    _PageSwitcher = SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return Get_ActivePageIndex(); });
    _PageSwitcher->AddSlot()[Build_CheckerPage()];
    _PageSwitcher->AddSlot()[Build_TextureHealthPage()];
    _PageSwitcher->AddSlot()[SAssignNew(_UvDensityPage, SCkTextureDebugger_UvDensityPage)];
    _PageSwitcher->AddSlot()[SAssignNew(_MaterialInputsPage, SCkTextureDebugger_MaterialInputsPage)];
    _PageSwitcher->AddSlot()[SAssignNew(_SurfaceLightingPage, SCkTextureDebugger_SurfaceLightingPage)];
    _PageSwitcher->AddSlot()[Build_SceneAuditPage()];
    Sync_DiagnosticPages();

    const auto Pages = TArray<FCkDebug_UnderlineTabDesc>{
        {TEXT("Checker"), FText::FromString(TEXT("Checker"))}, {TEXT("Health"), FText::FromString(TEXT("Texture Health"))},
        {TEXT("UvDensity"), FText::FromString(TEXT("UV & Density"))}, {TEXT("MaterialInputs"), FText::FromString(TEXT("Material Inputs"))},
        {TEXT("SurfaceLighting"), FText::FromString(TEXT("Surface & Lighting"))}, {TEXT("SceneAudit"), FText::FromString(TEXT("Scene Audit"))},
    };

    ChildSlot[
        SNew(SCkDebug_WindowChrome)
        .WindowId(Get_WindowId()).ToolTabId(TEXT("CkTextureDebugger"))
        .StatusText(TAttribute<FText>::CreateSP(this, &SCkTextureDebuggerWindow::Get_StatusText)).ShowRefreshControls(true)
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(TEXT("TextureContext"), LOCTEXT("ContextTip", "Active-world selector and component picker"),
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, CkStyle::SpaceM, 0)[SNew(SCkDebug_WorldSelector, _WorldModel).ShowHeaderLabel(false)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, CkStyle::SpaceM, 0)[SNew(SCkDebug_ViewportComponentPickerControls).Picker(_ComponentPicker).PickTooltip(LOCTEXT("PickTip", "Pick a mesh component in the active Editor, PIE, or game world. Collisionless meshes use bounds fallback; no hit section is guessed as a material slot."))]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).Text(LOCTEXT("Refresh", "Refresh")).OnClicked(this, &SCkTextureDebuggerWindow::OnRefreshRequested)])
        })
        .Content()
        [
            SNew(SVerticalBox)
            // Page tabs own a strip row of their own -- below the chrome, above the switcher --
            // exactly where the Optimization debugger puts its pages. As a command-bar Context
            // group they sat inside a horizontally scrolling lane whose scrollbar is collapsed,
            // so a page past the available width was invisible AND undiscoverable.
            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
            [
                SNew(SCkDebug_UnderlineTabs)
                .Tabs(Pages)
                .ActiveTabId_Lambda([this] { return _ActivePageId; })
                .OnTabSelected(this, &SCkTextureDebuggerWindow::OnPageSelected)
            ]
            + SVerticalBox::Slot().AutoHeight()[Build_ContextStrip()]
            + SVerticalBox::Slot().FillHeight(1.0f)[_PageSwitcher.ToSharedRef()]
        ]
    ];
}

auto
    SCkTextureDebuggerWindow::
    Build_ContextStrip() const
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(FSlateColor{CkStyle::Bg1()})
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([this]() -> FText
                {
                    const auto* World = _WorldModel.IsValid() ? _WorldModel->Get_SelectedWorld() : nullptr;
                    return World == nullptr
                        ? LOCTEXT("ContextNoWorld", "No active world")
                        : FText::FromString(FString::Printf(TEXT("World · %s"), *World->GetName()));
                })
                .Tone_Lambda([this]() -> ECk_Tone
                {
                    return _WorldModel.IsValid() && _WorldModel->Get_SelectedWorld() != nullptr
                        ? ECk_Tone::Info
                        : ECk_Tone::Warn;
                })
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([this]() -> FText
                {
                    const auto* Component = Find_SelectedComponent();
                    return Component == nullptr
                        ? LOCTEXT("ContextNoComponent", "No component selected")
                        : FText::FromString(FString::Printf(
                            TEXT("Component · %s / %s"),
                            *Component->ActorDisplayName,
                            *Component->ComponentDisplayName));
                })
                .Tone_Lambda([this]() -> ECk_Tone
                {
                    return Find_SelectedComponent() == nullptr ? ECk_Tone::Neutral : ECk_Tone::Accent;
                })
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([this]() -> FText
                {
                    if (NOT _SelectedTexture.IsSet())
                    { return LOCTEXT("ContextNoTexture", "No texture selected"); }

                    const auto& Selection = _SelectedTexture.GetValue();
                    return FText::FromString(FString::Printf(
                        TEXT("Texture · %s · slot %d"),
                        *Selection.DisplayName,
                        Selection.SlotIndex));
                })
                .Tone_Lambda([this]() -> ECk_Tone
                {
                    return _SelectedTexture.IsSet() ? ECk_Tone::Accent : ECk_Tone::Neutral;
                })
            ]
        ];
}

auto
    SCkTextureDebuggerWindow::
    Get_WindowDisplayName() const
    -> FText
{
    return LOCTEXT("DisplayName", "Texture & Surface");
}

auto
    SCkTextureDebuggerWindow::
    Tick(
        const FGeometry& Geometry,
        double CurrentTime,
        float DeltaTime)
    -> void
{
    SCkDebugger_WindowBase::Tick(Geometry, CurrentTime, DeltaTime);
    if (_ComponentPicker.IsValid() && _ComponentPicker->IsActive()) { _ComponentPicker->Tick(DeltaTime); }
    _OverrideSession.DiscardDestroyedComponents();
    if (FCkDebuggerRefreshGate::Should_RefreshNow(Get_WindowId())) { RefreshSnapshot(); }
}

auto
    SCkTextureDebuggerWindow::
    Build_Section(
        FText Heading,
        TSharedRef<SWidget> Content) const
    -> TSharedRef<SWidget>
{
    return SNew(SBorder).BorderImage(CkStyle::GetRoundedBrush_Large()).BorderBackgroundColor(FSlateColor{CkStyle::Bg2()}).Padding(CkStyle::SpaceL)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(MoveTemp(Heading)).Font(CkStyle::BoldFont(CkStyle::FontSizeH4())).ColorAndOpacity(FSlateColor{CkStyle::TextStrong()})]
        + SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceM, 0, 0)[Content]];
}

auto
    SCkTextureDebuggerWindow::
    Build_TextPage(
        FText Heading,
        TAttribute<FText> Text)
    -> TSharedRef<SWidget>
{
    return SNew(SScrollBox) + SScrollBox::Slot().Padding(CkStyle::SpaceM)
    [Build_Section(MoveTemp(Heading), SNew(STextBlock).Text(MoveTemp(Text)).AutoWrapText(true).ColorAndOpacity(FSlateColor{CkStyle::Text() }))];
}

auto
    SCkTextureDebuggerWindow::
    Build_TextureHealthPage()
    -> TSharedRef<SWidget>
{
    auto Result = SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush_Large())
        .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
        .Padding(CkStyle::SpaceL)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("HealthHeading", "Texture Health"))
                .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextStrong()})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceS, 0, CkStyle::SpaceM)
            [
                SNew(STextBlock)
                .Text_Lambda([this] { return Get_HealthSummary(); })
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SAssignNew(_TextureHealthTable, SCkTextureDebugger_TextureHealthTable)
                .OnSelectionChanged(this, &SCkTextureDebuggerWindow::OnTextureHealthSelectionChanged)
            ]
        ];

    _TextureHealthTable->Set_Snapshot(_Snapshot, _SelectedComponent);
    return Result;
}

auto
    SCkTextureDebuggerWindow::
    Build_SceneAuditPage()
    -> TSharedRef<SWidget>
{
    const auto WeakWindow = TWeakPtr<SCkTextureDebuggerWindow>{SharedThis(this)};
    auto Result = SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush_Large())
        .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
        .Padding(CkStyle::SpaceL)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SceneAuditHeading", "Loaded-world Scene Audit"))
                .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextStrong()})
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceS, 0, CkStyle::SpaceM)
            [
                SNew(STextBlock)
                .Text_Lambda([this] { return Get_AuditSummary(); })
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SAssignNew(_SceneAuditTable, SCkTextureDebugger_SceneAuditTable)
                .OnComponentSelected([WeakWindow](TWeakObjectPtr<UPrimitiveComponent> InComponent)
                {
                    const auto Window = WeakWindow.Pin();
                    if (Window.IsValid())
                    { Window->OnAuditComponentSelected(InComponent); }
                })
            ]
        ];

    _SceneAuditTable->SetSnapshot(_Snapshot);
    _SceneAuditTable->SetSelectedComponent(_SelectedComponent);
    return Result;
}

auto
    SCkTextureDebuggerWindow::
    Build_CheckerPage()
    -> TSharedRef<SWidget>
{
    auto Gallery = SNew(SHorizontalBox);
    for (auto CheckerIndex = 0; CheckerIndex < UE_ARRAY_COUNT(ck_texture_debugger_window::Checkers); ++CheckerIndex)
    {
        const auto& Checker = ck_texture_debugger_window::Checkers[CheckerIndex];
        const auto Id = Checker.Id;
        Gallery->AddSlot().FillWidth(1).Padding(0, 0, CkStyle::SpaceS, 0)
        [SNew(SCkDebug_ToggleSurface).IsOn_Lambda([this, Id] { return _SelectedCheckerId == Id; }).AccessibleText(FText::FromString(Checker.Label)).ToolTipText_Lambda([this, Id] { return Get_CheckerLoadSummary(Id); }).OnStateChanged_Lambda([this, Id](bool InIsOn) { if (InIsOn) { OnCheckerSelected(Id); } })
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(SBox).WidthOverride(96.0f).HeightOverride(96.0f)
                    [SNew(SImage).Image(&_CheckerBrushes[CheckerIndex])]]
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Checker.Label)).Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))]
                + SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceXS, 0, 0)[SNew(STextBlock).Text_Lambda([this, Id] { return Get_CheckerLoadSummary(Id); }).Font(CkStyle::RegularFont(CkStyle::FontSizeMicro())).ColorAndOpacity(FSlateColor{CkStyle::TextMute()})]]];
    }

    _SlotBox = SNew(SVerticalBox);
    RebuildSlotControls();
    return SNew(SScrollBox)
        + SScrollBox::Slot().Padding(CkStyle::SpaceM)[Build_Section(LOCTEXT("Gallery", "Checker gallery"), Gallery)]
        + SScrollBox::Slot().Padding(CkStyle::SpaceM)[Build_Section(LOCTEXT("Target", "Target and explicit material slots"), _SlotBox.ToSharedRef())]
        + SScrollBox::Slot().Padding(CkStyle::SpaceM)[Build_Section(LOCTEXT("Apply", "Scope and reversible application"),
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, CkStyle::SpaceS, 0)[SNew(SCkDebug_ToggleSurface).IsOn_Lambda([this] { return _Scope == EScope::Selected; }).AccessibleText(LOCTEXT("SelectedScope", "Selected component scope")).ToolTipText(LOCTEXT("SelectedTip", "Apply only checked material slots on the selected component.")).OnStateChanged_Lambda([this](bool InIsOn) { constexpr auto IsLoadedWorld = false; if (InIsOn) { OnScopeChanged(IsLoadedWorld); } })[SNew(STextBlock).Text(LOCTEXT("SelectedLabel", "Selected component"))]]
                + SHorizontalBox::Slot().AutoWidth()[SNew(SCkDebug_ToggleSurface).IsOn_Lambda([this] { return _Scope == EScope::LoadedWorld; }).AccessibleText(LOCTEXT("LoadedScope", "Loaded world scope")).ToolTipText(LOCTEXT("LoadedTip", "Apply all slots on checker-capable components already loaded in the active world; no World Partition cell is loaded.")).OnStateChanged_Lambda([this](bool InIsOn) { constexpr auto IsLoadedWorld = true; if (InIsOn) { OnScopeChanged(IsLoadedWorld); } })[SNew(STextBlock).Text(LOCTEXT("LoadedLabel", "Loaded world"))]]]
            + SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceM, 0, 0)[SNew(STextBlock).Text_Lambda([this] { return Get_AuditSummary(); }).AutoWrapText(true).ColorAndOpacity(FSlateColor{CkStyle::TextMute()})]
            + SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceS, 0, 0)[SNew(SCkDebug_ToggleSurface).IsOn_Lambda([this] { return Is_ComponentWideOverrideConfirmed(); }).AccessibleText(LOCTEXT("ConfirmInstances", "Confirm component-wide instanced mesh override")).ToolTipText(LOCTEXT("ConfirmInstancesTip", "Required before applying checker to a selected ISM, HISM, or foliage component. The override affects every instance sharing that component.")).OnStateChanged_Lambda([this](bool InIsOn) { Set_ComponentWideOverrideConfirmed(InIsOn); })[SNew(STextBlock).Text(LOCTEXT("ConfirmInstancesLabel", "I understand selected instanced/foliage overrides are component-wide"))]]
            + SVerticalBox::Slot().AutoHeight().Padding(0, CkStyle::SpaceM, 0, 0)[SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, CkStyle::SpaceS, 0)[SNew(SButton).Text(LOCTEXT("ApplyChecker", "Apply checker")).OnClicked(this, &SCkTextureDebuggerWindow::OnApplyChecker)]
                + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Restore", "Restore originals")).ToolTipText(LOCTEXT("RestoreTip", "Restore values still owned by the checker and preserve material changes made externally after application.")).OnClicked(this, &SCkTextureDebuggerWindow::OnRestoreChecker)]] )];
}

auto
    SCkTextureDebuggerWindow::
    Get_StatusText() const
    -> FText
{
    return FText::FromString(_StatusDetail.IsEmpty()
        ? TEXT("Active-world inspection: no preview or QA world is created.")
        : _StatusDetail);
}

auto
    SCkTextureDebuggerWindow::
    Get_ActivePageIndex() const
    -> int32
{
    return ck_texture_debugger_window::Get_PageIndex(_ActivePageId);
}

auto
    SCkTextureDebuggerWindow::
    OnPageSelected(
        FName InId)
    -> void
{
    _ActivePageId = InId;
}

auto
    SCkTextureDebuggerWindow::
    OnModelWorldChanged(
        UWorld* InWorld)
    -> void
{
    if (InWorld == nullptr)
    {
        if (_ComponentPicker.IsValid()) { _ComponentPicker->Deactivate(); }
        if (_OverrideSession.HasActiveSession() || _OverrideSession.IsSuspendedForWorldSave()) { _OverrideSession.ReleaseForWorldCleanup(_OverrideSession.GetWorld()); }
        Reset_ComponentWideOverrideConfirmation();
        _Snapshot = {}; _SelectedComponent.Reset(); _SelectedTexture.Reset(); _SelectedSlots.Reset();
        if (_SceneAuditTable.IsValid()) { _SceneAuditTable->SetSnapshot(_Snapshot); _SceneAuditTable->SetSelectedComponent({}); }
        if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Set_Snapshot(_Snapshot); }
        _StatusDetail = TEXT("Active world ended; picker and checker session were released without touching teardown state.");
        RebuildSlotControls();
        return;
    }
    OnWorldChanged();
}

auto
    SCkTextureDebuggerWindow::
    OnWorldChanged()
    -> void
{
    if (_ComponentPicker.IsValid()) { _ComponentPicker->Deactivate(); }
    if (_OverrideSession.HasActiveSession()) { _StatusDetail = RemoveCheckerFromLiveWorld(); }
    else if (_OverrideSession.IsSuspendedForWorldSave())
    {
        _OverrideSession.ReleaseForWorldCleanup(_OverrideSession.GetWorld());
        _StatusDetail = TEXT("Suspended checker session released while changing worlds; originals remain restored.");
    }
    Reset_ComponentWideOverrideConfirmation();
    _SelectedComponent.Reset(); _SelectedTexture.Reset(); _SelectedSlots.Reset(); RefreshSnapshot(); RebuildSlotControls();
}

auto
    SCkTextureDebuggerWindow::
    OnWorldCleanup(
        UWorld* World,
        bool,
        bool)
    -> void
{
    const auto IsSelectedWorldCleanup = _Snapshot.World.Get() == World
        || (_WorldModel.IsValid() && _WorldModel->Get_SelectedWorld() == World);
    if (IsSelectedWorldCleanup && _ComponentPicker.IsValid())
    { _ComponentPicker->Deactivate(); }
    _OverrideSession.ReleaseForWorldCleanup(World);
    if (_Snapshot.World.Get() == World) { _Snapshot = {}; _SelectedComponent.Reset(); _SelectedTexture.Reset(); _SelectedSlots.Reset(); Reset_ComponentWideOverrideConfirmation(); _StatusDetail = TEXT("Active world ended; picker and checker session were released."); if (_SceneAuditTable.IsValid()) { _SceneAuditTable->SetSnapshot(_Snapshot); _SceneAuditTable->SetSelectedComponent({}); } if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Set_Snapshot(_Snapshot); } RebuildSlotControls(); }
}

auto
    SCkTextureDebuggerWindow::
    OnSessionInvalidated()
    -> void
{
    const auto* SelectedWorld = _WorldModel.IsValid() ? _WorldModel->Get_SelectedWorld() : nullptr;
    if (SelectedWorld != nullptr && SelectedWorld->WorldType == EWorldType::Editor)
    {
        _StatusDetail = TEXT("PIE session changed; the persistent Editor-world inspection remains active.");
        RefreshSnapshot();
        return;
    }

    if (_ComponentPicker.IsValid()) { _ComponentPicker->Deactivate(); }
    if (_OverrideSession.HasActiveSession()) { _StatusDetail = RemoveCheckerFromLiveWorld(); }
    else if (_OverrideSession.IsSuspendedForWorldSave())
    {
        _OverrideSession.ReleaseForWorldCleanup(_OverrideSession.GetWorld());
        _StatusDetail = TEXT("Suspended checker session released after debugger invalidation; originals remain restored.");
    }
    _SelectedComponent.Reset(); _SelectedTexture.Reset(); _SelectedSlots.Reset(); Reset_ComponentWideOverrideConfirmation(); _Snapshot = {}; if (_SceneAuditTable.IsValid()) { _SceneAuditTable->SetSnapshot(_Snapshot); _SceneAuditTable->SetSelectedComponent({}); } if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Set_Snapshot(_Snapshot); } RebuildSlotControls();
}

auto
    SCkTextureDebuggerWindow::
    RemoveCheckerFromLiveWorld()
    -> FString
{
    if (NOT _OverrideSession.HasActiveSession())
    { return {}; }

    const auto ExactRestore = _OverrideSession.TryRestore();
    if (NOT _OverrideSession.HasActiveSession())
    { return ExactRestore.Detail; }

    // A newly-added component slot overlay can block exact topology restoration. Teardown still must remove every
    // checker value before dropping the ledger, so reuse the serializer-safe per-slot path and intentionally leave
    // any externally-added overlay/topology changes untouched.
    auto* World = _OverrideSession.GetWorld();
    const auto SafeRemoval = _OverrideSession.PrepareForWorldSave(World);
    if (_OverrideSession.IsSuspendedForWorldSave())
    {
        _OverrideSession.CompleteWorldSave(false);
    }

    return SafeRemoval.Succeeded()
        ? FString::Printf(TEXT("%s Checker values were removed safely; unrelated overlay/topology changes were preserved."), *ExactRestore.Detail)
        : SafeRemoval.Detail;
}

#if WITH_EDITOR
auto
    SCkTextureDebuggerWindow::
    OnPreSaveWorld(
        UWorld* World,
        FObjectPreSaveContext)
    -> void
{
    const auto Report = _OverrideSession.PrepareForWorldSave(World);
    if (NOT Report.Detail.IsEmpty()) { _StatusDetail = Report.Detail; }
}

auto
    SCkTextureDebuggerWindow::
    OnPostSaveWorld(
        UWorld* World,
        FObjectPostSaveContext Context)
    -> void
{
    if (NOT _OverrideSession.IsSuspendedForWorldSave() || _OverrideSession.GetWorld() != World)
    { return; }

    const auto Report = _OverrideSession.CompleteWorldSave(Context.SaveSucceeded());
    if (NOT Report.Detail.IsEmpty()) { _StatusDetail = Report.Detail; }
    RefreshSnapshot(); RebuildSlotControls();
}
#endif

auto
    SCkTextureDebuggerWindow::
    OnComponentPicked(
        const FCkDebug_ComponentPickResult& Pick)
    -> void
{
    auto* Component = Pick.Component.Get();
    if (Component == nullptr) { _StatusDetail = TEXT("Pick did not resolve a live component."); return; }
    Reset_ComponentWideOverrideConfirmation();
    _SelectedTexture.Reset();
    if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Clear_Selection(); }
    _SelectedComponent = Component; _SelectedSlots.Reset();
    for (auto Slot = 0; Slot < Component->GetNumMaterials(); ++Slot) { _SelectedSlots.Add(Slot); }
    _StatusDetail = Pick.UsedBoundsFallback ? TEXT("Component selected by collisionless bounds fallback. All slots are selected explicitly; collision section was not guessed as a slot.") : TEXT("Component selected. Material slots remain explicit; collision section is context only.");
    RefreshSnapshot(); RebuildSlotControls();
}

auto
    SCkTextureDebuggerWindow::
    OnAuditComponentSelected(
        TWeakObjectPtr<UPrimitiveComponent> InComponent)
    -> void
{
    auto* Component = InComponent.Get();
    const auto* Row = Component == nullptr
        ? nullptr
        : _Snapshot.Components.FindByPredicate([Component](const FCkTextureDebugger_ComponentRow& InRow)
        {
            return InRow.NavigationTarget.Get() == Component;
        });
    if (Row == nullptr)
    {
        _SelectedComponent.Reset();
        _SelectedTexture.Reset();
        _SelectedSlots.Reset();
        Reset_ComponentWideOverrideConfirmation();
        if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Clear_Selection(); }
        if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Set_Snapshot(_Snapshot); }
        _StatusDetail = Component == nullptr
            ? TEXT("Scene Audit selection cleared.")
            : TEXT("The selected audit row no longer resolves to a live component.");
        Sync_DiagnosticPages();
        RebuildSlotControls();
        return;
    }

    Reset_ComponentWideOverrideConfirmation();
    _SelectedTexture.Reset();
    if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Clear_Selection(); }
    _SelectedComponent = Component;
    _SelectedSlots.Reset();
    for (const auto& Slot : Row->MaterialSlots)
    { _SelectedSlots.Add(Slot.SlotIndex); }

    _StatusDetail = TEXT("Scene Audit component selected; all material slots are selected explicitly.");
    _SceneAuditTable->SetSelectedComponent(_SelectedComponent);
    if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Set_Snapshot(_Snapshot, _SelectedComponent); }
    Sync_DiagnosticPages();
    RebuildSlotControls();
}

auto
    SCkTextureDebuggerWindow::
    OnTextureHealthSelectionChanged(
        const FCkTextureDebugger_TextureHealthSelection& InSelection)
    -> void
{
    if (InSelection.Texture.IsValid() && NOT InSelection.DisplayName.IsEmpty())
    {
        _SelectedTexture = InSelection;
        _StatusDetail = FString::Printf(TEXT("Selected texture: %s"), *InSelection.DisplayName);
    }
    else
    {
        _SelectedTexture.Reset();
    }
    Sync_DiagnosticPages();
}

auto
    SCkTextureDebuggerWindow::
    RefreshSnapshot()
    -> void
{
    if (NOT _WorldModel.IsValid()) { _Snapshot = {}; return; }
    _Snapshot = ck::texture_debugger::collector::Collect_LoadedWorld(_WorldModel->Get_SelectedWorld());
    if (_SelectedComponent.IsValid() && Find_SelectedComponent() == nullptr) { _SelectedComponent.Reset(); _SelectedSlots.Reset(); }
    if (_SceneAuditTable.IsValid())
    {
        _SceneAuditTable->SetSnapshot(_Snapshot);
        _SceneAuditTable->SetSelectedComponent(_SelectedComponent);
    }
    if (_TextureHealthTable.IsValid())
    {
        _TextureHealthTable->Set_Snapshot(_Snapshot, _SelectedComponent);
        if (const auto CurrentTextureSelection = _TextureHealthTable->Get_Selection(); CurrentTextureSelection.IsSet())
        { _SelectedTexture = CurrentTextureSelection; }
    }
    Sync_DiagnosticPages();
}

auto
    SCkTextureDebuggerWindow::
    Sync_DiagnosticPages()
    -> void
{
    auto Component = TOptional<FCkTextureDebugger_ComponentRow>{};
    auto Slots = TArray<int32>{};

    const auto* ContextTarget = _SelectedTexture.IsSet() && _SelectedTexture->Component.IsValid()
        ? _SelectedTexture->Component.Get()
        : _SelectedComponent.Get();
    if (ContextTarget != nullptr)
    {
        if (const auto* ContextRow = _Snapshot.Components.FindByPredicate(
            [ContextTarget](const FCkTextureDebugger_ComponentRow& InRow)
            {
                return InRow.NavigationTarget.Get() == ContextTarget;
            }))
        {
            Component = *ContextRow;
        }
    }

    if (_SelectedTexture.IsSet() && _SelectedTexture->SlotIndex != INDEX_NONE)
    {
        Slots.Add(_SelectedTexture->SlotIndex);
    }
    else
    {
        Slots = Get_SelectedSlotIndices();
    }

    if (_UvDensityPage.IsValid())
    { _UvDensityPage->Set_Context(Component, _SelectedTexture, Slots); }
    if (_MaterialInputsPage.IsValid())
    { _MaterialInputsPage->Set_Context(Component, _SelectedTexture, Slots); }
    if (_SurfaceLightingPage.IsValid())
    { _SurfaceLightingPage->Set_Context(Component, _SelectedTexture, Slots); }
}

auto
    SCkTextureDebuggerWindow::
    Find_SelectedComponent() const
    -> const FCkTextureDebugger_ComponentRow*
{
    const auto* Selected = _SelectedComponent.Get();
    return Selected == nullptr ? nullptr : _Snapshot.Components.FindByPredicate([Selected](const FCkTextureDebugger_ComponentRow& Row) { return Row.NavigationTarget.Get() == Selected; });
}

auto
    SCkTextureDebuggerWindow::
    RebuildSlotControls()
    -> void
{
    if (NOT _SlotBox.IsValid()) { return; }
    _SlotBox->ClearChildren();
    _SlotBox->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] { return Get_SelectedComponentSummary(); }).AutoWrapText(true).ColorAndOpacity(FSlateColor{CkStyle::TextDim()})];
    _SlotBox->AddSlot().AutoHeight().Padding(0, CkStyle::SpaceM, 0, 0)[SNew(SButton).Text(LOCTEXT("Next", "Select next loaded mesh")).OnClicked(this, &SCkTextureDebuggerWindow::OnSelectNextComponent)];
    const auto* Row = Find_SelectedComponent();
    if (Row == nullptr) { return; }
    for (const auto& Slot : Row->MaterialSlots)
    {
        const auto SlotIndex = Slot.SlotIndex;
        _SlotBox->AddSlot().AutoHeight().Padding(0, CkStyle::SpaceXS, 0, 0)
        [SNew(SCkDebug_ToggleSurface).IsOn_Lambda([this, SlotIndex] { return _SelectedSlots.Contains(SlotIndex); }).AccessibleText(FText::FromString(FString::Printf(TEXT("Material slot %d"), SlotIndex))).ToolTipText(LOCTEXT("SlotTip", "Explicit checker target slot. It never follows collision face/section guessing.")).OnStateChanged_Lambda([this, SlotIndex](bool InIsOn) { OnSlotSelected(SlotIndex, InIsOn); })
            [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Slot %d · %s"), SlotIndex, *Slot.DisplayName)))]];
    }
}

auto
    SCkTextureDebuggerWindow::
    Get_SelectedSlotIndices() const
    -> TArray<int32>
{
    auto Result = _SelectedSlots.Array();
    Result.Sort();
    return Result;
}

auto
    SCkTextureDebuggerWindow::
    Build_Targets() const
    -> TArray<FCkTextureDebugger_CheckerTarget>
{
    auto Targets = TArray<FCkTextureDebugger_CheckerTarget>{};
    if (_Scope == EScope::Selected)
    {
        const auto* Row = Find_SelectedComponent(); auto* Component = Row != nullptr ? Cast<UMeshComponent>(Row->NavigationTarget.Get()) : nullptr; auto Slots = Get_SelectedSlotIndices();
        if (Component != nullptr && NOT Slots.IsEmpty()) { Targets.Add({Component, MoveTemp(Slots)}); }
        return Targets;
    }
    for (const auto& Row : _Snapshot.Components)
    {
        auto* Component = Cast<UMeshComponent>(Row.NavigationTarget.Get());
        if (Component == nullptr || NOT Row.SupportsCheckerOverride || Row.MaterialSlots.IsEmpty()) { continue; }
        auto Slots = TArray<int32>{}; for (const auto& Slot : Row.MaterialSlots) { Slots.Add(Slot.SlotIndex); }
        Targets.Add({Component, MoveTemp(Slots)});
    }
    return Targets;
}

auto
    SCkTextureDebuggerWindow::
    Get_ComponentWideTargetSignature() const
    -> FString
{
    auto TargetSignatures = TArray<FString>{};
    for (const auto& Target : Build_Targets())
    {
        const auto* Component = Target.Component.Get();
        if (Component == nullptr) { continue; }

        auto Slots = Target.SlotIndices;
        Slots.Sort();
        TargetSignatures.Add(FString::Printf(
            TEXT("%s[%s]"),
            *Component->GetPathName(),
            *FString::JoinBy(Slots, TEXT(","), [](int32 InSlot) { return FString::FromInt(InSlot); })));
    }
    TargetSignatures.Sort();

    const auto* World = _WorldModel.IsValid() ? _WorldModel->Get_SelectedWorld() : nullptr;
    return FString::Printf(
        TEXT("%s|%d|%s"),
        World == nullptr ? TEXT("<no-world>") : *World->GetPathName(),
        static_cast<int32>(_Scope),
        *FString::Join(TargetSignatures, TEXT("|")));
}

auto
    SCkTextureDebuggerWindow::
    Is_ComponentWideOverrideConfirmed() const
    -> bool
{
    return _ConfirmedComponentWideInstanceOverride
        && _ConfirmedComponentWideTargetSignature == Get_ComponentWideTargetSignature();
}

auto
    SCkTextureDebuggerWindow::
    Set_ComponentWideOverrideConfirmed(
        bool InConfirmed)
    -> void
{
    _ConfirmedComponentWideInstanceOverride = InConfirmed;
    _ConfirmedComponentWideTargetSignature = InConfirmed ? Get_ComponentWideTargetSignature() : FString{};
}

auto
    SCkTextureDebuggerWindow::
    Reset_ComponentWideOverrideConfirmation()
    -> void
{
    _ConfirmedComponentWideInstanceOverride = false;
    _ConfirmedComponentWideTargetSignature.Reset();
}

auto
    SCkTextureDebuggerWindow::
    TryLoadCheckerMaterial() const
    -> UMaterialInterface*
{
    return LoadObject<UMaterialInterface>(nullptr, ck_texture_debugger_window::CheckerMaterialPath);
}

auto
    SCkTextureDebuggerWindow::
    TryLoadCheckerTexture() const
    -> UTexture*
{
    const auto* Checker = ck_texture_debugger_window::FindChecker(_SelectedCheckerId);
    return Checker == nullptr ? nullptr : LoadObject<UTexture>(nullptr, Checker->TexturePath);
}

auto
    SCkTextureDebuggerWindow::
    OnCheckerSelected(
        FName InId)
    -> void
{
    if (_SelectedCheckerId == InId)
    { return; }

    const auto* Checker = ck_texture_debugger_window::FindChecker(InId);
    auto* Texture = Checker == nullptr ? nullptr : LoadObject<UTexture>(nullptr, Checker->TexturePath);
    if (Texture == nullptr)
    {
        _StatusDetail = TEXT("The selected checker texture is unavailable; the current checker was left unchanged.");
        return;
    }

    if (_OverrideSession.HasActiveSession() || _OverrideSession.IsSuspendedForWorldSave())
    {
        const auto Report = _OverrideSession.SwitchCheckerTexture(
            Texture,
            ck_texture_debugger_window::CheckerTextureParameter);
        _StatusDetail = Report.Detail;
        if (NOT Report.Succeeded())
        { return; }
    }

    _SelectedCheckerId = InId;
}

auto
    SCkTextureDebuggerWindow::
    OnScopeChanged(
        bool InLoadedWorld)
    -> void
{
    _Scope = InLoadedWorld ? EScope::LoadedWorld : EScope::Selected;
    Reset_ComponentWideOverrideConfirmation();
    Sync_DiagnosticPages();
}

auto
    SCkTextureDebuggerWindow::
    OnSlotSelected(
        int32 Slot,
        bool InSelected)
    -> void
{
    if (InSelected)
    {
        _SelectedSlots.Add(Slot);
    }
    else
    {
        _SelectedSlots.Remove(Slot);
    }

    Reset_ComponentWideOverrideConfirmation();
    Sync_DiagnosticPages();
}

auto
    SCkTextureDebuggerWindow::
    OnSelectNextComponent()
    -> FReply
{
    if (_Snapshot.Components.IsEmpty()) { _StatusDetail = TEXT("No mesh component is loaded in the active world."); return FReply::Handled(); }
    const auto Current = _Snapshot.Components.IndexOfByPredicate([this](const FCkTextureDebugger_ComponentRow& Row) { return Row.NavigationTarget.Get() == _SelectedComponent.Get(); });
    for (auto Offset = 1; Offset <= _Snapshot.Components.Num(); ++Offset)
    {
        const auto& Candidate = _Snapshot.Components[(Current + Offset + _Snapshot.Components.Num()) % _Snapshot.Components.Num()];
        if (Candidate.SupportsCheckerOverride && Candidate.NavigationTarget.IsValid())
        {
            Reset_ComponentWideOverrideConfirmation();
            _SelectedTexture.Reset();
            if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Clear_Selection(); }
            _SelectedComponent = Candidate.NavigationTarget; _SelectedSlots.Reset(); for (const auto& Slot : Candidate.MaterialSlots) { _SelectedSlots.Add(Slot.SlotIndex); }
            if (_SceneAuditTable.IsValid()) { _SceneAuditTable->SetSelectedComponent(_SelectedComponent); }
            if (_TextureHealthTable.IsValid()) { _TextureHealthTable->Set_Snapshot(_Snapshot, _SelectedComponent); }
            Sync_DiagnosticPages();
            _StatusDetail = TEXT("Selected loaded mesh component; all its material slots are explicitly selected."); RebuildSlotControls(); return FReply::Handled();
        }
    }
    _StatusDetail = TEXT("No checker-capable mesh component is loaded in the active world."); return FReply::Handled();
}

auto
    SCkTextureDebuggerWindow::
    OnApplyChecker()
    -> FReply
{
    auto* World = _WorldModel.IsValid() ? _WorldModel->Get_SelectedWorld() : nullptr; auto* Master = TryLoadCheckerMaterial(); auto* Texture = TryLoadCheckerTexture();
    const auto* SelectedRow = Find_SelectedComponent();
    const auto HasInstancedTarget = _Scope == EScope::Selected
        ? (SelectedRow != nullptr && SelectedRow->InstanceCount > 0)
        : _Snapshot.Components.ContainsByPredicate([](const FCkTextureDebugger_ComponentRow& InRow)
            { return InRow.SupportsCheckerOverride && InRow.InstanceCount > 0; });
    if (HasInstancedTarget && NOT Is_ComponentWideOverrideConfirmed())
    {
        _StatusDetail = TEXT("This scope contains ISM/HISM/foliage components. Confirm the component-wide override before applying; every instance sharing each component will change.");
        return FReply::Handled();
    }
    if (Master == nullptr || Texture == nullptr) { _StatusDetail = TEXT("Checker master or selected texture is unavailable; no material was fabricated or substituted."); return FReply::Handled(); }
    auto* Checker = UMaterialInstanceDynamic::Create(Master, GetTransientPackage());
    if (Checker == nullptr) { _StatusDetail = TEXT("Could not create a transient checker material instance; nothing changed."); return FReply::Handled(); }
    Checker->SetTextureParameterValue(ck_texture_debugger_window::CheckerTextureParameter, Texture);
    _StatusDetail = _OverrideSession.Apply(World, Checker, Build_Targets()).Detail; RefreshSnapshot(); return FReply::Handled();
}

auto
    SCkTextureDebuggerWindow::
    OnRestoreChecker()
    -> FReply
{
    _StatusDetail = _OverrideSession.TryRestore().Detail;
    RefreshSnapshot();
    return FReply::Handled();
}

auto
    SCkTextureDebuggerWindow::
    OnRefreshRequested()
    -> FReply
{
    Reset_ComponentWideOverrideConfirmation();
    RefreshSnapshot();
    _StatusDetail = TEXT("Refreshed already-loaded active-world state without loading cells or assets.");
    RebuildSlotControls();
    return FReply::Handled();
}

auto
    SCkTextureDebuggerWindow::
    Get_SelectedComponentSummary() const
    -> FText
{
    const auto* Row = Find_SelectedComponent(); if (Row == nullptr) { return LOCTEXT("NoSelection", "No component selected. Pick in PIE/game viewport or use Select next loaded mesh."); }
    const auto Instances = Row->InstanceCount > 0 ? FString::Printf(TEXT(" Component-wide across %d instance(s); foliage/ISM is never per-instance."), Row->InstanceCount) : FString{};
    const auto Overlay = Row->HasComponentSlotOverlay ? TEXT(" Checker admission will fail safely: per-slot overlay materials cannot be reconstructed through public APIs.") : TEXT("");
    return FText::FromString(FString::Printf(TEXT("%s · %s · %d slots.%s%s"), *Row->ActorDisplayName, *Row->ComponentDisplayName, Row->MaterialSlots.Num(), *Instances, Overlay));
}

auto
    SCkTextureDebuggerWindow::
    Get_SelectedSlotsSummary() const
    -> FText
{
    const auto SlotText = FString::JoinBy(
        Get_SelectedSlotIndices(),
        TEXT(", "),
        [](int32 Slot)
        {
            return FString::FromInt(Slot);
        });
    return FText::FromString(FString::Printf(TEXT("Explicit selected slots: %s"), *SlotText));
}

auto
    SCkTextureDebuggerWindow::
    Get_HealthSummary() const
    -> FText
{
    const auto* Row = Find_SelectedComponent();
    const auto Scope = Row == nullptr
        ? FString{TEXT("All loaded-world runtime textures are shown. Pick a component or select one in Scene Audit to emphasize its rows.")}
        : FString::Printf(
            TEXT("All loaded-world runtime textures remain visible; rows from %s · %s are emphasized."),
            *Row->ActorDisplayName,
            *Row->ComponentDisplayName);
    return FText::FromString(FString::Printf(
        TEXT("%s Streaming capability: %s Select a row to show exactly which texture is active, preview it, and inspect its cooked dimensions, mips, memory, and streaming state."),
        *Scope,
        *Get_StreamingAvailabilitySummary().ToString()));
}

auto
    SCkTextureDebuggerWindow::
    Get_SelectedTextureContextSummary() const
    -> FText
{
    if (NOT _SelectedTexture.IsSet())
    { return LOCTEXT("SelectedTextureNone", "Selected texture: none. Choose a row in Texture Health."); }

    const auto& Selection = _SelectedTexture.GetValue();
    return FText::FromString(FString::Printf(
        TEXT("Selected texture: %s\n%s\n%s"),
        *Selection.DisplayName,
        *Selection.TexturePath.ToString(),
        *Selection.Provenance));
}

auto
    SCkTextureDebuggerWindow::
    Get_UvDensitySummary() const
    -> FText
{
    const auto* Row = Find_SelectedComponent();
    auto* Component = Row != nullptr ? Cast<UMeshComponent>(Row->NavigationTarget.Get()) : nullptr;
    const auto Slots = Get_SelectedSlotIndices();
    if (Component == nullptr || Slots.IsEmpty())
    { return FText::FromString(Get_SelectedTextureContextSummary().ToString() + TEXT("\n\nUnavailable — select a checker-capable component and at least one explicit material slot.")); }

    const auto Result = ck::texture_debugger::uv_density::InspectComponentCapability(
        Component, Slots[0], 0, INDEX_NONE, INDEX_NONE);
    if (Result.Availability != ECkTextureDebugger_UvDensityAvailability::Available)
    {
        return FText::FromString(FString::Printf(
            TEXT("%s\n\nUnavailable for slot %d / UV0 — %s\nA number is shown only when triangle world area, triangle UV area, texture binding, and coordinate transform are all authoritative."),
            *Get_SelectedTextureContextSummary().ToString(), Slots[0], *Result.UnavailableReason));
    }

    return FText::FromString(FString::Printf(TEXT("%s\n\nSlot %d / UV0: %.3f texels/cm"), *Get_SelectedTextureContextSummary().ToString(), Slots[0], Result.TexelsPerCm));
}
auto
    SCkTextureDebuggerWindow::
    Get_MaterialInputsSummary() const
    -> FText
{
    const auto* Row = Find_SelectedComponent();
    if (Row == nullptr)
    {
        return FText::FromString(Get_SelectedTextureContextSummary().ToString()
            + TEXT("\n\nUnavailable — select a component to inspect active material inputs."));
    }

    auto Lines = TArray<FString>{Get_SelectedTextureContextSummary().ToString(), FString{}};
    for (const auto& Slot : Row->MaterialSlots)
    {
        if (NOT _SelectedSlots.IsEmpty() && NOT _SelectedSlots.Contains(Slot.SlotIndex))
        { continue; }

        const auto Analysis = ck::texture_debugger::material_analysis::Analyze(Slot.NavigationTarget.Get());
        Lines.Add(FString::Printf(TEXT("Slot %d · %s · quality %d · shader platform %d"),
            Slot.SlotIndex, *Slot.DisplayName, static_cast<int32>(Analysis.ActiveQualityLevel), static_cast<int32>(Analysis.ActiveShaderPlatform)));
        for (const auto& Input : Analysis.Rows)
        {
            const TCHAR* Provenance = TEXT("Unavailable");
            if (Input.Provenance == ECkTextureDebugger_MaterialTextureProvenance::Parameter) { Provenance = TEXT("Parameter"); }
            else if (Input.Provenance == ECkTextureDebugger_MaterialTextureProvenance::UsedTexture) { Provenance = TEXT("UsedTexture / potential"); }

            const auto Parameter = Input.ParameterInfo.Name.IsNone()
                ? FString{}
                : FString::Printf(TEXT(" parameter %s"), *Input.ParameterInfo.Name.ToString());
            const auto Detail = Input.UnavailableReason.IsEmpty() ? Input.TexturePath.ToString() : Input.UnavailableReason;
            Lines.Add(FString::Printf(TEXT("  [%s]%s %s — %s"), Provenance, *Parameter, *Input.DisplayName, *Detail));
        }
    }
    return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

auto
    SCkTextureDebuggerWindow::
    Get_SurfaceLightingSummary() const
    -> FText
{
    const auto* Row = Find_SelectedComponent(); auto* Component = Row != nullptr ? Row->NavigationTarget.Get() : nullptr;
    if (Component == nullptr) { return LOCTEXT("SurfaceNone", "Unavailable — select a live component to inspect runtime surface flags."); }
    auto Lines = TArray<FString>{FString::Printf(TEXT("Component: cast shadow %s · visible %s · %d material slots"),
        Component->CastShadow ? TEXT("Yes") : TEXT("No"), Component->IsVisible() ? TEXT("Yes") : TEXT("No"), Row->MaterialSlots.Num())};
    for (const auto& Slot : Row->MaterialSlots)
    {
        const auto Surface = ck::texture_debugger::surface_analysis::Describe(Component, Slot.NavigationTarget.Get());
        if (NOT Surface.HasMaterial)
        {
            Lines.Add(FString::Printf(TEXT("Slot %d: material unavailable"), Slot.SlotIndex));
            continue;
        }

        const auto Lightmap = Surface.LightMapResolution.IsSet()
            ? FString::Printf(TEXT("%dx%d"), Surface.LightMapResolution->X, Surface.LightMapResolution->Y)
            : TEXT("Unavailable");
        const auto Nanite = Surface.HasNaniteData.IsSet()
            ? (Surface.HasNaniteData.GetValue() ? TEXT("Yes") : TEXT("No"))
            : TEXT("Unavailable");
        Lines.Add(FString::Printf(
            TEXT("Slot %d: blend %d · two-sided %s · masked %s (clip %.3f) · translucent %s · dynamic/static shadow %s/%s · decals %s · static lighting %s · lightmap %s · Nanite %s"),
            Slot.SlotIndex, static_cast<int32>(Surface.BlendMode), Surface.IsTwoSided ? TEXT("Yes") : TEXT("No"),
            Surface.IsMasked ? TEXT("Yes") : TEXT("No"), Surface.OpacityMaskClipValue,
            Surface.IsTranslucent ? TEXT("Yes") : TEXT("No"), Surface.CastsDynamicShadow ? TEXT("Yes") : TEXT("No"),
            Surface.CastsStaticShadow ? TEXT("Yes") : TEXT("No"), Surface.ReceivesDecals ? TEXT("Yes") : TEXT("No"),
            Surface.HasStaticLighting ? TEXT("Yes") : TEXT("No"), *Lightmap, Nanite));
    }
    Lines.Add(TEXT("These are material/component facts. They do not claim a Lumen, VSM, light-leak, or blurry-texture root cause."));
    return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

auto
    SCkTextureDebuggerWindow::
    Get_AuditSummary() const
    -> FText
{
    auto Capable = 0; auto OverlayBlocked = 0; auto Instanced = 0; auto TextureRows = 0;
    for (const auto& Component : _Snapshot.Components) { if (Component.SupportsCheckerOverride) { ++Capable; } if (Component.HasComponentSlotOverlay) { ++OverlayBlocked; } if (Component.InstanceCount > 0) { ++Instanced; } for (const auto& Slot : Component.MaterialSlots) { TextureRows += Slot.Textures.Num(); } }
    return FText::FromString(FString::Printf(TEXT("Loaded active-world audit: %d primitive components; %d checker-capable; %d instanced/foliage component-wide targets; %d safely blocked by slot overlays; %d runtime-used texture rows. No World Partition cell is loaded."), _Snapshot.Components.Num(), Capable, Instanced, OverlayBlocked, TextureRows));
}

auto
    SCkTextureDebuggerWindow::
    Get_CheckerLoadSummary(
        FName Id) const
    -> FText
{
    const auto* Checker = ck_texture_debugger_window::FindChecker(Id); const auto* Texture = Checker != nullptr ? LoadObject<UTexture>(nullptr, Checker->TexturePath) : nullptr;
    return Texture != nullptr ? FText::FromString(FString::Printf(TEXT("Loaded · %dx%d"), static_cast<int32>(Texture->GetSurfaceWidth()), static_cast<int32>(Texture->GetSurfaceHeight()))) : LOCTEXT("CheckerMissing", "Unavailable - cooked asset absent");
}

auto
    SCkTextureDebuggerWindow::
    Get_StreamingAvailabilitySummary() const
    -> FText
{
    return FText::FromString(ck_texture_debugger_window::StreamingLabel(_Snapshot.StreamingAvailability));
}

#undef LOCTEXT_NAMESPACE
