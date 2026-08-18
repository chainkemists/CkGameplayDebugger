#include "CkSmDebugger/Graph/SCkSmRuntimeGraph.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

struct FCkSmRuntimeGraphCardPresentation
{
    FCkSmRuntimeGraphNode Node;
    bool bSelected = false;
};

namespace ck_sm_runtime_graph
{
    constexpr auto CompoundCornerRadius = 8.0f;

    auto GetCompoundBorderBrush() -> const FSlateBrush*
    {
        static auto Standard = FSlateRoundedBoxBrush(FLinearColor::White, CompoundCornerRadius);
        static auto Tight = FSlateRoundedBoxBrush(FLinearColor::White, CompoundCornerRadius * 0.5f);
        return ck_sm_debugger_axes::Get_NodeRadiusScale() < 1.0f ? &Tight : &Standard;
    }

    auto GetCompoundFillBrush() -> const FSlateBrush*
    {
        static auto Standard = FSlateRoundedBoxBrush(FLinearColor::White,
                                                     CompoundCornerRadius - 1.0f);
        static auto Tight = FSlateRoundedBoxBrush(FLinearColor::White,
                                                  (CompoundCornerRadius - 1.0f) * 0.5f);
        return ck_sm_debugger_axes::Get_NodeRadiusScale() < 1.0f ? &Tight : &Standard;
    }

    auto GetCardStructureHash(const FCkSmRuntimeGraphNode& InNode) -> uint32
    {
        auto Hash = GetTypeHash(InNode.Kind);
        Hash = HashCombine(Hash, GetTypeHash(InNode.bExpandTasks));
        if (InNode.State && InNode.bExpandTasks)
        {
            for (const auto& Task : InNode.State->Tasks)
            {
                Hash = HashCombine(Hash, GetTypeHash(Task.ClassName));
            }
        }
        if (InNode.Transition)
        {
            for (const auto& Condition : InNode.Transition->Conditions)
            {
                Hash = HashCombine(Hash, GetTypeHash(Condition.ClassName));
            }
        }
        return Hash;
    }

    auto IsInlineBreakpointStyle(const int32 InBreakpointStyle) -> bool
    {
        return InBreakpointStyle == 22 || InBreakpointStyle == 23;
    }

    auto DrawSquare(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geo,
                    const FVector2f Pos, const float Size, const FLinearColor& Color) -> void
    {
        const auto Child = Geo.MakeChild(FVector2f(Size, Size), FSlateLayoutTransform(Pos));
        FSlateDrawElement::MakeBox(Out, Layer, Child.ToPaintGeometry(),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Color);
    }

    auto DrawRect(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geo,
                  const FVector2f Pos, const FVector2f Size, const FLinearColor& Color) -> void
    {
        const auto Child = Geo.MakeChild(Size, FSlateLayoutTransform(Pos));
        FSlateDrawElement::MakeBox(Out, Layer, Child.ToPaintGeometry(),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Color);
    }

    auto DrawCircle(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geo,
                    const FVector2f Pos, const float Size, const FLinearColor& Color) -> void
    {
        static const auto Brush = FSlateRoundedBoxBrush(FLinearColor::White, 999.0f);
        const auto Child = Geo.MakeChild(FVector2f(Size, Size), FSlateLayoutTransform(Pos));
        FSlateDrawElement::MakeBox(Out, Layer, Child.ToPaintGeometry(), &Brush,
            ESlateDrawEffect::None, Color);
    }

    auto DrawDiamond(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geo,
                     const FVector2f Center, const float Size, const FLinearColor& Color) -> void
    {
        const auto Half = Size * 0.5f;
        const auto Child = Geo.MakeChild(FVector2f(Size, Size),
            FSlateLayoutTransform(Center - FVector2f(Half, Half)));
        FSlateDrawElement::MakeRotatedBox(Out, Layer, Child.ToPaintGeometry(),
            FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, PI / 4.0f,
            TOptional<FVector2D>(), FSlateDrawElement::RelativeToElement, Color);
    }

    auto DrawHollowCircle(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geo,
                          const FVector2f Pos, const float Size, const FLinearColor& Color,
                          const FLinearColor& Background) -> void
    {
        DrawCircle(Out, Layer, Geo, Pos, Size, Color);
        DrawCircle(Out, Layer + 1, Geo, Pos + FVector2f(2.0f, 2.0f), Size - 4.0f, Background);
    }

    auto DrawHollowSquare(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geo,
                          const FVector2f Pos, const float Size, const FLinearColor& Color,
                          const FLinearColor& Background) -> void
    {
        DrawSquare(Out, Layer, Geo, Pos, Size, Color);
        DrawSquare(Out, Layer + 1, Geo, Pos + FVector2f(2.0f, 2.0f), Size - 4.0f, Background);
    }

    class SCkSmRuntimeBreakpointOverlay final : public SLeafWidget
    {
      public:
        SLATE_BEGIN_ARGS(SCkSmRuntimeBreakpointOverlay) {}
            SLATE_ARGUMENT(TWeakPtr<FCkSmRuntimeGraphCardPresentation>, Presentation)
            SLATE_ARGUMENT(int32, Style)
            SLATE_ARGUMENT(bool, bTransition)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            _Presentation = Args._Presentation;
            _Style = Args._Style;
            _bTransition = Args._bTransition;
        }

        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

        virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&,
                              FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&,
                              bool) const override
        {
            const auto Presentation = _Presentation.Pin();
            if (NOT Presentation)
            {
                return Layer;
            }
            return _bTransition ? PaintTransition(*Presentation, Geo, Out, Layer)
                                : PaintState(*Presentation, Geo, Out, Layer);
        }

      private:
        auto PaintTransition(const FCkSmRuntimeGraphCardPresentation& P, const FGeometry& Geo,
                             FSlateWindowElementList& Out, const int32 Layer) const -> int32
        {
            const auto bSet = P.Node.Transition && P.Node.Transition->HasBreakpoint;
            const auto Red = CkStyle::Err();
            const auto Hollow = CkStyle::OverlayOf(Red, 0.25f);
            const auto Size = ck_sm_debugger_axes::Get_IndicatorSize();
            const auto Extent = Geo.GetLocalSize();
            const auto Square = [&](const FVector2f Pos, const bool bDiamond, const FLinearColor& Color)
            {
                if (bDiamond) DrawDiamond(Out, Layer + 1, Geo, Pos + FVector2f(Size * 0.5f), Size, Color);
                else DrawSquare(Out, Layer + 1, Geo, Pos, Size, Color);
            };
            switch (_Style)
            {
            case 0: if (bSet) Square(FVector2f(Extent.X - Size, 0), false, Red); break;
            case 1: if (bSet) Square(FVector2f::ZeroVector, false, Red); break;
            case 2: if (bSet) Square(FVector2f(Extent.X - Size, 0), true, Red); break;
            case 3: if (bSet) Square(FVector2f::ZeroVector, true, Red); break;
            case 4: Square(FVector2f(Extent.X - Size, 0), true, bSet ? Red : Hollow); break;
            case 5: Square(FVector2f::ZeroVector, true, bSet ? Red : Hollow); break;
            case 6: Square(FVector2f(Extent.X - Size, Extent.Y - Size), true, bSet ? Red : Hollow); break;
            case 7: Square(FVector2f(Extent.X - Size, 0), false, bSet ? Red : Hollow); break;
            case 8:
                if (bSet) DrawSquare(Out, Layer + 1, Geo, FVector2f::ZeroVector,
                                     FMath::Min(Extent.X, Extent.Y),
                                     CkStyle::OverlayOf(Red, FMath::Clamp(0.35f * ck_sm_debugger_axes::Get_NodeDimScale(), 0.05f, 1.0f)));
                break;
            default: break;
            }
            return Layer + 2;
        }

        auto PaintState(const FCkSmRuntimeGraphCardPresentation& P, const FGeometry& Geo,
                        FSlateWindowElementList& Out, const int32 Layer) const -> int32
        {
            if (NOT P.Node.State || _Style < 0 || _Style > 21)
            {
                return Layer;
            }
            const auto bEntry = P.Node.State->HasEntryBreakpoint;
            const auto bExit = P.Node.State->HasExitBreakpoint;
            if (NOT bEntry && NOT bExit) return Layer;
            const auto Red = CkStyle::Err();
            const auto Bg = CkStyle::NodeFill_Inactive();
            const auto Faded = CkStyle::OverlayOf(Red, 0.4f);
            const auto Extent = Geo.GetLocalSize(); const float W = Extent.X, H = Extent.Y, CY = H * .5f;
            constexpr float S=10, O=12, G=2, Pad=4;
            const auto C = [&](FVector2f Pos, float Z, FLinearColor X) { DrawCircle(Out, Layer + 1, Geo, Pos, Z, X); };
            const auto HC = [&](FVector2f Pos, float Z) { DrawHollowCircle(Out, Layer + 1, Geo, Pos, Z, Red, Bg); };
            const auto SQ = [&](FVector2f Pos, float Z, FLinearColor X) { DrawSquare(Out, Layer + 1, Geo, Pos, Z, X); };
            const auto HS = [&](FVector2f Pos, float Z) { DrawHollowSquare(Out, Layer + 1, Geo, Pos, Z, Red, Bg); };
            switch (_Style)
            {
            case 0: { float X=W-S+Pad; if(bEntry){C({X,-S*.5f},S,Red);X-=S+G;} if(bExit)HC({X,-O*.5f},O); } break;
            case 1: { float X=-Pad; if(bEntry){C({X,-S*.5f},S,Red);X+=S+G;} if(bExit)HC({X,-O*.5f},O); } break;
            case 2: { float X=W-S+Pad; if(bEntry){C({X,H-S*.5f},S,Red);X-=S+G;} if(bExit)HC({X,H-O*.5f},O); } break;
            case 3: { float X=-Pad; if(bEntry){C({X,H-S*.5f},S,Red);X+=S+G;} if(bExit)HC({X,H-O*.5f},O); } break;
            case 4: { float X=W+Pad; if(bEntry){C({X,CY-S*.5f},S,Red);X+=S+G;} if(bExit)HC({X,CY-O*.5f},O); } break;
            case 5: { float X=-(S+Pad); if(bExit){X-=O+G;HC({X+O+G,CY-O*.5f},O);} if(bEntry)C({X,CY-S*.5f},S,Red); } break;
            case 6: { float X=(W-(bEntry?S:0)-(bExit?O:0)-((bEntry&&bExit)?G:0))*.5f; if(bEntry){C({X,-(S+Pad)},S,Red);X+=S+G;}if(bExit)HC({X,-(O+Pad)},O); } break;
            case 7: { float X=(W-(bEntry?S:0)-(bExit?O:0)-((bEntry&&bExit)?G:0))*.5f; if(bEntry){C({X,H+Pad},S,Red);X+=S+G;}if(bExit)HC({X,H+Pad},O); } break;
            case 8: case 9: { float X=_Style==8?W+Pad:-(FMath::Max(S,O)+Pad); float Y=CY-((bEntry&&bExit)?S+G*.5f:S*.5f); if(bEntry){C({X,Y},S,Red);Y+=S+G;}if(bExit)HC({X,Y},O); } break;
            case 10: case 11:
            {
                const float X = _Style == 10 ? -6.0f : W + 3.0f;
                if (bEntry) DrawRect(Out, Layer + 1, Geo, FVector2f(X, 0), FVector2f(3, H), Red);
                if (bExit) for (int32 I = 0; I < 3; ++I)
                {
                    const float BarX = _Style == 10 ? (bEntry ? -10.0f : -6.0f) : W + (bEntry ? 7.0f : 3.0f);
                    const float SegmentHeight = (H - 4.0f) / 3.0f;
                    DrawRect(Out, Layer + 1, Geo, FVector2f(BarX, 1.0f + I * SegmentHeight),
                        FVector2f(3.0f, SegmentHeight - 2.0f), Red);
                }
            } break;
            case 12: case 13:
            {
                const float Y = _Style == 12 ? -5.0f : H + 2.0f, Half = W * .5f;
                if (bEntry) DrawRect(Out, Layer + 1, Geo, FVector2f(0, Y), FVector2f(Half, 3), Red);
                if (bExit) DrawRect(Out, Layer + 1, Geo, FVector2f(Half, Y), FVector2f(Half, 3), Faded);
            } break;
            case 14: { float X=W-S+Pad; if(bEntry){SQ({X,-S*.5f},S,Red);X-=S+G;}if(bExit)HS({X,-O*.5f},O); } break;
            case 15: { float X=W+Pad; if(bEntry){SQ({X,CY-S*.5f},S,Red);X+=S+G;}if(bExit)HS({X,CY-O*.5f},O); } break;
            case 16: { float X=W-2; if(bEntry){DrawDiamond(Out,Layer+1,Geo,{X,-2},S,Red);X-=S+G+2;}if(bExit)DrawDiamond(Out,Layer+1,Geo,{X,-2},S,Faded); } break;
            case 17: { float X=W+Pad+S*.5f; if(bEntry){DrawDiamond(Out,Layer+1,Geo,{X,CY},S,Red);X+=S+G;}if(bExit)DrawDiamond(Out,Layer+1,Geo,{X,CY},S,Faded); } break;
            case 18: if(bEntry){SQ({-10,CY-4},8,Red);SQ({-6,CY-2},4,Red);}if(bExit){SQ({W+2,CY-4},8,Red);SQ({W+2,CY-2},4,Red);} break;
            case 19:
                if (bEntry) DrawRect(Out, Layer + 1, Geo, FVector2f(0, H + 3), FVector2f(W, 2), Red);
                if (bExit) for (float X = 0; X < W; X += 9)
                    DrawRect(Out, Layer + 1, Geo, FVector2f(X, bEntry ? H + 7 : H + 3), FVector2f(6, 2), Red);
                break;
            case 20: case 21: { const float Y=_Style==20?-6:H-8; if(bExit)HC({W-4,Y},14);if(bEntry)C({W-1,Y+3},8,Red); } break;
            default: break;
            }
            return Layer + 3;
        }

        TWeakPtr<FCkSmRuntimeGraphCardPresentation> _Presentation;
        int32 _Style = 0;
        bool _bTransition = false;
    };
} // namespace ck_sm_runtime_graph

void SCkSmRuntimeGraph::Construct(const FArguments& InArgs)
{
    _OnSelectionChanged = InArgs._OnSelectionChanged;
    _OnBreakpointRequested = InArgs._OnBreakpointRequested;
    ChildSlot[SAssignNew(_Canvas, SCkDebug_GraphCanvas)
                   .AllowNodeDragging(true)
                   .OnSelectionChanged(FOnCkDebug_GraphCanvasSelectionChanged::CreateSP(
                       this, &SCkSmRuntimeGraph::HandleSelectionChanged))
                   .OnNodeMoved(FOnCkDebug_GraphCanvasNodeMoved::CreateSP(
                       this, &SCkSmRuntimeGraph::HandleNodeMoved))
                   .OnResolveDragGroup(FOnCkDebug_GraphCanvasResolveDragGroup::CreateSP(
                       this, &SCkSmRuntimeGraph::ResolveDragGroup))
                   .OnNodeContextMenu(FOnCkDebug_GraphCanvasNodeContextMenu::CreateSP(
                       this, &SCkSmRuntimeGraph::HandleNodeContextMenu))];
}

