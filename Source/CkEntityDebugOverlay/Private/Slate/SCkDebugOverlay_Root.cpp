// Implements the viewport root widget for the on-screen entity debug overlay.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"
#include "Rendering/SlateRenderTransform.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

// ====================================================================================================================

namespace OverlayRoot_Constants
{
    // Pixel margin between the focus card and the viewport edge it anchors to.
    constexpr float FocusCardMargin = 8.0f;
    constexpr double AuthoredPollIntervalSeconds = 0.5;
}

namespace ck_debugoverlay_root
{
    // Dark ink for text sitting on a saturated provider fill — same reading as the focus card's.
    constexpr auto ChipInk = FLinearColor{ 0.04f, 0.07f, 0.10f, 1.0f };

    // Plate text sits in the WORLD, not on the card, so it deliberately does NOT compose the
    // overlay's PlateFontScale — only the suite-wide TextScale rides on the CkStyle role.
    auto Get_PlateBadgeFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); }

    auto Get_PlateHeaderFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall()); }

    // ProviderChipStyle at plate density (3px side padding, micro font). Tint is the badge
    // exactly as the near-plate shipped; the provider color is per-badge, so this cannot go
    // through the tone-keyed ck::debug_axes::Make_ProviderChip.
    auto Make_PlateBadge(
        const FText&        InText,
        const FLinearColor& InProviderColor) -> TSharedRef<SWidget>
    {
        const auto Make_Pill = [&](const FLinearColor& InFill, const FLinearColor& InInk) -> TSharedRef<SWidget>
        {
            return SNew(SBorder)
                .BorderImage_Static(&CkStyle::GetRoundedBrush)
                .BorderBackgroundColor(InFill)
                .VAlign(VAlign_Center)
                .Padding_Lambda([]() -> FMargin
                {
                    return ck::debug_axes::Apply_RowDensity(FMargin{ 3.0f, 0.0f });
                })
                [
                    SNew(STextBlock)
                        .Text(InText)
                        .Font_Static(&ck_debugoverlay_root::Get_PlateBadgeFont)
                        .ColorAndOpacity(InInk)
                ];
        };

        switch (UCkDebuggerStyleSettings::Get_Selection().ProviderChipStyle)
        {
            case ECkDebugAxis_ProviderChipStyle::Tint:
                return Make_Pill(InProviderColor, ChipInk);

            case ECkDebugAxis_ProviderChipStyle::Solid:
                return Make_Pill(InProviderColor, CkStyle::TextStrong());

            // The plate badge text is ALREADY the provider abbreviation, so this option drops the
            // pill rather than truncating a second time.
            case ECkDebugAxis_ProviderChipStyle::AbbrevOnly:
                return SNew(STextBlock)
                    .Text(InText)
                    .Font_Static(&ck_debugoverlay_root::Get_PlateBadgeFont)
                    .ColorAndOpacity(InProviderColor);
        }

        return Make_Pill(InProviderColor, ChipInk);
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Construct(const FArguments& InArgs)
    -> void
{
    SetVisibility(TAttribute<EVisibility>::CreateLambda([]() -> EVisibility
    {
        return ck::diagnostic_visibility::Is_HiddenForStreamerMode()
            ? EVisibility::Collapsed
            : EVisibility::HitTestInvisible;
    }));

    SAssignNew(_FocusCard, SCkDebugOverlay_FocusCard);
    SAssignNew(_TagCanvas, SConstraintCanvas);
    SAssignNew(_CardStrip, SVerticalBox);

    // The hints strip is the one overlay surface that is NOT rebuilt per frame (the focus card,
    // pinned cards and world tags all re-present from the subsystem's ticker every frame, so their
    // axis reads are live by repetition). Its style therefore has to be bound, or a Style Lab flip
    // never reaches it — at rest or otherwise.
    SAssignNew(_HintsBox, SBorder)
        .Visibility(EVisibility::Collapsed)
        .BorderImage_Static(&CkStyle::GetRoundedBrush)
        .BorderBackgroundColor_Lambda([]() -> FSlateColor
        {
            return FSlateColor{CkStyle::OverlayOf(CkStyle::BgRoot(), 0.82f)};
        })
        .Padding_Lambda([]() -> FMargin
        {
            return ck::debug_axes::Apply_RowDensity(FMargin{ CkStyle::SpaceS, CkStyle::SpaceXS });
        })
        [
            SAssignNew(_HintsText, STextBlock)
                .Font_Lambda([]() -> FSlateFontInfo
                {
                    return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro());
                })
                .ColorAndOpacity_Lambda([]() -> FSlateColor
                {
                    return FSlateColor{CkStyle::TextMute()};
                })
        ];

    SAssignNew(_WorldTagPort, SBox)
    [
        _TagCanvas.ToSharedRef()
    ];
    SAssignNew(_CardPort, SBox)
        .Clipping(EWidgetClipping::ClipToBounds)
        .MaxDesiredHeight_Lambda([this]() -> FOptionalSize
        {
            const auto ViewportH = GetCachedGeometry().GetLocalSize().Y;
            if (ViewportH <= KINDA_SMALL_NUMBER)
            { return FOptionalSize{}; }
            return FOptionalSize{ViewportH * _PlateMaxHeightFraction - 2.0f * OverlayRoot_Constants::FocusCardMargin};
        })
        [
            _CardStrip.ToSharedRef()
        ];
    SAssignNew(_PresentationHost, SBox);

#if WITH_DEV_AUTOMATION_TESTS
    _AuthoredMarkupPath = InArgs._AuthoredMarkupPathOverride;
    _AuthoredStylesheetPath = InArgs._AuthoredStylesheetPathOverride;
#endif

    ChildSlot
    [
        _PresentationHost.ToSharedRef()
    ];

    DoRebuildLayout();
    DoBuild_AuthoredPresentation();
}

