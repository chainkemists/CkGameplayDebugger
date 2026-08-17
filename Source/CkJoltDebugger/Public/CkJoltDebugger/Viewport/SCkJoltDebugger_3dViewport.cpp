#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkJoltDebugger/CkJoltDebugger_Log.h"
#include "CkJoltDebugger/Settings/CkJoltDebuggerSettings.h"

#include "Rendering/DrawElements.h"

auto
    ck_jolt_debugger_viewport::
    Make_InverseViewMatrix(
        const FVector& InViewOrigin,
        const FMatrix& InViewRotationMatrix)
    -> FMatrix
{
    return ck::Debug3dViewport::Make_InverseViewMatrix(InViewOrigin, InViewRotationMatrix);
}

auto
    ck_jolt_debugger_viewport::
    Select_NearestLabels(
        const TArray<FCk_Jolt_DebugDrawLabel>& InLabels,
        const FVector& InViewLocation,
        int32 InCap,
        TArray<int32>& OutIndices)
    -> void
{
    OutIndices.Reset();
    if (InCap <= 0)
    {
        return;
    }

    const auto NumLabels = InLabels.Num();
    if (NumLabels <= InCap)
    {
        OutIndices.Reserve(NumLabels);
        for (auto Index = 0; Index < NumLabels; ++Index)
        {
            OutIndices.Emplace(Index);
        }
        return;
    }

    const auto Get_DistanceSquared = [&InLabels, &InViewLocation](int32 InIndex)
    {
        return FVector::DistSquared(InLabels[InIndex].Get_WorldPosition(), InViewLocation);
    };
    const auto IsFartherFirst = [&Get_DistanceSquared](int32 InLeft, int32 InRight)
    {
        return Get_DistanceSquared(InLeft) > Get_DistanceSquared(InRight);
    };

    OutIndices.Reserve(InCap);
    for (auto Index = 0; Index < InCap; ++Index)
    {
        OutIndices.Emplace(Index);
    }
    OutIndices.Heapify(IsFartherFirst);
    for (auto Index = InCap; Index < NumLabels; ++Index)
    {
        if (Get_DistanceSquared(Index) >= Get_DistanceSquared(OutIndices.HeapTop()))
        {
            continue;
        }
        OutIndices.HeapPopDiscard(IsFartherFirst, EAllowShrinking::No);
        OutIndices.HeapPush(Index, IsFartherFirst);
    }
    OutIndices.Sort([&Get_DistanceSquared](int32 InLeft, int32 InRight)
    {
        return Get_DistanceSquared(InLeft) < Get_DistanceSquared(InRight);
    });
}

auto
    ck_jolt_debugger_viewport::
    Select_NearestLabels(
        const TArray<FCk_Jolt_DebugDrawLabel>& InLabels,
        const FVector& InViewLocation,
        int32 InCap)
    -> TArray<int32>
{
    auto Indices = TArray<int32>{};
    Select_NearestLabels(InLabels, InViewLocation, InCap, Indices);
    return Indices;
}

namespace ck_jolt_debugger_viewport
{
auto
Get_CameraPreference(ECkDebug3dCameraPreset InPreset) -> TOptional<ECkJoltDebugger_CameraPref>
{
    switch (InPreset)
    {
        case ECkDebug3dCameraPreset::Perspective: return ECkJoltDebugger_CameraPref::Perspective;
        case ECkDebug3dCameraPreset::Top:         return ECkJoltDebugger_CameraPref::Top;
        case ECkDebug3dCameraPreset::Bottom:      return ECkJoltDebugger_CameraPref::Bottom;
        case ECkDebug3dCameraPreset::Left:        return ECkJoltDebugger_CameraPref::Left;
        case ECkDebug3dCameraPreset::Right:       return ECkJoltDebugger_CameraPref::Right;
        case ECkDebug3dCameraPreset::Front:       return ECkJoltDebugger_CameraPref::Front;
        case ECkDebug3dCameraPreset::Back:        return ECkJoltDebugger_CameraPref::Back;
        default:                                  return {};
    }
}
} // namespace ck_jolt_debugger_viewport