auto SCkSmRuntimeGraph::SetSmInfo(const FCkSmDebugger_SmInfo* InInfo) -> void
{
    if (InInfo == nullptr)
    {
        if (_SmInfo != nullptr)
        {
            Clear();
        }
        return;
    }

    if (_PositionScopeHandle != InInfo->Handle)
    {
        _PositionOverrides.Reset();
        _PositionScopeHandle = InInfo->Handle;
    }
    _SmInfo = InInfo;
    const auto NewStructureHash = FCkSmRuntimeGraphModel::ComputeStructureHash(
        *InInfo,
        _Layout.ExpandTasks,
        _Layout.NameDepth,
        _Layout.SpacingX,
        _Layout.SpacingY,
        _Layout.UndirectedBFS);
    if (NOT _HasStructureHash || _StructureHash != NewStructureHash)
    {
        RebuildScene();
        return;
    }

    _Model.UpdateRuntimeState(*InInfo);
    InstallScene();
}
auto SCkSmRuntimeGraph::SetExpandTasks(const bool bInExpandTasks) -> void
{
    if (_Layout.ExpandTasks == bInExpandTasks)
    {
        return;
    }
    _Layout.ExpandTasks = bInExpandTasks;
    RebuildScene();
}
auto SCkSmRuntimeGraph::SetNameDepth(const int32 InNameDepth) -> void
{
    const auto ClampedDepth = FMath::Max(0, InNameDepth);
    if (_Layout.NameDepth == ClampedDepth)
    {
        return;
    }
    _Layout.NameDepth = ClampedDepth;
    RebuildScene();
}
auto SCkSmRuntimeGraph::SetLayout(const FCkSmRuntimeGraphLayout& InParams) -> void
{
    if (_Layout == InParams)
    {
        return;
    }
    const auto bGeometryChanged = _Layout.ExpandTasks != InParams.ExpandTasks ||
                                  _Layout.UndirectedBFS != InParams.UndirectedBFS ||
                                  _Layout.SpacingX != InParams.SpacingX ||
                                  _Layout.SpacingY != InParams.SpacingY ||
                                  _Layout.NameDepth != InParams.NameDepth;
    _Layout = InParams;
    _BreakpointStyle = InParams.StateBreakpointStyle;
    _TransitionBreakpointStyle = InParams.TransitionBreakpointStyle;
    if (bGeometryChanged)
    {
        RebuildScene();
    }
    else
    {
        // Style-only changes rebuild retained cards through their card structure hash, but must
        // not discard an in-session manual arrangement.
        InstallScene();
    }
}

auto SCkSmRuntimeGraph::SetBreakpointStyle(const int32 InBreakpointStyle) -> void
{
    if (_BreakpointStyle == InBreakpointStyle)
    {
        return;
    }
    _BreakpointStyle = InBreakpointStyle;
    InstallScene();
}
auto SCkSmRuntimeGraph::ApplyScrubHighlight(const int32 InActiveStateIndex,
                                            const int32 InExitedStateIndex) -> void
{
    _Model.ApplyScrubHighlight(InActiveStateIndex, InExitedStateIndex);
    InstallScene();
}
auto SCkSmRuntimeGraph::ClearPresentation() -> void
{
    _Model.ClearPresentation();
    InstallScene();
}
auto SCkSmRuntimeGraph::TriggerLivePresentation(
    const TArray<FCkSmDebugger_HistoryEntry>& InEvents) -> void
{
    _Model.TriggerLivePresentation(InEvents);
    InstallScene();
}
auto SCkSmRuntimeGraph::TriggerLivePresentation(
    const int32 InPreviousStateIndex,
    const int32 InCurrentStateIndex,
    const TSet<FString>& InPreviousStateNames) -> void
{
    _Model.TriggerLivePresentation(InPreviousStateIndex,
                                   InCurrentStateIndex,
                                   InPreviousStateNames);
    InstallScene();
}

auto SCkSmRuntimeGraph::TickLivePresentation(const float InDeltaTime) -> void
{
    _Model.TickLivePresentation(InDeltaTime);
    InstallScene();
}
auto SCkSmRuntimeGraph::FrameAll() -> void
{
    if (_Canvas)
    {
        _Canvas->Frame_All();
    }
}

auto SCkSmRuntimeGraph::ResetNodePositions() -> void
{
    if (_PositionOverrides.IsEmpty())
    {
        return;
    }
    _PositionOverrides.Reset();
    InstallScene();
}

auto SCkSmRuntimeGraph::Clear() -> void
{
    _SmInfo = nullptr;
    _Model.Clear();
    _CardCache.Reset();
    _CardStructureHashes.Reset();
    _CardPresentations.Reset();
    _StatePills.Reset();
    _PositionOverrides.Reset();
    _PositionScopeHandle = FCk_Handle_StateMachine{};
    _StructureHash = 0;
    _HasStructureHash = false;
    if (_Canvas)
    {
        const auto InstallingGuard = TGuardValue<bool>(_IsInstallingScene, true);
        _Canvas->Set_Scene({});
    }
}

auto SCkSmRuntimeGraph::RebuildScene(const bool bInClearPositionOverrides) -> void
{
    if (NOT _Canvas)
    {
        return;
    }
    if (NOT _SmInfo)
    {
        Clear();
        return;
    }
    if (bInClearPositionOverrides)
    {
        _PositionOverrides.Reset();
    }
    _Model.Rebuild(*_SmInfo,
                   _Layout.ExpandTasks,
                   _Layout.NameDepth,
                   _Layout.SpacingX,
                   _Layout.SpacingY,
                   _Layout.UndirectedBFS);
    auto PresentIds = TSet<uint64>{};
    for (const auto& Node : _Model.GetScene().Nodes)
    {
        PresentIds.Add(Node.Id);
    }
    for (auto It = _PositionOverrides.CreateIterator(); It; ++It)
    {
        const auto* Node = _Model.FindNodeById(It.Key());
        if (NOT PresentIds.Contains(It.Key()) ||
            (Node != nullptr && Node->Kind == ECkSmRuntimeGraphNodeKind::Compound))
        {
            It.RemoveCurrent();
        }
    }
    _StructureHash = FCkSmRuntimeGraphModel::ComputeStructureHash(*_SmInfo,
                                                                  _Layout.ExpandTasks,
                                                                  _Layout.NameDepth,
                                                                  _Layout.SpacingX,
                                                                  _Layout.SpacingY,
                                                                  _Layout.UndirectedBFS);
    _HasStructureHash = true;
    InstallScene();
}