SCkDebugOverlay_Root::~SCkDebugOverlay_Root()
{
    Release_AuthoredPresentation();
}

auto
    SCkDebugOverlay_Root::
    Release_OwnerInteractions()
    -> void
{
    Release_AuthoredPresentation();
}

auto
    SCkDebugOverlay_Root::
    Tick(
        const FGeometry& InAllottedGeometry,
        const double     InCurrentTime,
        const float      InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    DoPoll_AuthoredPresentation(InCurrentTime);
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Set_PlateLayout(
        ECk_DebugOverlay_PlateAnchor InAnchor,
        float InWidth,
        float InMaxHeightFraction)
    -> void
{
    if (_AuthoredPresentationReleased)
    { return; }

    if (InAnchor == _PlateAnchor &&
        FMath::IsNearlyEqual(InWidth, _PlateWidth) &&
        FMath::IsNearlyEqual(InMaxHeightFraction, _PlateMaxHeightFraction))
    { return; }

    _PlateAnchor            = InAnchor;
    _PlateWidth             = InWidth;
    _PlateMaxHeightFraction = InMaxHeightFraction;
    DoRebuildLayout();
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    DoRebuildLayout()
    -> void
{
    // Tell the card its inner content width so its section wrap-boxes wrap at the
    // real width (lock ring = 2px each side, card padding = SpaceM each side).
    if (_FocusCard.IsValid())
    {
        _FocusCard->Set_WrapWidth(_PlateWidth - 4.0f - 2.0f * CkStyle::SpaceM);
    }

    auto HAlign = HAlign_Right;
    auto VAlign = VAlign_Top;

    switch (_PlateAnchor)
    {
        case ECk_DebugOverlay_PlateAnchor::TopLeft:      HAlign = HAlign_Left;   VAlign = VAlign_Top;    break;
        case ECk_DebugOverlay_PlateAnchor::TopCenter:    HAlign = HAlign_Center; VAlign = VAlign_Top;    break;
        case ECk_DebugOverlay_PlateAnchor::TopRight:     HAlign = HAlign_Right;  VAlign = VAlign_Top;    break;
        case ECk_DebugOverlay_PlateAnchor::Left:         HAlign = HAlign_Left;   VAlign = VAlign_Center; break;
        case ECk_DebugOverlay_PlateAnchor::Right:        HAlign = HAlign_Right;  VAlign = VAlign_Center; break;
        case ECk_DebugOverlay_PlateAnchor::BottomLeft:   HAlign = HAlign_Left;   VAlign = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::BottomCenter: HAlign = HAlign_Center; VAlign = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::BottomRight:  HAlign = HAlign_Right;  VAlign = VAlign_Bottom; break;
    }

    auto HintsH = HAlign_Left;
    auto HintsV = VAlign_Bottom;
    Resolve_HintsAnchor(HintsH, HintsV);

    // The authored overlay owns the stable stacking order. Native ports retain only projected
    // paint and stateful cards. Two HTML hint rows use visibility for the dynamic corner.
    if (_WorldTagPort.IsValid())
    {
        _WorldTagPort->SetHAlign(HAlign_Fill);
        _WorldTagPort->SetVAlign(VAlign_Fill);
    }
    if (_CardPort.IsValid())
    {
        _CardPort->SetHAlign(HAlign);
        _CardPort->SetVAlign(VAlign);
        _CardPort->SetPadding(FMargin{OverlayRoot_Constants::FocusCardMargin});
    }
    _HintsOnLeft = HintsH == HAlign_Left;

    DoRebuild_CardStrip();
}

auto
    SCkDebugOverlay_Root::
    DoMount_NativeFallback()
    -> void
{
    if (NOT _PresentationHost.IsValid() || NOT _WorldTagPort.IsValid() || NOT _CardPort.IsValid() || NOT _HintsBox.IsValid())
    { return; }

    // Fallback is error-only. Its ordinary hint border is deliberately not an authored port;
    // normal presentation uses bound HTML rows below.
    auto HintsH = HAlign_Left;
    auto HintsV = VAlign_Top;
    Resolve_HintsAnchor(HintsH, HintsV);
    _NativeFallback = SNew(SOverlay)
        + SOverlay::Slot()[_WorldTagPort.ToSharedRef()]
        + SOverlay::Slot()[_CardPort.ToSharedRef()]
        + SOverlay::Slot().HAlign(HintsH).VAlign(HintsV).Padding(FMargin{OverlayRoot_Constants::FocusCardMargin})
        [ _HintsBox.ToSharedRef() ];
    _PresentationHost->SetContent(_NativeFallback.ToSharedRef());
}

auto
    SCkDebugOverlay_Root::
    DoBuild_AuthoredPresentation()
    -> void
{
    if (_AuthoredPresentationReleased || NOT _PresentationHost.IsValid() || NOT _WorldTagPort.IsValid() || NOT _CardPort.IsValid())
    { return; }

    if (_AuthoredMarkupPath.IsEmpty() || _AuthoredStylesheetPath.IsEmpty())
    {
        const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
        if (NOT Plugin.IsValid())
        {
            _AuthoredFailure = TEXT("Unable to locate CkDebugger authored Entity Debug Overlay resources.");
            DoMount_NativeFallback();
            return;
        }

        const auto Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
        _AuthoredMarkupPath = FPaths::Combine(Directory, TEXT("EntityDebugOverlay.ui.html"));
        _AuthoredStylesheetPath = FPaths::Combine(Directory, TEXT("EntityDebugOverlay.ui.css"));
    }

    // Detach before the candidate claims its sole native port. The view does all later
    // compatible reload commits atomically; no port is ever parented by both presentations.
    _PresentationHost->SetContent(SNullWidget::NullWidget);
    _NativeFallback.Reset();
    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("entity-debug-overlay-world-tags"), _WorldTagPort);
    NativeBindings.Add(TEXT("entity-debug-overlay-cards"), _CardPort);
    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkDebugOverlay_Root> WeakRoot = SharedThis(this);
    Data.Text.Add(TEXT("entity-debug-overlay-hints-text"), TAttribute<FText>::CreateLambda([WeakRoot]() -> FText
    {
        const TSharedPtr<SCkDebugOverlay_Root> Root = WeakRoot.Pin();
        return Root.IsValid() && Root->_HintsVisible
            ? FText::FromString(Root->_ShowFullHints ? Root->_HintsFull : Root->_HintsCompact)
            : FText::GetEmpty();
    }));
    Data.Visibility.Add(TEXT("entity-debug-overlay-hints-left-visible"), TAttribute<bool>::CreateLambda([WeakRoot]() -> bool
    {
        const TSharedPtr<SCkDebugOverlay_Root> Root = WeakRoot.Pin();
        return Root.IsValid() && Root->_HintsVisible && Root->_HintsOnLeft;
    }));
    Data.Visibility.Add(TEXT("entity-debug-overlay-hints-right-visible"), TAttribute<bool>::CreateLambda([WeakRoot]() -> bool
    {
        const TSharedPtr<SCkDebugOverlay_Root> Root = WeakRoot.Pin();
        return Root.IsValid() && Root->_HintsVisible && NOT Root->_HintsOnLeft;
    }));
    Data.CanDispatchEvents = TAttribute<bool>(false);
    const TSharedRef<FCkUiView> View = FCkUiView::Create(MoveTemp(NativeBindings), {}, {},
        CkStyle::RegularFont(CkStyle::FontSizeMicro()), MoveTemp(Data));
    View->SetFiles(_AuthoredMarkupPath, _AuthoredStylesheetPath);
    _AuthoredView = View;
    View->GetRegion(TEXT("main"));
    View->PollFiles();

    _UsingNativeFallback = NOT View->GetLastResult().Succeeded;
    _AuthoredFailure = _UsingNativeFallback ? FString::Join(View->GetLastResult().Errors, TEXT("\n")) : FString{};
    if (_UsingNativeFallback)
    { DoMount_NativeFallback(); }
    else
    { _PresentationHost->SetContent(View->GetRegion(TEXT("main"))); }
}

auto
    SCkDebugOverlay_Root::
    DoPoll_AuthoredPresentation(
        const double InCurrentTime)
    -> void
{
    if (_AuthoredPresentationReleased || NOT _AuthoredView.IsValid() || NOT _PresentationHost.IsValid() ||
        InCurrentTime < _NextAuthoredPollSeconds)
    { return; }

    _NextAuthoredPollSeconds = InCurrentTime + OverlayRoot_Constants::AuthoredPollIntervalSeconds;
    if (NOT _UsingNativeFallback)
    {
        _AuthoredView->PollFiles();
        _AuthoredFailure = _AuthoredView->GetLastResult().Succeeded
            ? FString{}
            : FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n"));
        return;
    }

    // Same-path recovery: an invalid startup candidate is retained, but its native port must
    // be detached from the fallback while PollFiles stages an accepted replacement.
    _PresentationHost->SetContent(SNullWidget::NullWidget);
    _NativeFallback.Reset();
    const auto Changed = _AuthoredView->PollFiles();
    _UsingNativeFallback = NOT _AuthoredView->GetLastResult().Succeeded;
    _AuthoredFailure = _UsingNativeFallback
        ? FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n"))
        : FString{};
    if (_UsingNativeFallback || NOT Changed)
    { DoMount_NativeFallback(); return; }

    _PresentationHost->SetContent(_AuthoredView->GetRegion(TEXT("main")));
}

auto
    SCkDebugOverlay_Root::
    Release_AuthoredPresentation()
    -> void
{
    if (_AuthoredPresentationReleased)
    { return; }
    _AuthoredPresentationReleased = true;

    if (_AuthoredView.IsValid())
    { _AuthoredView->ReleaseOwnerInteractions(); }
    if (_PresentationHost.IsValid())
    { _PresentationHost->SetContent(SNullWidget::NullWidget); }
    _AuthoredView.Reset();
    _NativeFallback.Reset();
    _WorldTagPort.Reset();
    _CardPort.Reset();
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    DoRebuild_CardStrip()
    -> void
{
    if (_AuthoredPresentationReleased || NOT _CardStrip.IsValid())
    { return; }

    _CardStrip->ClearChildren();

    const auto AddCard = [this](const TSharedPtr<SCkDebugOverlay_FocusCard>& InCard)
    {
        if (NOT InCard.IsValid())
        { return; }

        _CardStrip->AddSlot()
            .AutoHeight()
            .Padding(FMargin{ 0.0f, 0.0f, 0.0f, OverlayRoot_Constants::FocusCardMargin })
            [
                SNew(SBox)
                    .WidthOverride(_PlateWidth)
                    [
                        InCard.ToSharedRef()
                    ]
            ];
    };

    AddCard(_FocusCard);
    for (const auto& Pinned : _PinnedCards)
    { AddCard(Pinned); }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Resolve_HintsAnchor(EHorizontalAlignment& OutH, EVerticalAlignment& OutV) const
    -> void
{
    // Hints live in the TOP-LEFT by default, flipping to the TOP-RIGHT only when the focus
    // card itself is anchored top-left (so the two never share that corner).
    OutV = VAlign_Top;
    OutH = (_PlateAnchor == ECk_DebugOverlay_PlateAnchor::TopLeft) ? HAlign_Right : HAlign_Left;
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Set_FocusCardContent(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow,
        bool                                bIsLocked,
        bool                                bIsPinned,
        int32                               InCoLocatedIndex,
        int32                               InCoLocatedCount,
        const FText&                        InLayoutLabel,
        const FText&                        InSelectionSummary)
    -> void
{
    if (_AuthoredPresentationReleased)
    { return; }

    if (_FocusCard.IsValid())
    {
        _FocusCard->Set_Model(InModel, InStyle, InHistory, InNow, bIsLocked, bIsPinned,
            InCoLocatedIndex, InCoLocatedCount, InLayoutLabel, InSelectionSummary);
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Set_PinnedCards(
        const TArray<FCk_DebugOverlay_EntityModel>& InModels,
        const FCk_DebugOverlay_RenderStyle&         InStyle,
        const FCk_DebugOverlay_History&             InHistory,
        double                                      InNow)
    -> void
{
    if (_AuthoredPresentationReleased)
    { return; }

    const auto DesiredNum  = InModels.Num();
    const auto CountChanged = _PinnedCards.Num() != DesiredNum;

    // Grow / shrink the pinned-card pool (reuse existing widgets across frames).
    while (_PinnedCards.Num() < DesiredNum)
    {
        TSharedPtr<SCkDebugOverlay_FocusCard> NewCard;
        SAssignNew(NewCard, SCkDebugOverlay_FocusCard);
        NewCard->Set_WrapWidth(_PlateWidth - 4.0f - 2.0f * CkStyle::SpaceM);
        _PinnedCards.Add(NewCard);
    }
    while (_PinnedCards.Num() > DesiredNum)
    {
        _PinnedCards.Pop();
    }

    if (CountChanged)
    {
        DoRebuild_CardStrip();
    }

    constexpr auto NotLocked = false;
    constexpr auto IsPinned  = true;
    for (auto Idx = 0; Idx < DesiredNum; ++Idx)
    {
        if (_PinnedCards[Idx].IsValid())
        {
            _PinnedCards[Idx]->Set_Model(InModels[Idx], InStyle, InHistory, InNow, NotLocked, IsPinned);
        }
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Update_KeyHints(
        const FString& InCompact,
        const FString& InFull,
        bool           bShowFull,
        bool           bVisible)
    -> void
{
    if (_AuthoredPresentationReleased)
    { return; }

    _HintsCompact = InCompact;
    _HintsFull = InFull;
    _ShowFullHints = bShowFull;
    _HintsVisible = bVisible;

    // The authored rows read the retained state above directly. The native hint widgets exist only for fallback
    // presentation, so their lifetime must never gate authored visibility publication.
    if (NOT _HintsBox.IsValid() || NOT _HintsText.IsValid())
    { return; }

    if (NOT bVisible)
    {
        _HintsBox->SetVisibility(EVisibility::Collapsed);
        return;
    }

    _HintsBox->SetVisibility(EVisibility::HitTestInvisible);
    _HintsText->SetText(FText::FromString(bShowFull ? InFull : InCompact));
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Update_WorldTags(
        const TArray<FCk_DebugOverlay_WorldTagInfo>& InTags,
        double                                      InNow)
    -> void
{
    if (_AuthoredPresentationReleased || NOT _TagCanvas.IsValid())
    {
        return;
    }

    _TagCanvas->ClearChildren();
    auto SeenEntityKeys = TSet<uint32>{};
    SeenEntityKeys.Reserve(InTags.Num());
    auto UniqueTags = TArray<const FCk_DebugOverlay_WorldTagInfo*>{};
    UniqueTags.Reserve(InTags.Num());

    for (const auto& TagInfo : InTags)
    {
        if (TagInfo.EntityKey == MAX_uint32)
        { continue; }

        if (SeenEntityKeys.Contains(TagInfo.EntityKey))
        { continue; }

        SeenEntityKeys.Add(TagInfo.EntityKey);
        UniqueTags.Add(&TagInfo);
        auto& FadeState = _WorldTagFadeStates.FindOrAdd(TagInfo.EntityKey);
        if (NOT FadeState.bAdmitted)
        { continue; }

        const auto Opacity = ck_debugoverlay::Advance_WorldTagVisibilityFade(
            FadeState, TagInfo.bInRange, InNow);
        if (NOT TagInfo.bInRange && FMath::IsNearlyZero(Opacity))
        { FadeState.bAdmitted = false; }
    }

    for (auto It = _WorldTagFadeStates.CreateIterator(); It; ++It)
    {
        if (NOT SeenEntityKeys.Contains(It.Key()))
        { It.RemoveCurrent(); }
    }

    auto AdmittedCount = int32{ 0 };
    for (const auto& Pair : _WorldTagFadeStates)
    {
        if (Pair.Value.bAdmitted)
        { ++AdmittedCount; }
    }

    for (const auto* TagInfo : UniqueTags)
    {
        if (NOT TagInfo->bInRange)
        { continue; }

        auto* FadeState = _WorldTagFadeStates.Find(TagInfo->EntityKey);
        if (FadeState == nullptr || FadeState->bAdmitted ||
            AdmittedCount >= ck_debugoverlay::WorldTagPresentationBudget)
        { continue; }

        FadeState->bAdmitted = true;
        ck_debugoverlay::Advance_WorldTagVisibilityFade(*FadeState, true, InNow);
        ++AdmittedCount;
    }

    for (const auto* TagInfo : UniqueTags)
    {
        const auto* FadeState = _WorldTagFadeStates.Find(TagInfo->EntityKey);
        if (FadeState == nullptr || NOT FadeState->bAdmitted)
        { continue; }

        const auto Opacity = FadeState->CurrentOpacity;

        // SConstraintCanvas with a POINT anchor (0,0): Offset is (PosX, PosY, W, H) and,
        // with AutoSize, the child uses its own desired size (the pill hugs its text) —
        // Offset W/H are ignored. Alignment is the pivot ON the widget that lands at the
        // anchor+offset position: (0.5, 1.0) = bottom-centre, so the pill sits centred
        // directly above the entity's projected screen point.
        const auto PosX = static_cast<float>(TagInfo->ScreenPos.X);
        const auto PosY = static_cast<float>(TagInfo->ScreenPos.Y);

        auto Content = TSharedPtr<SWidget>{};
        if (TagInfo->bIsPlate)
        {
            Content = DoBuild_NearPlate(*TagInfo);
        }
        else
        {
            TSharedPtr<SCkDebugOverlay_WorldTag> WorldTag;
            SAssignNew(WorldTag, SCkDebugOverlay_WorldTag)
                .Text(TagInfo->Text);
            WorldTag->Set_Scale(TagInfo->Scale);
            Content = WorldTag;
        }

        Content->SetRenderOpacity(Opacity);

        _TagCanvas->AddSlot()
            .Anchors(FAnchors{ 0.0f, 0.0f })
            .Offset(FMargin{ PosX, PosY, 0.0f, 0.0f })
            .Alignment(FVector2D{ 0.5f, 1.0f })
            .AutoSize(true)
            [
                Content.ToSharedRef()
            ];
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Get_AdmittedWorldTagKeys() const
    -> TSet<uint32>
{
    if (_AuthoredPresentationReleased)
    { return {}; }

    auto Result = TSet<uint32>{};
    Result.Reserve(_WorldTagFadeStates.Num());
    for (const auto& Pair : _WorldTagFadeStates)
    {
        if (Pair.Value.bAdmitted)
        { Result.Add(Pair.Key); }
    }

    return Result;
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    DoBuild_NearPlate(const FCk_DebugOverlay_WorldTagInfo& InInfo)
    -> TSharedRef<SWidget>
{
    // Ultra-condensed world plate: near candidates have a name header and badge row; far
    // candidates intentionally have only the same provider-presence legend badges.
    auto BadgeRow = SNew(SWrapBox)
        .UseAllottedSize(false); // hug content — plate stays as narrow as its badges

    for (const auto& Badge : InInfo.Badges)
    {
        BadgeRow->AddSlot()
            .Padding(FMargin{ 0.0f, 0.0f, 2.0f, 0.0f })
            [
                ck_debugoverlay_root::Make_PlateBadge(FText::FromString(Badge.Text), Badge.Color)
            ];
    }

    auto Body = SNew(SVerticalBox);

    if (InInfo.bShowHeader)
    {
        Body->AddSlot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                    .Text(InInfo.Header)
                    .Font_Static(&ck_debugoverlay_root::Get_PlateHeaderFont)
                    .ColorAndOpacity(CkStyle::TextStrong())
            ];
    }

    if (InInfo.Badges.Num() > 0)
    {
        Body->AddSlot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(InInfo.bShowHeader ? FMargin{ 0.0f, 2.0f, 0.0f, 0.0f } : FMargin{})
            [
                BadgeRow
            ];
    }

    // The focus entity's plate gets a bright outer ring so it visibly matches the emphasized
    // (1.25x) diamond marker — you can always tell which plate belongs to the focused entity.
    const auto RingColor = InInfo.bIsFocus
        ? FLinearColor{ 1.0f, 1.0f, 1.0f, 1.0f }
        : FLinearColor::Transparent;

    auto Plate = SNew(SBorder)
        .Visibility(EVisibility::HitTestInvisible)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(RingColor)
        .Padding(FMargin{ InInfo.bIsFocus ? 2.0f : 0.0f })
        [
            SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::BgRoot(), 0.82f))
                .Padding(ck::debug_axes::Apply_RowDensity(FMargin{ CkStyle::SpaceS, CkStyle::SpaceXS }))
                [
                    Body
                ]
        ];

    // World plates use the same distance scale as single-line tags. The root applies the shared
    // range-transition opacity after constructing either presentation.
    Plate->SetRenderTransform(FSlateRenderTransform(FScale2D(InInfo.Scale)));
    Plate->SetRenderTransformPivot(FVector2D{ 0.5f, 0.5f });
    return Plate;
}

// ====================================================================================================================