auto
    SCkJoltDebugger_3dViewport::
    Construct(const FArguments& InArgs)
    -> void
{
    _CommonAdapter = MakeShared<FCkJoltDebugger_3dPreviewAdapter>();
    _CommonAdapter->Set_OnPick([Callback = InArgs._OnBodyPicked](TOptional<uint64> InKey, bool InAdditive)
    {
        Callback.ExecuteIfBound(InKey, InAdditive);
    });
    _CommonAdapter->Set_OnDragArm([Callback = InArgs._OnDragArm](uint64 InKey, const FVector& InPoint)
    {
        Callback.ExecuteIfBound(TOptional<uint64>{InKey}, InPoint);
    });
    _CommonAdapter->Set_OnDragRay([Callback = InArgs._OnDragRay](const FCkDebug3dCursorRay& InRay)
    {
        Callback.ExecuteIfBound(InRay._Origin, InRay._Direction);
    });
    _CommonAdapter->Set_OnDragPlaneShift([Callback = InArgs._OnDragPlaneShift](float InDirection)
    {
        Callback.ExecuteIfBound(InDirection);
    });
    _CommonAdapter->Set_OnDragRelease([Callback = InArgs._OnDragRelease]()
    {
        Callback.ExecuteIfBound();
    });
    _CommonAdapter->Set_OnHover([Callback = InArgs._OnBodyHovered](TOptional<uint64> InKey)
    {
        Callback.ExecuteIfBound(InKey);
    });
    _CommonAdapter->Set_OnCommand(
        [TogglePause = InArgs._OnTogglePause, StepOnce = InArgs._OnStepOnce,
         ToggleIsolate = InArgs._OnToggleIsolate](ECkDebug3dNeutralCommand InCommand)
        {
            switch (InCommand)
            {
            case ECkDebug3dNeutralCommand::TogglePause:
                TogglePause.ExecuteIfBound();
                break;
            case ECkDebug3dNeutralCommand::StepOnce:
                StepOnce.ExecuteIfBound();
                break;
            case ECkDebug3dNeutralCommand::ToggleIsolate:
                ToggleIsolate.ExecuteIfBound();
                break;
            }
        });

    auto CameraBookmarks = TArray<FCkDebug3dCameraBookmark>{};
    if (const auto* Settings = GetDefault<UCkJoltDebuggerSettings>(); ck::IsValid(Settings))
    {
        CameraBookmarks.Reserve(Settings->CameraBookmarks.Num());
        for (const auto& Bookmark : Settings->CameraBookmarks)
        {
            CameraBookmarks.Emplace(FCkDebug3dCameraBookmark{Bookmark.Location, Bookmark.Rotation, Bookmark.OrthoWidth,
                                                             Bookmark.IsOrthographic, Bookmark.IsSet});
        }
    }
    _CommonViewport = SNew(SCkDebug_3dPreviewViewport)
                          .Descriptor(FCkDebug3dPreviewDescriptor{})
                          .Adapter(_CommonAdapter)
                          .CameraBookmarks(MoveTemp(CameraBookmarks))
                          .OnCameraBookmarksChanged(FOnCkDebug3dBookmarksChanged::CreateLambda(
                              [](const TArray<FCkDebug3dCameraBookmark>& InBookmarks)
                              {
                                  auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
                                  if (ck::Is_NOT_Valid(Settings))
                                  {
                                      return;
                                  }
                                  Settings->CameraBookmarks.SetNum(InBookmarks.Num());
                                  for (auto Index = 0; Index < InBookmarks.Num(); ++Index)
                                  {
                                      const auto& Source = InBookmarks[Index];
                                      auto& Destination = Settings->CameraBookmarks[Index];
                                      Destination.Location = Source._Location;
                                      Destination.Rotation = Source._Rotation;
                                      Destination.OrthoWidth = Source._OrthoWidth;
                                      Destination.IsOrthographic = Source._IsOrthographic;
                                      Destination.IsSet = Source._IsSet;
                                  }
                                  Settings->SaveConfig();
                              }))
                          .OnCameraOrientationChanged(FOnCkDebug3dCameraOrientationChanged::CreateLambda(
                              [](ECkDebug3dCameraPreset InPreset)
                              {
                                  const auto Preference = ck_jolt_debugger_viewport::Get_CameraPreference(InPreset);
                                  if (NOT Preference.IsSet())
                                  {
                                      return;
                                  }
                                  auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();
                                  if (ck::Is_NOT_Valid(Settings))
                                  {
                                      return;
                                  }
                                  Settings->CameraPreset = *Preference;
                                  Settings->SaveConfig();
                              }));
    ChildSlot[_CommonViewport.ToSharedRef()];
    _LabelFont = ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall());
    SetToolTipText(TAttribute<FText>::CreateSP(this, &SCkJoltDebugger_3dViewport::Get_HoverTooltip));
}