auto SCkSmRuntimeGraph::InstallScene() -> void
{
    if (NOT _Canvas)
    {
        return;
    }
    // Compounds became derived geometry. Discard stale session entries from the previous
    // ownership model before resolving the scene so they cannot reappear after a refresh.
    for (auto It = _PositionOverrides.CreateIterator(); It; ++It)
    {
        const auto* OverrideNode = _Model.FindNodeById(It.Key());
        if (OverrideNode && OverrideNode->Kind == ECkSmRuntimeGraphNodeKind::Compound)
        {
            It.RemoveCurrent();
        }
    }
    auto CanvasScene = FCkDebug_GraphCanvasScene{};
    const auto& SelectedIds = _Canvas->Get_SelectedNodeIds();
    auto PresentIds = TSet<uint64>{};
    for (const auto& Node : _Model.GetScene().Nodes)
    {
        PresentIds.Add(Node.Id);
        const auto EffectiveGeometry = FCkSmRuntimeGraphModel::ResolveNodeGeometry(
            _Model.GetScene(), Node, _PositionOverrides);
        auto Presentation = _CardPresentations.FindRef(Node.Id);
        if (NOT Presentation)
        {
            Presentation = MakeShared<FCkSmRuntimeGraphCardPresentation>();
            _CardPresentations.Add(Node.Id, Presentation);
        }
        Presentation->Node = Node;
        Presentation->Node.Size = EffectiveGeometry.Size;
        Presentation->bSelected = SelectedIds.Contains(Node.Id);

        auto StructureHash = ck_sm_runtime_graph::GetCardStructureHash(Node);
        StructureHash = HashCombine(StructureHash, GetTypeHash(_BreakpointStyle));
        StructureHash = HashCombine(StructureHash, GetTypeHash(_TransitionBreakpointStyle));
        const auto* CachedHash = _CardStructureHashes.Find(Node.Id);
        auto Card = _CardCache.FindRef(Node.Id);
        if (NOT Card || CachedHash == nullptr || *CachedHash != StructureHash)
        {
            _StatePills.Remove(Node.Id);
            Card = MakeCard(Presentation.ToSharedRef());
            _CardCache.Add(Node.Id, Card);
            _CardStructureHashes.Add(Node.Id, StructureHash);
        }
        if (const auto Pill = _StatePills.FindRef(Node.Id))
        {
            Pill->Set_Selected(SelectedIds.Contains(Node.Id));
        }
        auto CanvasNode = FCkDebug_GraphCanvasNode{};
        CanvasNode.Id = Node.Id;
        CanvasNode.Position = Node.Kind == ECkSmRuntimeGraphNodeKind::Transition
                                  ? GetEffectiveTransitionBadgePosition(Node)
                                  : EffectiveGeometry.Position;
        CanvasNode.Size = EffectiveGeometry.Size;
        CanvasNode.Layer = Node.Kind == ECkSmRuntimeGraphNodeKind::Compound ? 0 : 1;
        if (Node.Kind == ECkSmRuntimeGraphNodeKind::Compound)
        {
            const auto DescendantIds = GetCompoundDescendantIds(Node.StateIndex);
            for (const auto DescendantId : DescendantIds)
            {
                const auto* Descendant = _Model.FindNodeById(DescendantId);
                if (Descendant && Descendant->Kind == ECkSmRuntimeGraphNodeKind::State
                    && _PositionOverrides.Contains(DescendantId))
                {
                    CanvasNode.bHasManualPosition = true;
                    break;
                }
            }
        }
        else
        {
            CanvasNode.bHasManualPosition = _PositionOverrides.Contains(Node.Id);
        }
        CanvasNode.Widget = Card;
        CanvasScene.Nodes.Add(MoveTemp(CanvasNode));
    }
    for (const auto& Edge : _Model.GetScene().Edges)
    {
        const auto EventEmphasis = FCkSmDebuggerStyle::Get_GraphEventEmphasis(
            UCkDebuggerStyleSettings::Get_Selection().GraphEventEmphasis);
        auto CanvasEdge = FCkDebug_GraphCanvasEdge{};
        CanvasEdge.SourceId = Edge.SourceId;
        CanvasEdge.TargetId = Edge.TargetId;
        CanvasEdge.Color = Edge.bScrubHighlighted
                               ? CkStyle::Ok()
                               : (Edge.LiveFlashAlpha > 0.0f ? FLinearColor::White : Edge.Color);
        CanvasEdge.Thickness =
            Edge.bScrubHighlighted
                ? 3.0f
                : FMath::Lerp(Edge.Thickness,
                              FMath::Max(Edge.Thickness,
                                         EventEmphasis.EdgeFlashPeakThickness),
                              FMath::Clamp(Edge.LiveFlashAlpha, 0.0f, 1.0f));
        if (Edge.LiveFlashAlpha > 0.0f)
        {
            CanvasEdge.Color =
                FMath::Lerp(CkStyle::TextDim(), CkStyle::Warn(), Edge.LiveFlashAlpha);
        }
        CanvasEdge.IsDirected = Edge.bDirected;
        CanvasEdge.IsDashed = Edge.bReverse;
        CanvasEdge.LineSeparation = 4.5f;
        CanvasEdge.RoutePoints = GetEffectiveRoutePoints(Edge);
        CanvasScene.Edges.Add(MoveTemp(CanvasEdge));
    }
    for (auto It = _CardCache.CreateIterator(); It; ++It)
    {
        if (NOT PresentIds.Contains(It.Key()))
        {
            _CardStructureHashes.Remove(It.Key());
            _CardPresentations.Remove(It.Key());
            _StatePills.Remove(It.Key());
            It.RemoveCurrent();
        }
    }
    const auto InstallingGuard = TGuardValue<bool>(_IsInstallingScene, true);
    _Canvas->Set_Scene(MoveTemp(CanvasScene));
}

