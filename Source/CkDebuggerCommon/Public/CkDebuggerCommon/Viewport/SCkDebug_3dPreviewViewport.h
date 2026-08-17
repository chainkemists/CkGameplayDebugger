#pragma once

#include "CkDebuggerCommon/Viewport/CkDebug3dInteractionRouter.h"

#include "Camera/CameraTypes.h"
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/SViewport.h"

class FPreviewScene;
class FSceneViewport;
class FCkDebug3dPreviewViewportClient;
class SCkDebug_IconToolbar;
class UWorld;
struct FCkDebug3dCameraBookmark;

DECLARE_DELEGATE_OneParam(FOnCkDebug3dBookmarksChanged, const TArray<FCkDebug3dCameraBookmark>&);

namespace ck::Debug3dViewport
{
CKDEBUGGERCOMMON_API auto
Make_InverseViewMatrix(const FVector& InViewOrigin, const FMatrix& InViewRotationMatrix) -> FMatrix;
CKDEBUGGERCOMMON_API auto
ViewPixelsToLocalSlate(const FVector2D& InPixels, const FVector2D& InViewSize, const FVector2D& InLocalSize)
    -> FVector2D;
} // namespace ck::Debug3dViewport

enum class ECkDebug3dCameraPreset : uint8
{
    Perspective,
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back,
    FrameAll,
    FrameSelection
};
DECLARE_DELEGATE_OneParam(FOnCkDebug3dCameraOrientationChanged, ECkDebug3dCameraPreset);
enum class ECkDebug3dFrameTarget : uint8
{
    All,
    Selection
};
enum class ECkDebug3dRenderMode : uint8
{
    None,
    TransparentOnly,
    All
};
enum class ECkDebug3dGesturePolicy : uint8
{
    JoltUnrealUnion
};
enum class ECkDebug3dViewportCapability : uint16
{
    None = 0,
    Labels = 1 << 0,
    DirectionGlyphScale = 1 << 1,
    FollowSelection = 1 << 2,
    IsolateSelection = 1 << 3,
    FrameSelection = 1 << 4
};
ENUM_CLASS_FLAGS(ECkDebug3dViewportCapability)

struct FCkDebug3dCameraState
{
    FVector _Location = FVector::ZeroVector;
    FRotator _Rotation = FRotator::ZeroRotator;
    FVector _LookAt = FVector::ZeroVector;
    float _OrthoWidth = 512.0f;
    ECameraProjectionMode::Type _ProjectionMode = ECameraProjectionMode::Perspective;
    auto
    Equals(const FCkDebug3dCameraState& InOther) const -> bool;
};
struct FCkDebug3dCameraBookmark
{
    FVector _Location = FVector::ZeroVector;
    FRotator _Rotation = FRotator::ZeroRotator;
    float _OrthoWidth = 512.0f;
    bool _IsOrthographic = false;
    bool _IsSet = false;
};
struct FCkDebug3dPreviewScenePolicy
{
    bool _DefaultLighting = true;
    float _SkyBrightness = 1.0f;
    float _LightBrightness = UE_PI;
    bool _Lighting = true;
    bool _PostProcessing = true;
    bool _AntiAliasing = true;
    bool _TemporalAA = true;
    bool _DynamicShadows = false;
    bool _MotionBlur = false;
    bool _DepthOfField = false;
    bool _EyeAdaptation = false;
};
struct FCkDebug3dViewportRenderFeatures
{
    bool _HasPhysicsScene = false;
    bool _IsEditorScene = false;
    bool _Lighting = false;
    bool _PostProcessing = false;
    bool _AntiAliasing = false;
    bool _TemporalAA = false;
    bool _DynamicShadows = false;
    bool _MotionBlur = false;
    bool _DepthOfField = false;
    bool _EyeAdaptation = false;
};
struct FCkDebug3dPreviewDescriptor
{
    FCkDebug3dPreviewScenePolicy _PreviewPolicy;
    ECkDebug3dGesturePolicy _GesturePolicy = ECkDebug3dGesturePolicy::JoltUnrealUnion;
    bool _ShowOrientationCube = true;
};

class CKDEBUGGERCOMMON_API ICkDebug3dPreviewAdapter : public ICkDebug3dInteractionAdapter
{
  public:
    virtual ~ICkDebug3dPreviewAdapter() = default;
    virtual auto
    TryHit(const FCkDebug3dCursorRay&) -> TOptional<FCkDebug3dInteractionHit> override
    {
        return {};
    }
    virtual auto
    Select(uint64, bool) -> void override
    {
    }
    virtual auto
    CanArmDrag(const FCkDebug3dInteractionHit&) const -> bool override
    {
        return false;
    }
    virtual auto
    Get_FrameBounds(ECkDebug3dFrameTarget InTarget) const -> FBox = 0;
    virtual auto
    Get_SelectionCenter() const -> TOptional<FVector> = 0;
    virtual auto
    Get_Capabilities() const -> ECkDebug3dViewportCapability = 0;
    virtual auto
    On_Pick(const FCkDebug3dCursorRay& InRay) -> void = 0;
    virtual auto
    Get_RenderMode() const -> ECkDebug3dRenderMode
    {
        return ECkDebug3dRenderMode::None;
    }
    virtual auto
    Set_RenderMode(ECkDebug3dRenderMode) -> void
    {
    }
    virtual auto
    Get_ShowGrid() const -> bool
    {
        return false;
    }
    virtual auto
    Set_ShowGrid(bool) -> void
    {
    }
    virtual auto
    Get_ShowLabels() const -> bool
    {
        return false;
    }
    virtual auto
    Set_ShowLabels(bool) -> void
    {
    }
    virtual auto
    Get_DirectionGlyphScale() const -> float
    {
        return 1.0f;
    }
    virtual auto
    Set_DirectionGlyphScale(float) -> void
    {
    }
    virtual auto
    Set_IsolatedKeys(const TArray<uint64>&) -> void
    {
    }
    virtual auto
    On_ViewportTeardown() -> void = 0;
};