auto
    SCkJoltDebugger_3dViewport::
    Set_PrimaryLabel(TOptional<FCkJoltDebugger_ViewportLabel> InLabel)
    -> void
{
    _PrimaryLabel = MoveTemp(InLabel);
}

auto
    SCkJoltDebugger_3dViewport::
    Set_HoverLabel(FText InText)
    -> void
{
    _HoverText = MoveTemp(InText);
}

auto
    SCkJoltDebugger_3dViewport::
    Get_CommonAdapter() const
    -> TSharedPtr<FCkJoltDebugger_3dPreviewAdapter>
{
    return _CommonAdapter;
}

auto
    SCkJoltDebugger_3dViewport::
    Get_HoverTooltip() const
    -> FText
{
    return _HoverText;
}

auto
    SCkJoltDebugger_3dViewport::
    Set_SelectionBounds(TOptional<FBox> InBounds)
    -> void
{
    if (_CommonAdapter.IsValid())
    {
        _CommonAdapter->Set_SelectionBounds(MoveTemp(InBounds));
    }
}

auto
    SCkJoltDebugger_3dViewport::
    Set_SelectionKeys(TArray<uint64> InKeys)
    -> void
{
    if (_CommonViewport.IsValid())
    {
        _CommonViewport->Set_SelectionKeys(MoveTemp(InKeys));
    }
}

auto
    SCkJoltDebugger_3dViewport::
    Set_IsolateSelection(bool InIsEnabled)
    -> void
{
    if (_CommonViewport.IsValid())
    {
        _CommonViewport->Set_IsolateSelection(InIsEnabled);
    }
}

auto
    SCkJoltDebugger_3dViewport::
    Set_DragEnabled(bool InIsEnabled)
    -> void
{
    if (_CommonAdapter.IsValid())
    {
        _CommonAdapter->Set_DragEnabled(InIsEnabled);
    }
}

auto
    SCkJoltDebugger_3dViewport::
    Set_FollowSelection(bool InIsEnabled)
    -> void
{
    if (_CommonViewport.IsValid())
    {
        _CommonViewport->Set_FollowSelection(InIsEnabled);
    }
}

auto
    SCkJoltDebugger_3dViewport::
    Get_PreviewWorld() const
    -> UWorld*
{
    return _CommonViewport.IsValid() ? _CommonViewport->Get_PreviewWorld() : nullptr;
}

auto
    SCkJoltDebugger_3dViewport::
    Get_RenderFeatures() const
    -> FCkJoltDebugger_ViewportRenderFeatures
{
    if (NOT _CommonViewport.IsValid())
    {
        return {};
    }
    const auto Features = _CommonViewport->Get_RenderFeatures();
    return FCkJoltDebugger_ViewportRenderFeatures{Features._Lighting,
                                                  Features._PostProcessing,
                                                  Features._AntiAliasing,
                                                  Features._TemporalAA,
                                                  Features._TemporalAA ? EAntiAliasingMethod::AAM_TemporalAA
                                                                       : EAntiAliasingMethod::AAM_None,
                                                  Features._DynamicShadows,
                                                  Features._MotionBlur,
                                                  Features._DepthOfField,
                                                  Features._EyeAdaptation};
}

auto
    SCkJoltDebugger_3dViewport::
    Set_Target(TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget)
    -> void
{
    _Target = InTarget;
    if (_CommonAdapter.IsValid())
    {
        _CommonAdapter->Set_Target(MoveTemp(InTarget));
    }
}

auto
    SCkJoltDebugger_3dViewport::
    ApplyPreset(ECkJoltDebugger_CameraPreset InPreset)
    -> void
{
    if (_CommonViewport.IsValid())
    {
        _CommonViewport->Apply_CameraPreset(static_cast<ECkDebug3dCameraPreset>(InPreset));
    }
}