auto SCkSmRuntimeGraph::MakeCard(
    const TSharedRef<FCkSmRuntimeGraphCardPresentation>& InPresentation)
    -> TSharedRef<SWidget>
{
    const auto& InNode = InPresentation->Node;
    const auto WeakPresentation = TWeakPtr<FCkSmRuntimeGraphCardPresentation>{InPresentation};
    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Compound)
    {
        return SNew(SBorder)
            .BorderImage(ck_sm_runtime_graph::GetCompoundBorderBrush())
            .BorderBackgroundColor_Lambda([WeakPresentation]() -> FSlateColor
            {
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return CkStyle::TextDim();
                }
                auto Color = CkStyle::TextDim();
                Color.A = (Presentation->Node.bCurrent ? 0.22f : 0.10f)
                          * ck_sm_debugger_axes::Get_NodeDimScale();
                return Color;
            })
            .Padding(
                1.0f)[SNew(SBorder)
                           .BorderImage(ck_sm_runtime_graph::GetCompoundFillBrush())
                           .BorderBackgroundColor_Lambda([WeakPresentation]() -> FSlateColor
                           {
                               const auto Presentation = WeakPresentation.Pin();
                               auto Color = CkStyle::Bg3();
                               Color.A = ck_sm_debugger_axes::Get_NodeDrawsFill()
                                             ? (Presentation && Presentation->Node.bCurrent
                                                    ? 0.02f
                                                    : 0.01f)
                                                   * ck_sm_debugger_axes::Get_NodeDimScale()
                                             : 0.0f;
                               return Color;
                           })
                          .Padding(
                              8.0f)[SNew(STextBlock)
                                        .Text_Lambda([WeakPresentation]()
                                        {
                                            const auto Presentation = WeakPresentation.Pin();
                                            return Presentation
                                                       ? FText::FromString(Presentation->Node.Label)
                                                       : FText::GetEmpty();
                                        })
                                        .ColorAndOpacity_Lambda([WeakPresentation]() -> FSlateColor
                                        {
                                            const auto Presentation = WeakPresentation.Pin();
                                            return Presentation && Presentation->Node.bCurrent
                                                       ? CkStyle::Warn()
                                                       : CkStyle::TextDim();
                                        })]];
    }

    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Entry)
    {
        auto EntryPill = TSharedPtr<SCkDebug_NodePill>{};
        auto EntryCard =
            SAssignNew(EntryPill, SCkDebug_NodePill)
                .Variant(ECkDebug_NodePillVariant::Inactive)
                .Title(FText::FromString(TEXT("Entry")))
                .ShowCost(false)
                .MinDesiredWidth(InNode.Size.X)
                .AccentColor(CkStyle::Warn())
                .AccentWidth(FCkSmDebuggerStyle::Sm_AccentBarWidth)
                .BorderColorOverride(CkStyle::Warn())
                .FillColorOverride(CkStyle::Bg2())
                .Selected(InPresentation->bSelected)
                .BodyContent()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("INITIAL FLOW")))
                        .Font_Lambda([]()
                        {
                            return ck::debug_axes::ScaledFont("Bold", 7);
                        })
                        .ColorAndOpacity(CkStyle::TextDim())
                ];
        _StatePills.Add(InNode.Id, EntryPill);
        return EntryCard;
    }

    if (InNode.Kind == ECkSmRuntimeGraphNodeKind::Transition)
    {
        // Conditions belong to the existing Details surface; retaining them here changed the graph
        // topology and obscured wires. The compact badge still has to retain the brushes' authored
        // 25px footprint or its color-spill, icon, and breakpoint marker clip into one glyph.
        return SNew(SBox)
            .WidthOverride(FCkSmDebuggerStyle::Sm_TransitionBadgeSize)
            .HeightOverride(FCkSmDebuggerStyle::Sm_TransitionBadgeSize)
            [SNew(SOverlay)
             + SOverlay::Slot()[SNew(SImage)
                                     .Image(FCkDebuggerStyle::Get().GetBrush(
                                         TEXT("CkDebugger.Graph.TransitionNode.ColorSpill")))
                                     .ColorAndOpacity_Lambda([WeakPresentation]() -> FSlateColor
                                     {
                                         const auto Presentation = WeakPresentation.Pin();
                                         return Presentation && Presentation->bSelected
                                                    ? CkStyle::Accent()
                                                    : CkStyle::TextStrong();
                                     })]
             + SOverlay::Slot()[SNew(SImage)
                                     .Image(FCkIconStyle::Get_Brush(
                                         ECk_Icon::Tween, ECk_Icon_BrushSize::Size_16x16))]
             + SOverlay::Slot()[SNew(ck_sm_runtime_graph::SCkSmRuntimeBreakpointOverlay)
                                     .Presentation(WeakPresentation)
                                     .Style(_TransitionBreakpointStyle)
                                     .bTransition(true)]
             + SOverlay::Slot()
                   .HAlign(HAlign_Center)
                   .VAlign(VAlign_Center)
                   [SNew(SBox)
                        .WidthOverride(14.0f)
                        .HeightOverride(14.0f)
                        [SNew(SButton)
                             .ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
                             .ContentPadding(0.0f)
                             .ToolTipText(FText::FromString(TEXT("Toggle transition breakpoint")))
                             .IsEnabled_Lambda([this]() { return _OnBreakpointRequested.IsBound(); })
                             .OnClicked_Lambda([this, TransitionIndex = InNode.TransitionIndex]()
                             {
                                 return HandleBreakpointClicked(
                                     ECkSmRuntimeBreakpointTarget::Transition,
                                     TransitionIndex);
                             })]]];
    }

    auto Body = SNew(SVerticalBox);
    const auto BreakpointRed = CkStyle::Err();
    const auto BreakpointHollow = CkStyle::OverlayOf(BreakpointRed, 0.25f);
    const auto IndicatorSize = TAttribute<FOptionalSize>::CreateLambda([]()
    {
        return FOptionalSize{ck_sm_debugger_axes::Get_IndicatorSize()};
    });
    const auto bUseInlineBreakpoints = ck_sm_runtime_graph::IsInlineBreakpointStyle(_BreakpointStyle);
    const auto bUseDiamondBreakpoints = _BreakpointStyle == 23;
    const auto BreakpointTransform = bUseDiamondBreakpoints
                                         ? FSlateRenderTransform(FQuat2D(PI / 4.0))
                                         : FSlateRenderTransform();
    const auto MakeInlineBreakpoint =
        [this,
         WeakPresentation,
         BreakpointRed,
         BreakpointHollow,
         BreakpointTransform,
         StateIndex = InNode.StateIndex](const ECkSmRuntimeBreakpointTarget InTarget,
                                         const bool bInEntry) -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
            .ContentPadding(0.0f)
            .ToolTipText(FText::FromString(bInEntry ? TEXT("Toggle state entry breakpoint")
                                                     : TEXT("Toggle state exit breakpoint")))
            .IsEnabled_Lambda([this]() { return _OnBreakpointRequested.IsBound(); })
            .OnClicked_Lambda([this, InTarget, StateIndex]()
            {
                return HandleBreakpointClicked(InTarget, StateIndex);
            })
            [SNew(SBorder)
                 .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                 .BorderBackgroundColor_Lambda(
                     [WeakPresentation,
                      BreakpointRed,
                      BreakpointHollow,
                      bInEntry]() -> FSlateColor
                     {
                         const auto Presentation = WeakPresentation.Pin();
                         const auto bSet = Presentation && Presentation->Node.State
                                               && (bInEntry
                                                       ? Presentation->Node.State->HasEntryBreakpoint
                                                       : Presentation->Node.State->HasExitBreakpoint);
                         return bSet ? BreakpointRed : BreakpointHollow;
                     })
                 .RenderTransformPivot(FVector2D(0.5f, 0.5f))
                 .RenderTransform(BreakpointTransform)];
    };
    const auto LeftIndicator = bUseInlineBreakpoints
                                   ? MakeInlineBreakpoint(
                                         ECkSmRuntimeBreakpointTarget::StateEntry,
                                         true)
                                   : StaticCastSharedRef<SWidget>(
                                         SNew(SBorder)
                                             .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                                             .BorderBackgroundColor_Lambda([WeakPresentation]() -> FSlateColor
                                             {
                                                 const auto Presentation = WeakPresentation.Pin();
                                                 return Presentation ? Presentation->Node.Accent
                                                                     : CkStyle::TextMute();
                                             }));
    auto TitleRow = SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
              .AutoWidth()
              .VAlign(VAlign_Center)
              .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                   [SNew(SBox)
                        .WidthOverride(IndicatorSize)
                        .HeightOverride(IndicatorSize)
                            [LeftIndicator]]
        + SHorizontalBox::Slot()
              .AutoWidth()
              .VAlign(VAlign_Center)
                  [SNew(STextBlock)
                       .Text_Lambda([WeakPresentation]()
                       {
                           const auto Presentation = WeakPresentation.Pin();
                           return Presentation ? FText::FromString(Presentation->Node.Label)
                                               : FText::GetEmpty();
                       })
                       .Font_Lambda([]()
                       {
                           return ck::debug_axes::ScaledFont("Bold", CkStyle::NodeTitleFontSize());
                       })
                       .ColorAndOpacity(FSlateColor(CkStyle::Text()))]
         + SHorizontalBox::Slot()
               .AutoWidth()
               .VAlign(VAlign_Center)
               .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                   [SNew(SBox)
                        .WidthOverride(IndicatorSize)
                        .HeightOverride(IndicatorSize)
                        .Visibility(bUseInlineBreakpoints ? EVisibility::Visible
                                                          : EVisibility::Collapsed)
                            [MakeInlineBreakpoint(ECkSmRuntimeBreakpointTarget::StateExit,
                                                  false)]]
         + SHorizontalBox::Slot()
               .AutoWidth()
               .VAlign(VAlign_Center)
               .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                   [SNew(SBox)
                        // Reserve this small slot so a live breakpoint hit never changes card geometry.
                        .WidthOverride(18.0f)
                            [SNew(STextBlock)
                                 .Text(FText::FromString(TEXT("HIT")))
                                 .Font_Lambda([]() { return ck::debug_axes::ScaledFont("Bold", 7); })
                                 .ColorAndOpacity(CkStyle::Err())
                                 .Visibility_Lambda([WeakPresentation]()
                                 {
                                     const auto Presentation = WeakPresentation.Pin();
                                     return Presentation && Presentation->Node.State
                                                        && Presentation->Node.State->IsBreakpointHit
                                                ? EVisibility::SelfHitTestInvisible
                                                : EVisibility::Collapsed;
                                 })]];
    Body->AddSlot().AutoHeight()[TitleRow];
    Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [SNew(STextBlock)
             .Text(FText::FromString(TEXT("OVERRIDE")))
             .Font_Lambda([]() { return ck::debug_axes::ScaledFont("Bold", 7); })
             .ColorAndOpacity(FCkSmDebuggerStyle::Color_Sm_Override)
             .Visibility_Lambda([WeakPresentation]()
             {
                 const auto Presentation = WeakPresentation.Pin();
                 return Presentation && Presentation->Node.bHasOverride
                            ? EVisibility::SelfHitTestInvisible
                            : EVisibility::Collapsed;
             })];
    Body->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
        [SNew(STextBlock)
             .Text(FText::FromString(TEXT("EVENT-DRIVEN")))
             .Font_Lambda([]() { return ck::debug_axes::ScaledFont("Bold", 7); })
             .ColorAndOpacity(CkStyle::Warn())
             .Visibility_Lambda([WeakPresentation]()
             {
                 const auto Presentation = WeakPresentation.Pin();
                 return Presentation && Presentation->Node.bFullyEventDriven
                            ? EVisibility::SelfHitTestInvisible
                            : EVisibility::Collapsed;
             })];
    if (InNode.State && InNode.bExpandTasks)
    {
        for (auto TaskIndex = 0; TaskIndex < InNode.State->Tasks.Num(); ++TaskIndex)
        {
            Body->AddSlot()
                .AutoHeight()
                .Padding(FCkSmDebuggerStyle::Sm_NodePadding, 2.0f)
                    [SNew(SHorizontalBox)
                     + SHorizontalBox::Slot()
                           .AutoWidth()
                           .VAlign(VAlign_Center)
                           .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                               [SNew(SBox)
                                    .WidthOverride(FCkSmDebuggerStyle::Sm_StateIconSize)
                                    .HeightOverride(FCkSmDebuggerStyle::Sm_StateIconSize)
                                        [SNew(SBorder)
                                             .BorderImage(FCoreStyle::Get().GetBrush(
                                                 TEXT("WhiteBrush")))
                                             .BorderBackgroundColor_Lambda(
                                                 [WeakPresentation,
                                                  TaskIndex]() -> FSlateColor
                                                 {
                                                     const auto Presentation =
                                                         WeakPresentation.Pin();
                                                     if (NOT Presentation
                                                         || NOT Presentation->Node.State
                                                         || NOT Presentation->Node.State->Tasks
                                                                    .IsValidIndex(TaskIndex))
                                                     {
                                                         return CkStyle::TextMute();
                                                     }
                                                     return CkSmDebugger::GetTaskResultColor(
                                                         Presentation->Node.State->Tasks[TaskIndex]
                                                             .LastResult);
                                                 })]]
                     + SHorizontalBox::Slot()
                           .FillWidth(1.0f)
                           .VAlign(VAlign_Center)
                               [SNew(STextBlock)
                                    .Text_Lambda([WeakPresentation,
                                                  TaskIndex,
                                                  NameDepth = _Layout.NameDepth]()
                                    {
                                        const auto Presentation = WeakPresentation.Pin();
                                        if (NOT Presentation || NOT Presentation->Node.State
                                            || NOT Presentation->Node.State->Tasks.IsValidIndex(
                                                TaskIndex))
                                        {
                                            return FText::GetEmpty();
                                        }
                                        return FText::FromString(
                                            SCkDebug_NameLabel::Get_ShortName(
                                                Presentation->Node.State->Tasks[TaskIndex]
                                                    .ClassName,
                                                NameDepth));
                                    })
                                    .Font_Lambda([]()
                                    {
                                        return ck::debug_axes::ScaledFont(
                                            "Regular", CkStyle::FontSizeMicro());
                                    })
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))]
                     + SHorizontalBox::Slot()
                           .AutoWidth()
                           .VAlign(VAlign_Center)
                           .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                               [SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("TICK")))
                                    .Font_Lambda([]()
                                    {
                                        return ck::debug_axes::ScaledFont("Bold", 7);
                                    })
                                    .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
                                    .Visibility_Lambda([WeakPresentation, TaskIndex]()
                                    {
                                        const auto Presentation = WeakPresentation.Pin();
                                        return Presentation && Presentation->Node.State
                                                       && Presentation->Node.State->Tasks
                                                              .IsValidIndex(TaskIndex)
                                                       && Presentation->Node.State->Tasks[TaskIndex]
                                                                  .Mode
                                                              == ECk_SmTaskMode::Tick
                                                   ? EVisibility::SelfHitTestInvisible
                                                   : EVisibility::Collapsed;
                                    })]];
        }
    }
    auto Pill = TSharedPtr<SCkDebug_NodePill>{};
    auto Card =
        SAssignNew(Pill, SCkDebug_NodePill)
            .Variant(ECkDebug_NodePillVariant::Inactive)
            .Title(FText::GetEmpty())
            .ShowCost(false)
            .AccentColor_Lambda([WeakPresentation]()
            {
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return FLinearColor::Transparent;
                }
                return Presentation->Node.Accent;
            })
            .AccentWidth_Lambda([]() { return FCkSmDebuggerStyle::Sm_AccentBarWidth; })
            .BorderColorOverride_Lambda([WeakPresentation]()
            {
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return CkStyle::NodeFill_Inactive();
                }
                if (Presentation->Node.State
                    && Presentation->Node.State->IsBreakpointHit)
                {
                    return CkStyle::Err();
                }
                if (Presentation->Node.bScrubActive)
                {
                    return CkStyle::Ok();
                }
                const auto Border = FMath::Lerp(CkStyle::NodeFill_Inactive(),
                                                CkStyle::Ok(),
                                                Presentation->Node.BorderGlowAlpha);
                if (Presentation->Node.StateEventAlpha > 0.0f)
                {
                    return FMath::Lerp(Border,
                                       CkStyle::Warn(),
                                       FMath::Clamp(Presentation->Node.StateEventAlpha,
                                                    0.0f,
                                                    1.0f));
                }
                return Border;
            })
            .FillColorOverride_Lambda([WeakPresentation]()
            {
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return CkStyle::NodeFill_Inactive();
                }
                if (Presentation->Node.bScrubActive)
                {
                    return CkStyle::NodeFill_InPlan();
                }
                return FMath::Lerp(CkStyle::NodeFill_Inactive(),
                                   CkStyle::NodeFill_InPlan(),
                                   Presentation->Node.BorderGlowAlpha);
            })
            .OpacityOverride_Lambda([WeakPresentation]()
            {
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return ck::debug_axes::Get_NodeInactiveOpacity();
                }
                if (Presentation->Node.bScrubActive || Presentation->Node.bScrubExited)
                {
                    return 1.0f;
                }
                const auto InactiveOpacity = ck::debug_axes::Get_NodeInactiveOpacity();
                const auto Opacity = FMath::Lerp(InactiveOpacity,
                                                 1.0f,
                                                 Presentation->Node.CellGlowAlpha);
                auto EffectiveOpacity = Opacity;
                if (Presentation->Node.State && Presentation->Node.State->IsSubSmNode
                    && NOT Presentation->Node.bParentActive)
                {
                    // A sub-state whose owner is inactive stays subdued once at the card level.
                    // Per-control alpha multipliers made the same card nearly disappear.
                    const auto ParentInactiveOpacity = FMath::Lerp(InactiveOpacity, 1.0f, 0.35f);
                    EffectiveOpacity = FMath::Min(Opacity, ParentInactiveOpacity);
                }
                // Nested machines may be inactive when their exit event arrives. Bring only the
                // transient event frame back to full opacity so its yellow outline remains legible.
                return FMath::Lerp(EffectiveOpacity,
                                   1.0f,
                                   FMath::Clamp(Presentation->Node.StateEventAlpha, 0.0f, 1.0f));
            })
            .BorderThickness_Lambda([WeakPresentation]()
            {
                const auto BaseThickness = ck::debug_axes::Get_NodeBorderThickness();
                const auto Presentation = WeakPresentation.Pin();
                if (NOT Presentation)
                {
                    return BaseThickness;
                }
                const auto EventEmphasis = FCkSmDebuggerStyle::Get_GraphEventEmphasis(
                    UCkDebuggerStyleSettings::Get_Selection().GraphEventEmphasis);
                return FMath::Lerp(
                    BaseThickness,
                    FMath::Max(BaseThickness, EventEmphasis.StateEventOutlinePeakThickness),
                    FMath::Clamp(Presentation->Node.StateEventAlpha, 0.0f, 1.0f));
            })
            .Selected(InPresentation->bSelected)
            .BodyContent()[Body];
    Card->SetToolTipText(TAttribute<FText>::CreateLambda([WeakPresentation]()
    {
        const auto Presentation = WeakPresentation.Pin();
        if (NOT Presentation || NOT Presentation->Node.State)
        {
            return FText::GetEmpty();
        }
        const auto& State = *Presentation->Node.State;
        if (State.IsCurrentState)
        {
            return FText::FromString(
                FString::Printf(TEXT("Active for %.2f secs"), State.DwellTimeSeconds));
        }
        if (Presentation->Node.bPrevious)
        {
            return FText::FromString(
                FString::Printf(TEXT("Was active for %.2f secs"), State.DwellTimeSeconds));
        }
        return FText::GetEmpty();
    }));
    _StatePills.Add(InNode.Id, Pill);
    if (NOT bUseInlineBreakpoints)
    {
        return SNew(SOverlay)
            + SOverlay::Slot()[Card]
            + SOverlay::Slot()[SNew(ck_sm_runtime_graph::SCkSmRuntimeBreakpointOverlay)
                                   .Presentation(WeakPresentation)
                                   .Style(_BreakpointStyle)
                                   .bTransition(false)];
    }
    return Card;
}