class CKDEBUGGERCOMMON_API SCkDebug_3dPreviewViewport final : public SViewport
{
  public:
    SLATE_BEGIN_ARGS(SCkDebug_3dPreviewViewport)
    {
    }
    SLATE_ARGUMENT(FCkDebug3dPreviewDescriptor, Descriptor)
    SLATE_ARGUMENT(TSharedPtr<ICkDebug3dPreviewAdapter>, Adapter)
    SLATE_ARGUMENT(TArray<FCkDebug3dCameraBookmark>, CameraBookmarks)
    SLATE_EVENT(FOnCkDebug3dBookmarksChanged, OnCameraBookmarksChanged)
    SLATE_EVENT(FOnCkDebug3dCameraOrientationChanged, OnCameraOrientationChanged)
    SLATE_END_ARGS()

    virtual ~SCkDebug_3dPreviewViewport() override;
    auto
    Construct(const FArguments& InArgs) -> void;
    auto
    Teardown() -> void;
    auto
    Get_PreviewWorld() const -> UWorld*;
    auto
    Get_RenderFeatures() const -> FCkDebug3dViewportRenderFeatures;
    auto
    Apply_CameraPreset(ECkDebug3dCameraPreset InPreset) -> void;
    auto
    Get_ProjectionMode() const -> ECameraProjectionMode::Type;
    auto
    Get_ViewLocation() const -> FVector;
    auto
    Get_ViewRotation() const -> FRotator;
    auto
    Get_LookAtLocation() const -> FVector;
    auto
    Get_CameraState() const -> FCkDebug3dCameraState;
    auto
    Get_CameraSpeed() const -> float;
    auto
    Get_EffectivePanScale() const -> float;
    auto
    TryProject_WorldToLocal(const FVector& InWorldPosition, const FVector2D& InLocalSize,
                            FVector2D& OutLocalPosition) const -> bool;
    auto
    Input_Key(const FKey& InKey, EInputEvent InEvent) -> bool;
    auto
    Input_MouseAxis(const FKey& InAxisKey, float InDelta) -> bool;
    auto
    Handle_FocusLost() -> void;
    auto
    Get_HasActiveCameraGesture() const -> bool;
    auto
    Get_ShowOrientationCube() const -> bool;
    auto
    Store_CameraBookmark(int32 InSlot) -> void;
    auto
    Recall_CameraBookmark(int32 InSlot) -> void;
    auto
    Get_CameraBookmarks() const -> const TArray<FCkDebug3dCameraBookmark>&;
    /** Replaces the live dense bookmark set from persisted feature state; import
     * never emits the store callback. */
    auto
    Import_CameraBookmarks(TArray<FCkDebug3dCameraBookmark> InBookmarks) -> void;
    auto
    Set_FollowSelection(bool InIsEnabled) -> void;
    auto
    Tick_FollowSelection() -> void;
    auto
    Set_IsolateSelection(bool InIsEnabled) -> void;
    auto
    Set_SelectionKeys(TArray<uint64> InKeys) -> void;
    auto
    Get_IsolatedKeys() const -> const TArray<uint64>&;
    auto
    Build_CommonControlDescriptors() const -> TArray<FName>;
    auto
    Invoke_CommonControl(FName InControlId) -> bool;
    auto
    Get_RenderModeOptions() const -> TArray<ECkDebug3dRenderMode>;
    auto
    Get_CommonControlsWidget() const -> TSharedPtr<SWidget>;
    virtual auto
    Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

  private:
    auto
    Get_Capabilities() const -> ECkDebug3dViewportCapability;
    TSharedPtr<FPreviewScene> _PreviewScene;
    TSharedPtr<FCkDebug3dPreviewViewportClient> _ViewportClient;
    TSharedPtr<FSceneViewport> _SceneViewport;
    TWeakPtr<ICkDebug3dPreviewAdapter> _Adapter;
    FCkDebug3dPreviewDescriptor _Descriptor;
    TArray<FCkDebug3dCameraBookmark> _CameraBookmarks;
    FOnCkDebug3dBookmarksChanged _OnCameraBookmarksChanged;
    FOnCkDebug3dCameraOrientationChanged _OnCameraOrientationChanged;
    TArray<uint64> _SelectionKeys;
    TArray<uint64> _IsolatedKeys;
    TSharedPtr<SWidget> _CommonControls;
    bool _FollowSelection = false;
    bool _IsolateSelection = false;
    bool _TornDown = false;
    bool _ShowOrientationCube = true;
};