auto
    SCkJoltDebugger_3dViewport::
    Get_ProjectionMode() const
    -> ECameraProjectionMode::Type
{
    return _CommonViewport.IsValid() ? _CommonViewport->Get_ProjectionMode() : ECameraProjectionMode::Perspective;
}

auto
    SCkJoltDebugger_3dViewport::
    Get_ViewRotation() const
    -> FRotator
{
    return _CommonViewport.IsValid() ? _CommonViewport->Get_ViewRotation() : FRotator::ZeroRotator;
}

auto
    SCkJoltDebugger_3dViewport::
    Get_ViewLocation() const
    -> FVector
{
    return _CommonViewport.IsValid() ? _CommonViewport->Get_ViewLocation() : FVector::ZeroVector;
}

auto
    SCkJoltDebugger_3dViewport::
    Get_LookAtLocation() const
    -> FVector
{
    return _CommonViewport.IsValid() ? _CommonViewport->Get_LookAtLocation() : FVector::ZeroVector;
}

auto
    SCkJoltDebugger_3dViewport::
    Input_Key(const FKey& InKey, EInputEvent InEvent)
    -> bool
{
    return _CommonViewport.IsValid() && _CommonViewport->Input_Key(InKey, InEvent);
}

auto
    SCkJoltDebugger_3dViewport::
    Input_MouseAxis(const FKey& InAxisKey, float InDelta)
    -> bool
{
    return _CommonViewport.IsValid() && _CommonViewport->Input_MouseAxis(InAxisKey, InDelta);
}

auto
    SCkJoltDebugger_3dViewport::
    Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
}

auto
    SCkJoltDebugger_3dViewport::
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const
    -> int32
{
    const auto SceneLayer = SCompoundWidget::OnPaint(InArgs, InAllottedGeometry, InCullingRect, OutDrawElements,
                                                     InLayerId, InWidgetStyle, InParentEnabled);
    const auto Target = _Target.Pin();
    const auto HasLabelWork = _PrimaryLabel.IsSet() || (Target.IsValid() && NOT Target->Get_Labels().IsEmpty());
    if (NOT HasLabelWork || NOT _CommonViewport.IsValid())
    {
        return SceneLayer;
    }

    const auto LocalSize = InAllottedGeometry.GetLocalSize();
    const auto LabelLayer = SceneLayer + 1;
    const auto PaintLabel = [&](const FVector& InWorldPosition, const FString& InText, const FLinearColor& InColor)
    {
        if (InText.IsEmpty())
        {
            return;
        }
        auto LocalPosition = FVector2D::ZeroVector;
        if (NOT _CommonViewport->TryProject_WorldToLocal(InWorldPosition, LocalSize, LocalPosition))
        {
            return;
        }
        FSlateDrawElement::MakeText(
            OutDrawElements, LabelLayer,
            InAllottedGeometry.ToPaintGeometry(FVector2f{LocalSize.X, LocalSize.Y},
                                               FSlateLayoutTransform{FVector2f{static_cast<float>(LocalPosition.X),
                                                                               static_cast<float>(LocalPosition.Y)}}),
            InText, _LabelFont, ESlateDrawEffect::None, InColor);
    };
    if (_PrimaryLabel.IsSet())
    {
        PaintLabel(_PrimaryLabel->WorldPosition, _PrimaryLabel->Text, _PrimaryLabel->Color);
    }
    if (NOT Target.IsValid())
    {
        return LabelLayer;
    }

    const auto& Labels = Target->Get_Labels();
    ck_jolt_debugger_viewport::Select_NearestLabels(Labels, _CommonViewport->Get_ViewLocation(),
                                                    ck_jolt_debugger_viewport::MaxPaintedLabels, _LabelScratch);
    if (Labels.Num() > _LabelScratch.Num() && NOT _LabelCapLogged)
    {
        _LabelCapLogged = true;
        ck::jolt_debugger::Display(
            TEXT(
                "Jolt debugger viewport is painting the {} nearest of {} labels; the rest are dropped by the hard cap"),
            _LabelScratch.Num(), Labels.Num());
    }
    for (const auto Index : _LabelScratch)
    {
        const auto& Label = Labels[Index];
        PaintLabel(Label.Get_WorldPosition(), Label.Get_Text(), Label.Get_Color());
    }
    return LabelLayer;
}