auto SCkSmRuntimeGraph::HandleSelectionChanged(const TSet<uint64>& InSelection) -> void
{
    if (_IsInstallingScene)
    {
        return;
    }
    for (auto& Pair : _CardPresentations)
    {
        Pair.Value->bSelected = InSelection.Contains(Pair.Key);
    }
    for (auto& Pair : _StatePills)
    {
        Pair.Value->Set_Selected(InSelection.Contains(Pair.Key));
    }
    int32 StateIndex = INDEX_NONE;
    int32 TransitionIndex = INDEX_NONE;
    for (const auto Id : InSelection)
    {
        if (Id >= 0x100000000ull && Id < 0x200000000ull)
        {
            StateIndex = static_cast<int32>(Id - 0x100000000ull);
            break;
        }
        if (Id >= 0x200000000ull && Id < 0x300000000ull)
        {
            TransitionIndex = static_cast<int32>(Id - 0x200000000ull);
            break;
        }
    }
    _OnSelectionChanged.ExecuteIfBound(StateIndex, TransitionIndex);
}

auto SCkSmRuntimeGraph::GetEffectivePosition(const FCkSmRuntimeGraphNode& InNode) const -> FVector2D
{
    return FCkSmRuntimeGraphModel::ResolveNodeGeometry(
               _Model.GetScene(), InNode, _PositionOverrides)
        .Position;
}

auto SCkSmRuntimeGraph::GetEffectiveSize(const FCkSmRuntimeGraphNode& InNode) const -> FVector2D
{
    return FCkSmRuntimeGraphModel::ResolveNodeGeometry(
               _Model.GetScene(), InNode, _PositionOverrides)
        .Size;
}

auto SCkSmRuntimeGraph::GetEffectiveRoutePoints(const FCkSmRuntimeGraphEdge& InEdge) const
    -> TArray<FVector2D>
{
    auto Result = InEdge.RoutePoints;
    if (Result.IsEmpty())
    {
        return Result;
    }

    const auto* SourceNode = _Model.FindNodeById(InEdge.SourceId);
    const auto* TargetNode = _Model.FindNodeById(InEdge.TargetId);
    if (SourceNode == nullptr || TargetNode == nullptr)
    {
        return Result;
    }

    const auto SourceDelta = GetEffectivePosition(*SourceNode) - SourceNode->Position;
    const auto TargetDelta = GetEffectivePosition(*TargetNode) - TargetNode->Position;
    if (SourceDelta.Equals(TargetDelta))
    {
        // Compound moves are pure translations: preserve every generated/authored point exactly.
        for (auto& Point : Result)
        {
            Point += SourceDelta;
        }
        return Result;
    }

    const auto SourceCenter = SourceNode->Position + SourceNode->Size * 0.5f;
    const auto TargetCenter = TargetNode->Position + TargetNode->Size * 0.5f;
    const auto EffectiveSourceCenter = GetEffectivePosition(*SourceNode) +
                                       GetEffectiveSize(*SourceNode) * 0.5f;
    const auto EffectiveTargetCenter = GetEffectivePosition(*TargetNode) +
                                       GetEffectiveSize(*TargetNode) * 0.5f;
    const auto BaseDirection = TargetCenter - SourceCenter;
    const auto BaseLength = BaseDirection.Size();
    if (BaseLength <= KINDA_SMALL_NUMBER)
    {
        // Self-loop points are source-relative; when only its one node moves this is still exact.
        for (auto& Point : Result)
        {
            Point += SourceDelta;
        }
        return Result;
    }

    const auto EffectiveDirection = EffectiveTargetCenter - EffectiveSourceCenter;
    const auto EffectiveLength = EffectiveDirection.Size();
    const auto BaseNormal = FVector2D{-BaseDirection.Y, BaseDirection.X} / BaseLength;
    const auto EffectiveNormal = EffectiveLength > KINDA_SMALL_NUMBER
                                     ? FVector2D{-EffectiveDirection.Y, EffectiveDirection.X} /
                                           EffectiveLength
                                     : BaseNormal;
    const auto PerpendicularScale = EffectiveLength > KINDA_SMALL_NUMBER
                                        ? EffectiveLength / BaseLength
                                        : 1.0f;
    for (auto& Point : Result)
    {
        const auto Relative = Point - SourceCenter;
        const auto Along = FVector2D::DotProduct(Relative, BaseDirection) /
                           FMath::Square(BaseLength);
        const auto Perpendicular = FVector2D::DotProduct(Relative, BaseNormal) * PerpendicularScale;
        Point = EffectiveSourceCenter + EffectiveDirection * Along + EffectiveNormal * Perpendicular;
    }
    return Result;
}

auto SCkSmRuntimeGraph::GetEffectiveTransitionBadgePosition(const FCkSmRuntimeGraphNode& InNode) const
    -> FVector2D
{
    if (_PositionOverrides.Contains(InNode.Id))
    {
        return GetEffectivePosition(InNode);
    }

    const auto* Edge = _Model.GetScene().Edges.FindByPredicate([&InNode](const FCkSmRuntimeGraphEdge& InEdge)
    {
        return InEdge.TransitionId == InNode.Id;
    });
    if (Edge == nullptr)
    {
        return InNode.Position;
    }

    const auto* SourceNode = _Model.FindNodeById(Edge->SourceId);
    const auto* TargetNode = _Model.FindNodeById(Edge->TargetId);
    if (SourceNode == nullptr || TargetNode == nullptr)
    {
        return InNode.Position;
    }

    const auto RoutePoints = GetEffectiveRoutePoints(*Edge);
    auto Center = (GetEffectivePosition(*SourceNode) + GetEffectiveSize(*SourceNode) * 0.5f +
                    GetEffectivePosition(*TargetNode) + GetEffectiveSize(*TargetNode) * 0.5f) *
                   0.5f;
    if (Edge->bSelfLoop && RoutePoints.Num() >= 2)
    {
        Center = RoutePoints[1];
    }
    else if (Edge->bReverse && NOT RoutePoints.IsEmpty())
    {
        Center = RoutePoints[0];
    }
    return Center - InNode.Size * 0.5f;
}

auto SCkSmRuntimeGraph::IsStateDescendantOf(const int32 InStateIndex,
                                             const int32 InCompoundOwnerStateIndex) const -> bool
{
    const auto* Node = _Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(InStateIndex));
    if (Node == nullptr || NOT Node->State)
    {
        return false;
    }

    auto CurrentParent = Node->State->SubSmParentStateIndex;
    auto Visited = TSet<int32>{};
    while (CurrentParent != INDEX_NONE)
    {
        if (Visited.Contains(CurrentParent))
        {
            return false;
        }
        Visited.Add(CurrentParent);
        if (CurrentParent == InCompoundOwnerStateIndex)
        {
            return true;
        }
        const auto* ParentNode = _Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(CurrentParent));
        if (ParentNode == nullptr || NOT ParentNode->State)
        {
            return false;
        }
        CurrentParent = ParentNode->State->SubSmParentStateIndex;
    }
    return false;
}

auto SCkSmRuntimeGraph::GetCompoundDescendantIds(const int32 InCompoundOwnerStateIndex) const
    -> TSet<uint64>
{
    auto DescendantIds = TSet<uint64>{};
    for (const auto& Node : _Model.GetScene().Nodes)
    {
        if ((Node.Kind == ECkSmRuntimeGraphNodeKind::State ||
             Node.Kind == ECkSmRuntimeGraphNodeKind::Compound) &&
            IsStateDescendantOf(Node.StateIndex, InCompoundOwnerStateIndex))
        {
            DescendantIds.Add(Node.Id);
        }
    }
    for (const auto& Node : _Model.GetScene().Nodes)
    {
        if (Node.Kind != ECkSmRuntimeGraphNodeKind::Transition || NOT Node.Transition)
        {
            continue;
        }
        if (IsStateDescendantOf(Node.Transition->SourceStateIndex, InCompoundOwnerStateIndex) &&
            IsStateDescendantOf(Node.Transition->TargetStateIndex, InCompoundOwnerStateIndex))
        {
            DescendantIds.Add(Node.Id);
        }
    }
    return DescendantIds;
}

auto SCkSmRuntimeGraph::HasSelectedAncestorCompound(const uint64 InNodeId) const -> bool
{
    if (NOT _Canvas)
    {
        return false;
    }
    for (const auto SelectedId : _Canvas->Get_SelectedNodeIds())
    {
        const auto* SelectedNode = _Model.FindNodeById(SelectedId);
        if (SelectedNode && SelectedNode->Kind == ECkSmRuntimeGraphNodeKind::Compound &&
            GetCompoundDescendantIds(SelectedNode->StateIndex).Contains(InNodeId))
        {
            return true;
        }
    }
    return false;
}

auto SCkSmRuntimeGraph::ResolveDragGroup(const uint64 InNodeId) const -> TSet<uint64>
{
    const auto* Node = _Model.FindNodeById(InNodeId);
    if (Node == nullptr || Node->Kind != ECkSmRuntimeGraphNodeKind::Compound)
    {
        return {};
    }

    auto Result = GetCompoundDescendantIds(Node->StateIndex);
    Result.Add(Node->Id);
    return Result;
}

auto SCkSmRuntimeGraph::HandleNodeMoved(const uint64 InNodeId, const FVector2D& InPosition) -> void
{
    const auto* MovedNode = _Model.FindNodeById(InNodeId);
    if (MovedNode == nullptr || HasSelectedAncestorCompound(InNodeId))
    {
        // The canvas reports selected IDs independently and in stable-ID order. Descendants can
        // therefore arrive before their selected compound; the compound callback owns that move.
        return;
    }

    const auto PreviousPosition = MovedNode->Kind == ECkSmRuntimeGraphNodeKind::Transition
                                      ? GetEffectiveTransitionBadgePosition(*MovedNode)
                                      : GetEffectivePosition(*MovedNode);
    if (InPosition.Equals(PreviousPosition))
    {
        return;
    }

    if (MovedNode->Kind == ECkSmRuntimeGraphNodeKind::Compound)
    {
        // The canvas already moved the complete drag closure atomically. Persist those exact final
        // positions rather than re-deriving descendants from an override map being mutated here;
        // nested compound geometry is child-derived and would otherwise risk applying Delta twice.
        for (const auto DragNodeId : ResolveDragGroup(InNodeId))
        {
            const auto* ModelNode = _Model.FindNodeById(DragNodeId);
            if (ModelNode == nullptr || ModelNode->Kind != ECkSmRuntimeGraphNodeKind::State)
            {
                _PositionOverrides.Remove(DragNodeId);
                continue;
            }
            const auto* DragNode = _Canvas
                                       ? _Canvas->Get_Scene().Nodes.FindByPredicate(
                                             [DragNodeId](const FCkDebug_GraphCanvasNode& InNode)
                                             {
                                                 return InNode.Id == DragNodeId;
                                             })
                                       : nullptr;
            if (DragNode)
            {
                _PositionOverrides.Add(DragNodeId, DragNode->Position);
            }
        }
        // Compound bounds are derived from the persisted state positions, never independently
        // authored. Remove an old override if this session still has one.
        _PositionOverrides.Remove(InNodeId);
    }
    else
    {
        _PositionOverrides.Add(InNodeId, InPosition);
    }
    InstallScene();
}

auto SCkSmRuntimeGraph::HandleNodeContextMenu(const uint64 InNodeId,
                                              const FPointerEvent& InMouseEvent) -> void
{
    if (NOT _Canvas)
    {
        return;
    }

    const auto* Node = _Model.FindNodeById(InNodeId);
    const auto Payload = _Model.BuildCopyPayload(InNodeId);
    const auto bHasBreakpointAction = Node && _OnBreakpointRequested.IsBound()
                                      && ((Node->Kind == ECkSmRuntimeGraphNodeKind::State
                                           && Node->State)
                                          || (Node->Kind == ECkSmRuntimeGraphNodeKind::Transition
                                              && Node->Transition));
    if (NOT bHasBreakpointAction && NOT Payload.IsSet())
    {
        return;
    }

    auto Menu = FMenuBuilder(true, nullptr);
    if (bHasBreakpointAction && Node->Kind == ECkSmRuntimeGraphNodeKind::State)
    {
        Menu.AddMenuEntry(
            FText::FromString(Node->State->HasEntryBreakpoint
                                  ? TEXT("Clear entry breakpoint")
                                  : TEXT("Set entry breakpoint")),
            FText::FromString(TEXT("Toggle the breakpoint checked when this state becomes active")),
            FSlateIcon{},
            FUIAction(FExecuteAction::CreateSP(this,
                                               &SCkSmRuntimeGraph::RequestBreakpointToggle,
                                               ECkSmRuntimeBreakpointTarget::StateEntry,
                                               Node->StateIndex)));
        Menu.AddMenuEntry(
            FText::FromString(Node->State->HasExitBreakpoint
                                  ? TEXT("Clear exit breakpoint")
                                  : TEXT("Set exit breakpoint")),
            FText::FromString(TEXT("Toggle the breakpoint checked when this state exits")),
            FSlateIcon{},
            FUIAction(FExecuteAction::CreateSP(this,
                                               &SCkSmRuntimeGraph::RequestBreakpointToggle,
                                               ECkSmRuntimeBreakpointTarget::StateExit,
                                               Node->StateIndex)));
    }
    else if (bHasBreakpointAction)
    {
        Menu.AddMenuEntry(
            FText::FromString(Node->Transition->HasBreakpoint
                                  ? TEXT("Clear transition breakpoint")
                                  : TEXT("Set transition breakpoint")),
            FText::FromString(TEXT("Toggle the breakpoint checked when this transition fires")),
            FSlateIcon{},
            FUIAction(FExecuteAction::CreateSP(this,
                                               &SCkSmRuntimeGraph::RequestBreakpointToggle,
                                               ECkSmRuntimeBreakpointTarget::Transition,
                                               Node->TransitionIndex)));
    }

    if (bHasBreakpointAction && Payload.IsSet())
    {
        Menu.AddMenuSeparator();
    }

    if (Payload.IsSet() && Payload->Target == ECkSmRuntimeGraphCopyTarget::State)
    {
        ck::DebugCopyMenu::AddCopyEntry(Menu,
                                        FText::FromString(TEXT("Copy Display Name")),
                                        FText::FromString(TEXT("Copy the visible state label")),
                                        Payload->DisplayName);
        ck::DebugCopyMenu::AddCopyEntry(Menu,
                                        FText::FromString(TEXT("Copy State Class Name")),
                                        FText::FromString(
                                            TEXT("Copy the full underlying state class name")),
                                        Payload->ClassName);
    }
    else if (Payload.IsSet())
    {
        ck::DebugCopyMenu::AddCopyEntry(Menu,
                                        FText::FromString(TEXT("Copy Group Label")),
                                        FText::FromString(TEXT("Copy this group's title")),
                                        Payload->GroupLabel);
        if (NOT Payload->ChildDisplayNames.IsEmpty())
        {
            ck::DebugCopyMenu::AddCopyEntry(Menu,
                                            FText::FromString(TEXT("Copy Child State Names")),
                                            FText::FromString(TEXT(
                                                "Copy visible child state labels, one per line")),
                                            FString::Join(Payload->ChildDisplayNames, TEXT("\n")));
            ck::DebugCopyMenu::AddCopyEntry(Menu,
                                            FText::FromString(TEXT("Copy Child State Class Names")),
                                            FText::FromString(TEXT(
                                                "Copy full child state class names, one per line")),
                                            FString::Join(Payload->ChildClassNames, TEXT("\n")));
        }
        ck::DebugCopyMenu::AddCopyEntry(Menu,
                                        FText::FromString(TEXT("Copy All")),
                                        FText::FromString(
                                            TEXT("Copy the group label and child state names")),
                                        Payload->All);
    }

    FSlateApplication::Get().PushMenu(_Canvas.ToSharedRef(),
                                      FWidgetPath{},
                                      Menu.MakeWidget(),
                                      InMouseEvent.GetScreenSpacePosition(),
                                      FPopupTransitionEffect{FPopupTransitionEffect::ContextMenu});
}

auto SCkSmRuntimeGraph::HandleBreakpointClicked(const ECkSmRuntimeBreakpointTarget InTarget,
                                                 const int32 InIndex) -> FReply
{
    if (NOT _OnBreakpointRequested.IsBound())
    {
        return FReply::Unhandled();
    }
    RequestBreakpointToggle(InTarget, InIndex);
    return FReply::Handled();
}

auto SCkSmRuntimeGraph::RequestBreakpointToggle(const ECkSmRuntimeBreakpointTarget InTarget,
                                                 const int32 InIndex) -> void
{
    _OnBreakpointRequested.ExecuteIfBound(InTarget, InIndex);
}
