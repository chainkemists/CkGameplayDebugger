#include "CkCrowdDebugger/Window/SCkCrowdDebugger_ViewportPanel.h"

#include "CkCrowdDebugger/Commands/CkCrowdDebugger_PathNetworkCommand.h"
#include "CkCrowdDebugger/Settings/CkCrowdDebuggerSettings.h"
#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"
#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Utils.h"

#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkCrowdDebugger/Window/CkCrowdDebugger_PanelAxes.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkPmg/CkPmg_Utils_FlatShapes.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformTime.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	auto ColorForStatus(ECkCrowdDebugger_AgentStatus InStatus) -> FLinearColor
	{
		switch (InStatus)
		{
			case ECkCrowdDebugger_AgentStatus::Walking:     return CkStyle::Info();
			case ECkCrowdDebugger_AgentStatus::Idle:        return CkStyle::TextDim();
			case ECkCrowdDebugger_AgentStatus::Replanning:  return CkStyle::Warn();
			case ECkCrowdDebugger_AgentStatus::Failed:      return CkStyle::Err();
			case ECkCrowdDebugger_AgentStatus::Asleep:      return CkStyle::TextMute();
			case ECkCrowdDebugger_AgentStatus::PlayerProxy: return CkStyle::Accent();
			default:                                        return CkStyle::TextDim();
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	if (_ViewModel.IsValid())
	{
		// Cross-debugger sync resolves to an agent → one-shot re-center on it.
		_OnFrameRequestedHandle = _ViewModel->OnFrameSelectedAgentRequested.AddSP(
			SharedThis(this), &SCkCrowdDebugger_ViewportPanel::FrameSelectedAgent);
		_OnSelectedAgentChangedHandle = _ViewModel->OnSelectedAgentChanged.AddSP(
			SharedThis(this), &SCkCrowdDebugger_ViewportPanel::OnSelectedAgentChanged);
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkStyle::BgRoot()))
		.Padding(FMargin(0.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("TOP-DOWN VIEWPORT")))
					.ColorAndOpacity(FSlateColor(CkStyle::PaneHeadingColor()))
					.Font_Static(&ck::crowd_debugger_axes::Get_Font_PaneHeading)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("wheel zoom · drag pan · LMB select · RMB sidewalk move · F follow · Home fit")))
					.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
					.Font_Static(&ck::crowd_debugger_axes::Get_Font_Small)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceS, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("PATH")))
					.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
					.Font_Static(&ck::crowd_debugger_axes::Get_Font_Small)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(72.0f)
					[
						SNew(SSlider)
						.MinValue(0.0f)
						.MaxValue(1.0f)
						.ToolTipText(FText::FromString(TEXT("0 disables rendering")))
						.Value_Lambda([]() -> float
						{
							const auto* Settings = GetDefault<UCkCrowdDebuggerSettings>();
							return Settings != nullptr ? FMath::Clamp(Settings->PathNetworkOpacity, 0.0f, 1.0f) : 0.0f;
						})
						.OnValueChanged_Lambda([](float InOpacity)
						{
							auto* Settings = GetMutableDefault<UCkCrowdDebuggerSettings>();
							if (Settings == nullptr)
							{ return; }

							Settings->PathNetworkOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
							Settings->SaveConfig();
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
				[
					SNew(SCkDebug_IconToggle)
					.IconId(TEXT("Target"))
					.Label(FText::FromString(TEXT("Selected Trouble")))
					.ToolTip(FText::FromString(TEXT("Show the selected agent's retained path-trouble marker, attempted goal, dashed line, status, and Euclidean distance in this viewport.")))
					.IsOn_Lambda([]()
					{
						const auto* Settings = GetDefault<UCkCrowdDebuggerSettings>();
						return Settings == nullptr || Settings->ShowSelectedPathTroubleOverlay;
					})
					.OnStateChanged_Lambda([](bool InIsOn)
					{
						auto* Settings = GetMutableDefault<UCkCrowdDebuggerSettings>();
						if (Settings == nullptr)
						{ return; }

						Settings->ShowSelectedPathTroubleOverlay = InIsOn;
						Settings->SaveConfig();
					})
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNullWidget::NullWidget
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebugger_ViewportPanel::~SCkCrowdDebugger_ViewportPanel()
{
	if (_ViewModel.IsValid() && _OnFrameRequestedHandle.IsValid())
	{
		_ViewModel->OnFrameSelectedAgentRequested.Remove(_OnFrameRequestedHandle);
	}
	if (_ViewModel.IsValid() && _OnSelectedAgentChangedHandle.IsValid())
	{
		_ViewModel->OnSelectedAgentChanged.Remove(_OnSelectedAgentChangedHandle);
	}
	ClearIntentPathFade();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkCrowdDebugger_ViewportPanel::
	Tick(
		const FGeometry& AllottedGeometry,
		double InCurrentTime,
		float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (NOT _ViewModel.IsValid())
	{
		ClearIntentPathFade();
		return;
	}

	const auto* Selected = _ViewModel->Get_SelectedSnapshot();
	if (Selected == nullptr)
	{
		ClearIntentPathFade();
		return;
	}

	// A current request always wins over retained history from an earlier re-path in the same goal.
	if (Selected->Status == ECkCrowdDebugger_AgentStatus::Replanning)
	{
		_IntentPathFadeAgentPosition = Selected->Position;
		_IntentPathFadeGoal = Selected->ActiveGoal;
		const auto IsCurrentSidewalkFallback =
			Selected->HasPathTroubleEvent
			&& Selected->TroubleNavigationStatus == ECk_Nav_PathStatus::Pending;
		_IntentPathFadeLabel = IsCurrentSidewalkFallback
			? Selected->PathTroubleSummary
			: FString(TEXT("UNREAL NAV: Pending"));
		// KEPT LOCAL — canvas semantics. On the 2D map the intent line encodes WHICH routing layer
		// gave up (sidewalk fallback vs plain nav), and the hue is the only channel carrying that; a
		// Warn/Err role would collapse two different failures into one look. Same class as the AStar
		// cell states / Map fog the consolidation ruling keeps out of CkStyle::.
		_IntentPathFadeColor = IsCurrentSidewalkFallback
			? FLinearColor(1.0f, 0.35f, 0.05f, 1.0f)
			: CkStyle::Warn();
		_IntentPathFadeEventTimeSeconds = -1.0;
		_IntentPathFadeAlpha = 1.0f;
		_HasIntentPathFade = true;
		return;
	}

	if (Selected->HasPathTroubleEvent)
	{
		_IntentPathFadeAgentPosition = Selected->PathTroubleAgentPosition;
		_IntentPathFadeGoal = Selected->PathTroubleGoal;
		_IntentPathFadeLabel = Selected->PathTroubleSummary;
		_IntentPathFadeEventTimeSeconds = Selected->PathTroubleEventTimeSeconds;

		const auto HasNavigationTrouble =
			Selected->TroubleNavigationStatus == ECk_Nav_PathStatus::Partial
			|| Selected->TroubleNavigationStatus == ECk_Nav_PathStatus::Failed;
		// KEPT LOCAL — canvas semantics, three-way: sidewalk+nav both failed (magenta), sidewalk only
		// (orange), nav only (red). Three roles would have to be invented to say the same thing, and
		// they would mean nothing outside this map.
		_IntentPathFadeColor = Selected->HadPathNetworkFailure
			? (HasNavigationTrouble
				? FLinearColor(1.0f, 0.05f, 0.60f, 1.0f)
				: FLinearColor(1.0f, 0.35f, 0.05f, 1.0f))
			: FLinearColor(1.0f, 0.05f, 0.05f, 1.0f);

		const auto EventAge = FPlatformTime::Seconds() - _IntentPathFadeEventTimeSeconds;
		_IntentPathFadeAlpha = static_cast<float>(FMath::Clamp(
				1.0 - EventAge / ck::FFragment_CrowdAgent_PathTrouble::FadeDurationSeconds,
				0.0,
				1.0));
		_HasIntentPathFade = _IntentPathFadeAlpha > 0.0f;
		if (NOT _HasIntentPathFade)
		{ ClearIntentPathFade(); }
		return;
	}

	ClearIntentPathFade();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkCrowdDebugger_ViewportPanel::
	OnSelectedAgentChanged(FCk_Handle)
	-> void
{
	ClearIntentPathFade();
}

auto
	SCkCrowdDebugger_ViewportPanel::
	ClearIntentPathFade()
	-> void
{
	_IntentPathFadeAgentPosition = FVector::ZeroVector;
	_IntentPathFadeGoal = FVector::ZeroVector;
	_IntentPathFadeLabel.Empty();
	_IntentPathFadeColor = FLinearColor::Red;
	_IntentPathFadeEventTimeSeconds = -1.0;
	_IntentPathFadeAlpha = 0.0f;
	_HasIntentPathFade = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
	-> int32
{
	auto RetLayerId = SCompoundWidget::OnPaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (NOT _ViewModel.IsValid())
	{ return RetLayerId; }

	const FVector2D PanelSize = AllottedGeometry.GetLocalSize();
	if (PanelSize.X < 8.0 || PanelSize.Y < 8.0)
	{ return RetLayerId; }

	const auto& Nav    = _ViewModel->Get_NavmeshStatus();
	const auto& Agents = _ViewModel->Get_AllAgents();

	// --- World XY extent to frame: navmesh bounds, else agents' bbox -------------------------------
	FVector2D WMin = FVector2D::ZeroVector;
	FVector2D WMax = FVector2D::ZeroVector;
	auto HaveExtent = false;

	if (Nav._NavBoundsValid)
	{
		WMin = FVector2D(Nav._NavBoundsMin.X, Nav._NavBoundsMin.Y);
		WMax = FVector2D(Nav._NavBoundsMax.X, Nav._NavBoundsMax.Y);
		HaveExtent = true;
	}
	else if (Agents.Num() > 0)
	{
		WMin = FVector2D(Agents[0].Position.X, Agents[0].Position.Y);
		WMax = WMin;
		for (const auto& Agent : Agents)
		{
			WMin.X = FMath::Min(WMin.X, Agent.Position.X);
			WMin.Y = FMath::Min(WMin.Y, Agent.Position.Y);
			WMax.X = FMath::Max(WMax.X, Agent.Position.X);
			WMax.Y = FMath::Max(WMax.Y, Agent.Position.Y);
		}
		WMin -= FVector2D(300.0, 300.0);
		WMax += FVector2D(300.0, 300.0);
		HaveExtent = true;
	}

	if (NOT HaveExtent)
	{ return RetLayerId; }

	constexpr auto MinExtent = 200.0;
	if (WMax.X - WMin.X < MinExtent)
	{ const auto C = (WMax.X + WMin.X) * 0.5; WMin.X = C - MinExtent * 0.5; WMax.X = C + MinExtent * 0.5; }
	if (WMax.Y - WMin.Y < MinExtent)
	{ const auto C = (WMax.Y + WMin.Y) * 0.5; WMin.Y = C - MinExtent * 0.5; WMax.Y = C + MinExtent * 0.5; }

	// --- View rotation: project world onto the camera's right (u) + forward (v) so forward = up -----
	// For yaw θ, UE forward = (cosθ, sinθ), camera right = (-sinθ, cosθ). Projecting d = (world-center)
	// onto those gives u = d·right, v = d·forward; v maps to screen-up. Looking straight down, this
	// puts world +X (north) up and +Y (east) right at yaw 0 — and crucially is NOT mirrored (the
	// earlier ad-hoc rotation negated the right axis, flipping the map left-right).
	const auto Center = FVector2D((WMin.X + WMax.X) * 0.5, (WMin.Y + WMax.Y) * 0.5);
	const auto Yaw = _ViewModel->Get_ViewYawValid()
		? FMath::DegreesToRadians(static_cast<double>(_ViewModel->Get_ViewYawDegrees()))
		: 0.0;
	const auto CosY = FMath::Cos(Yaw);
	const auto SinY = FMath::Sin(Yaw);

	const auto ToView = [&](double InX, double InY) -> FVector2D
	{
		const auto Dx = InX - Center.X;
		const auto Dy = InY - Center.Y;
		return FVector2D(-Dx * SinY + Dy * CosY, Dx * CosY + Dy * SinY);
	};

	// View-space AABB of the (rotated) world bbox — what we fit into the panel.
	FVector2D VMin, VMax;
	{
		const FVector2D Corners[4] = {
			ToView(WMin.X, WMin.Y), ToView(WMax.X, WMin.Y),
			ToView(WMax.X, WMax.Y), ToView(WMin.X, WMax.Y)};
		VMin = Corners[0];
		VMax = Corners[0];
		for (const auto& Corner : Corners)
		{
			VMin.X = FMath::Min(VMin.X, Corner.X);
			VMin.Y = FMath::Min(VMin.Y, Corner.Y);
			VMax.X = FMath::Max(VMax.X, Corner.X);
			VMax.Y = FMath::Max(VMax.Y, Corner.Y);
		}
	}

	constexpr auto Pad = 14.0;
	const auto ViewW  = FMath::Max(VMax.X - VMin.X, 1.0);
	const auto ViewH  = FMath::Max(VMax.Y - VMin.Y, 1.0);
	const auto Scale  = FMath::Min((PanelSize.X - 2.0 * Pad) / ViewW, (PanelSize.Y - 2.0 * Pad) / ViewH);
	const auto OriginX = (PanelSize.X - ViewW * Scale) * 0.5;
	const auto OriginY = (PanelSize.Y - ViewH * Scale) * 0.5;

	const auto PanelCx  = PanelSize.X * 0.5;
	const auto PanelCy  = PanelSize.Y * 0.5;

	// Effective zoom/pan: in follow mode, centre the selected agent in a ~_FollowWindowCm window;
	// otherwise use the user's manual zoom/pan. (Default framing is the agent's local area, not the
	// whole navmesh.) The agent's fit-screen position comes from the base transform.
	const auto* SelForFrame = _ViewModel->Get_SelectedSnapshot();
	auto EffZoom = _Zoom;
	auto EffPanX = _PanX;
	auto EffPanY = _PanY;
	if (_AutoFrame && SelForFrame != nullptr)
	{
		const auto MinPanelDim = FMath::Min(PanelSize.X, PanelSize.Y);
		EffZoom = FMath::Clamp((MinPanelDim / _FollowWindowCm) / Scale, 0.2, 40.0);
		const auto AgentView = ToView(SelForFrame->Position.X, SelForFrame->Position.Y);
		const auto AgentFitX = OriginX + (AgentView.X - VMin.X) * Scale;
		const auto AgentFitY = OriginY + (VMax.Y - AgentView.Y) * Scale;
		EffPanX = -(AgentFitX - PanelCx) * EffZoom;
		EffPanY = -(AgentFitY - PanelCy) * EffZoom;
	}

	const auto EffScale = Scale * EffZoom;

	// Cache the transform so the click/hit-test uses the exact mapping the dots are drawn with.
	_XformValid = true;
	_XfCenterX = Center.X; _XfCenterY = Center.Y;
	_XfCosY = CosY; _XfSinY = SinY;
	_XfVMinX = VMin.X; _XfVMaxY = VMax.Y;
	_XfScale = Scale; _XfOriginX = OriginX; _XfOriginY = OriginY;
	_XfPanelCx = PanelCx; _XfPanelCy = PanelCy;
	_XfZoom = EffZoom; _XfPanX = EffPanX; _XfPanY = EffPanY;

	// Auto-fit mapping, then the effective zoom/pan (zoom about the panel centre, plus a screen pan).
	const auto WorldToScreen = [&](double InX, double InY) -> FVector2D
	{
		const auto V = ToView(InX, InY);
		const auto Fx = OriginX + (V.X - VMin.X) * Scale;
		const auto Fy = OriginY + (VMax.Y - V.Y) * Scale;
		return FVector2D(PanelCx + (Fx - PanelCx) * EffZoom + EffPanX, PanelCy + (Fy - PanelCy) * EffZoom + EffPanY);
	};

	// A world-space circle stays a circle under the uniform scale/rotation, so draw it directly in
	// screen space: centre = mapped world centre, radius = worldRadius * effective scale.
	const auto DrawWorldCircle = [&](const FVector2D& InCentreWorld, double InWorldRadius, const FLinearColor& InColor, int32 InLayer, bool InDashed)
	{
		const auto Centre = WorldToScreen(InCentreWorld.X, InCentreWorld.Y);
		const auto RadiusPx = InWorldRadius * EffScale;
		const auto PointAt = [&](double InAngle) -> FVector2D
		{ return FVector2D(Centre.X + RadiusPx * FMath::Cos(InAngle), Centre.Y + RadiusPx * FMath::Sin(InAngle)); };

		if (InDashed)
		{
			// Alternating short arcs → a dashed ring (for hypothetical/predicted circles).
			constexpr auto DashSegments = 48;
			for (auto i = 0; i < DashSegments; i += 2)
			{
				auto Seg = TArray<FVector2D>{
					PointAt((2.0 * PI * i) / DashSegments),
					PointAt((2.0 * PI * (i + 1)) / DashSegments)};
				FSlateDrawElement::MakeLines(
					OutDrawElements, InLayer, AllottedGeometry.ToPaintGeometry(),
					Seg, ESlateDrawEffect::None, InColor, true, 1.5f);
			}
			return;
		}

		constexpr auto Segments = 32;
		auto Pts = TArray<FVector2D>{};
		Pts.Reserve(Segments + 1);
		for (auto i = 0; i <= Segments; ++i)
		{ Pts.Add(PointAt((2.0 * PI * i) / Segments)); }
		FSlateDrawElement::MakeLines(
			OutDrawElements, InLayer, AllottedGeometry.ToPaintGeometry(),
			Pts, ESlateDrawEffect::None, InColor, true, 1.5f);
	};

	const auto DrawDashedScreenLine = [&](
		const FVector2D& InStart,
		const FVector2D& InEnd,
		const FLinearColor& InColor,
		int32 InLayer,
		float InThickness)
	{
		const auto Delta = InEnd - InStart;
		const auto Length = Delta.Size();
		if (Length <= 1.0)
		{ return; }

		const auto Direction = Delta / Length;
		constexpr auto BaseDashLengthPx = 8.0;
		constexpr auto BaseGapLengthPx  = 5.0;
		constexpr auto MaxDashCount     = 256.0;
		const auto BasePatternLength = BaseDashLengthPx + BaseGapLengthPx;
		const auto PatternScale = FMath::Max(1.0, Length / (BasePatternLength * MaxDashCount));
		const auto DashLengthPx = BaseDashLengthPx * PatternScale;
		const auto GapLengthPx  = BaseGapLengthPx * PatternScale;
		for (auto DistancePx = 0.0; DistancePx < Length; DistancePx += DashLengthPx + GapLengthPx)
		{
			auto Segment = TArray<FVector2D>{
				InStart + Direction * DistancePx,
				InStart + Direction * FMath::Min(DistancePx + DashLengthPx, Length)};
			FSlateDrawElement::MakeLines(
				OutDrawElements, InLayer, AllottedGeometry.ToPaintGeometry(),
				Segment, ESlateDrawEffect::None, InColor, true, InThickness);
		}
	};

	const auto* FillBrush = CkStyle::GetFilledBrush();

	// --- Navmesh walkable triangles — very transparent green fill (the base layer) -----------------
	const auto& NavTris = _ViewModel->Get_NavTriVerts();
	if (NavTris.Num() >= 3)
	{
		const auto RenderTransform = AllottedGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();
		const auto FillColor = FColor(70, 200, 110, 38);

		auto Verts   = TArray<FSlateVertex>{};
		auto Indices = TArray<SlateIndex>{};
		Verts.Reserve(NavTris.Num());
		Indices.Reserve(NavTris.Num());

		for (auto i = 0; i + 2 < NavTris.Num(); i += 3)
		{
			for (auto k = 0; k < 3; ++k)
			{
				const auto S = WorldToScreen(NavTris[i + k].X, NavTris[i + k].Y);
				Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
					RenderTransform,
					FVector2f(static_cast<float>(S.X), static_cast<float>(S.Y)),
					FVector2f(0.0f, 0.0f),
					FillColor));
				Indices.Add(static_cast<SlateIndex>(Verts.Num() - 1));
			}
		}

		if (Indices.Num() >= 3 && FSlateApplication::IsInitialized())
		{
			if (auto* Renderer = FSlateApplication::Get().GetRenderer())
			{
				const auto Handle = Renderer->GetResourceHandle(*FillBrush);
				FSlateDrawElement::MakeCustomVerts(
					OutDrawElements, RetLayerId + 1, Handle, Verts, Indices, nullptr, 0, 0);
			}
		}
	}

	// --- Navmesh extent outline (rotated rectangle) ------------------------------------------------
	{
		auto Loop = TArray<FVector2D>{
			WorldToScreen(WMin.X, WMin.Y),
			WorldToScreen(WMax.X, WMin.Y),
			WorldToScreen(WMax.X, WMax.Y),
			WorldToScreen(WMin.X, WMax.Y),
			WorldToScreen(WMin.X, WMin.Y)};
		FSlateDrawElement::MakeLines(
			OutDrawElements, RetLayerId + 2, AllottedGeometry.ToPaintGeometry(),
			Loop, ESlateDrawEffect::None, CkStyle::OverlayOf(CkStyle::BorderStrong(), 0.9f), true, 1.0f);
	}

	// --- Path-network ribbons (below agents and planned paths) -------------------------------------
	const auto* Settings = GetDefault<UCkCrowdDebuggerSettings>();
	const auto PathNetworkOpacity = Settings != nullptr
		? FMath::Clamp(Settings->PathNetworkOpacity, 0.0f, 1.0f)
		: 0.0f;
	if (PathNetworkOpacity > 0.0f)
	{
		// KEPT LOCAL — canvas paint. The ribbon is a world feature (the sidewalk network), drawn under
		// the agents at a user-driven opacity; it is not a status, so no semantic role applies.
		const auto RibbonColor = FLinearColor(0.16f, 0.76f, 0.96f, PathNetworkOpacity);
		const auto& Ribbons = _ViewModel->Get_PathNetworkRibbons();
		for (const auto& Ribbon : Ribbons)
		{
			if (Ribbon.Points.Num() < 2 || Ribbon.HalfWidths.Num() != Ribbon.Points.Num())
			{ continue; }

			for (auto PointIndex = 0; PointIndex + 1 < Ribbon.Points.Num(); ++PointIndex)
			{
				const auto& Start = Ribbon.Points[PointIndex];
				const auto& End = Ribbon.Points[PointIndex + 1];
				auto Segment = TArray<FVector2D>{
					WorldToScreen(Start.X, Start.Y),
					WorldToScreen(End.X, End.Y)};
				const auto AverageHalfWidth = (Ribbon.HalfWidths[PointIndex] + Ribbon.HalfWidths[PointIndex + 1]) * 0.5f;
				const auto Thickness = FMath::Max(1.0f, 2.0f * AverageHalfWidth * static_cast<float>(EffScale));
				FSlateDrawElement::MakeLines(
					OutDrawElements, RetLayerId + 2, AllottedGeometry.ToPaintGeometry(),
					Segment, ESlateDrawEffect::None, RibbonColor, true, Thickness);
			}
		}
	}

	// --- Agents as dots (color by status; selected = bright + larger) ------------------------------
	const auto Selected = _ViewModel->Get_SelectedHandle();
	const auto SelectedValid = ck::IsValid(Selected);

	for (const auto& Agent : Agents)
	{
		const auto Screen = WorldToScreen(Agent.Position.X, Agent.Position.Y);
		const auto IsSelected = SelectedValid && Agent.Handle == Selected;

		auto Color    = ColorForStatus(Agent.Status);
		auto DotPx    = 6.0f;
		auto DotLayer = RetLayerId + 3;
		if (IsSelected)
		{
			// KEPT LOCAL — canvas paint. The selected dot has to out-read every status hue on the map at
			// once; CkStyle::Selection() is a UI-chrome blue that disappears against the navmesh fill.
			Color = FLinearColor(1.0f, 0.90f, 0.20f, 1.0f);
			DotPx = 11.0f;
			DotLayer = RetLayerId + 4;
		}

		const auto TopLeft = FVector2f(
			static_cast<float>(Screen.X) - DotPx * 0.5f,
			static_cast<float>(Screen.Y) - DotPx * 0.5f);

		FSlateDrawElement::MakeBox(
			OutDrawElements, DotLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(DotPx, DotPx), FSlateLayoutTransform(TopLeft)),
			FillBrush, ESlateDrawEffect::None, Color);
	}

	// --- Player marker: pawn chevron + view-camera wedge --------------------------------------------
	// The user's constant orientation anchor — without it, "where are the NPCs
	// relative to ME" is unanswerable from the map.
	{
		const auto MarkerLayer = RetLayerId + 4;

		// World direction (yaw degrees) -> screen direction under the map's camera-aligned
		// rotation (screen up = view forward; screen Y grows downward).
		const auto YawToScreenDir = [&](float InYawDegrees) -> FVector2D
		{
			const auto YawRad = FMath::DegreesToRadians(static_cast<double>(InYawDegrees));
			const auto Dx = FMath::Cos(YawRad);
			const auto Dy = FMath::Sin(YawRad);
			const auto ViewX = -Dx * SinY + Dy * CosY;
			const auto ViewY =  Dx * CosY + Dy * SinY;
			return FVector2D(ViewX, -ViewY).GetSafeNormal();
		};

		if (_ViewModel->Get_PlayerPawnValid())
		{
			const auto PawnPos   = _ViewModel->Get_PlayerPawnPosition();
			const auto Screen    = WorldToScreen(PawnPos.X, PawnPos.Y);
			const auto Dir       = YawToScreenDir(_ViewModel->Get_PlayerPawnYawDegrees());
			const auto Perp      = FVector2D(-Dir.Y, Dir.X);
			const auto PlayerTint = CkStyle::Accent();

			// Chevron: tip forward, notched tail — reads as "player, facing this way".
			auto Chevron = TArray<FVector2D>{
				Screen + Dir * 12.0,
				Screen - Dir * 7.0 + Perp * 8.0,
				Screen - Dir * 3.0,
				Screen - Dir * 7.0 - Perp * 8.0,
				Screen + Dir * 12.0};
			FSlateDrawElement::MakeLines(
				OutDrawElements, MarkerLayer, AllottedGeometry.ToPaintGeometry(),
				Chevron, ESlateDrawEffect::None, PlayerTint, true, 2.5f);
		}

		// While ejected the flying editor camera is somewhere else entirely — mark it
		// with a small FOV wedge so the user can tell map-space from camera-space.
		if (_ViewModel->Get_ViewCameraValid() && _ViewModel->Get_ViewYawValid())
		{
			const auto CamPos  = _ViewModel->Get_ViewCameraPosition();
			const auto Screen  = WorldToScreen(CamPos.X, CamPos.Y);
			const auto Dir     = YawToScreenDir(_ViewModel->Get_ViewYawDegrees());
			const auto CamTint = CkStyle::OverlayOf(CkStyle::TextDim(), 0.9f);

			const auto WedgeHalfAngleRad = FMath::DegreesToRadians(30.0);
			const auto RotateDir = [&](double InRad) -> FVector2D
			{
				const auto C = FMath::Cos(InRad);
				const auto S = FMath::Sin(InRad);
				return FVector2D(Dir.X * C - Dir.Y * S, Dir.X * S + Dir.Y * C);
			};

			auto Wedge = TArray<FVector2D>{
				Screen + RotateDir(-WedgeHalfAngleRad) * 18.0,
				Screen,
				Screen + RotateDir(WedgeHalfAngleRad) * 18.0};
			FSlateDrawElement::MakeLines(
				OutDrawElements, MarkerLayer, AllottedGeometry.ToPaintGeometry(),
				Wedge, ESlateDrawEffect::None, CamTint, true, 1.5f);
		}
	}

	// --- Selected-agent orbit-diagnosis overlay (the 2D twin of the in-world rings) -----------------
	const auto* Sel = _ViewModel->Get_SelectedSnapshot();
	if (Sel != nullptr)
	{
		const auto OverlayLayer = RetLayerId + 5;
		const auto Color_Arrival = CkStyle::Ok();   // green
		const auto Color_Orbit   = CkStyle::Err();  // red
		const auto Color_Turn    = CkStyle::Info(); // blue
		const auto Color_Vel     = CkStyle::Warn(); // yellow
		// KEPT LOCAL — canvas paint: the planned-path polyline must stay distinguishable from the four
		// role-toned diagnosis rings above it while sharing the map with them.
		const auto Color_Path     = FLinearColor(0.48f, 0.64f, 1.0f, 0.9f); // planned-path light blue
		auto Color_GoalFail = _IntentPathFadeColor;
		Color_GoalFail.A = _IntentPathFadeAlpha;

		// Planned path — the nav waypoint polyline the agent is following.
		if (Sel->PlannedPath.Num() >= 2)
		{
			auto PathPts = TArray<FVector2D>{};
			PathPts.Reserve(Sel->PlannedPath.Num());
			for (const auto& Waypoint : Sel->PlannedPath)
			{ PathPts.Add(WorldToScreen(Waypoint.X, Waypoint.Y)); }
			FSlateDrawElement::MakeLines(
				OutDrawElements, OverlayLayer, AllottedGeometry.ToPaintGeometry(),
				PathPts, ESlateDrawEffect::None, Color_Path, true, 2.0f);
		}

		if (_HasIntentPathFade
			&& (Settings == nullptr || Settings->ShowSelectedPathTroubleOverlay))
		{
			const auto AgentScreen = WorldToScreen(
				_IntentPathFadeAgentPosition.X,
				_IntentPathFadeAgentPosition.Y);
			const auto GoalScreen = WorldToScreen(
				_IntentPathFadeGoal.X,
				_IntentPathFadeGoal.Y);
			DrawDashedScreenLine(AgentScreen, GoalScreen, Color_GoalFail, OverlayLayer + 1, 2.0f);

			const auto GoalTL = FVector2f(static_cast<float>(GoalScreen.X) - 4.0f, static_cast<float>(GoalScreen.Y) - 4.0f);
			FSlateDrawElement::MakeBox(
				OutDrawElements, OverlayLayer + 2,
				AllottedGeometry.ToPaintGeometry(FVector2f(8.0f, 8.0f), FSlateLayoutTransform(GoalTL)),
				FillBrush, ESlateDrawEffect::None, Color_GoalFail);

			const auto LabelPosition = (AgentScreen + GoalScreen) * 0.5 + FVector2D(4.0, -14.0);
			const auto GoalDistanceCm = FVector::Dist(
				_IntentPathFadeAgentPosition,
				_IntentPathFadeGoal);
			FSlateDrawElement::MakeText(
				OutDrawElements, OverlayLayer + 2,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(360.0f, 28.0f),
					FSlateLayoutTransform(FVector2f(
						static_cast<float>(LabelPosition.X),
						static_cast<float>(LabelPosition.Y)))),
				FString::Printf(
					TEXT("%s | %.0f cm (3D)"),
					*_IntentPathFadeLabel,
					GoalDistanceCm),
				// Canvas label: the in-viewport overlay is painted every frame, so ScaledFont here is live
				// by construction. 9pt is the body role.
				ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()),
				ESlateDrawEffect::None,
				Color_GoalFail);
		}

		// Goal rings — only while actually heading to a goal.
		if (Sel->IsWalking)
		{
			const auto GoalWorld = FVector2D(Sel->ActiveGoal.X, Sel->ActiveGoal.Y);
			DrawWorldCircle(GoalWorld, Sel->ActiveArrivalRadius, Color_Arrival, OverlayLayer, /*dashed*/ false);
			if (Sel->MaxTurnRate > 0.0f)
			{ DrawWorldCircle(GoalWorld, Sel->MaxSpeed / Sel->MaxTurnRate, Color_Orbit, OverlayLayer, /*dashed*/ true); }

			const auto GoalScreen = WorldToScreen(GoalWorld.X, GoalWorld.Y);
			const auto GoalTL = FVector2f(static_cast<float>(GoalScreen.X) - 3.0f, static_cast<float>(GoalScreen.Y) - 3.0f);
			FSlateDrawElement::MakeBox(
				OutDrawElements, OverlayLayer + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(6.0f, 6.0f), FSlateLayoutTransform(GoalTL)),
				FillBrush, ESlateDrawEffect::None, FLinearColor::White);
		}

		// Turn-radius circle + velocity vector — only while moving.
		const auto Speed2D = FMath::Sqrt(Sel->Velocity.X * Sel->Velocity.X + Sel->Velocity.Y * Sel->Velocity.Y);
		if (Speed2D > 5.0)
		{
			const auto DirX = Sel->Velocity.X / Speed2D;
			const auto DirY = Sel->Velocity.Y / Speed2D;
			const auto AgentWorld = FVector2D(Sel->Position.X, Sel->Position.Y);

			if (Sel->MaxTurnRate > 0.0f)
			{
				// Both tangent turn-radius circles — the agent can curve either way, and rides the
				// edge of both (they touch at the agent). Faded + dashed so they read as hypothetical.
				const auto TurnRadius = Speed2D / Sel->MaxTurnRate;
				const auto TurnFaded = CkStyle::OverlayOf(Color_Turn, 0.5f);
				const auto LeftCentre  = FVector2D(AgentWorld.X - DirY * TurnRadius, AgentWorld.Y + DirX * TurnRadius);
				const auto RightCentre = FVector2D(AgentWorld.X + DirY * TurnRadius, AgentWorld.Y - DirX * TurnRadius);
				DrawWorldCircle(LeftCentre,  TurnRadius, TurnFaded, OverlayLayer, /*dashed*/ true);
				DrawWorldCircle(RightCentre, TurnRadius, TurnFaded, OverlayLayer, /*dashed*/ true);
			}

			const auto VelLen = FMath::Min(Speed2D, 300.0);
			auto VelLine = TArray<FVector2D>{
				WorldToScreen(AgentWorld.X, AgentWorld.Y),
				WorldToScreen(AgentWorld.X + DirX * VelLen, AgentWorld.Y + DirY * VelLen)};
			FSlateDrawElement::MakeLines(
				OutDrawElements, OverlayLayer + 1, AllottedGeometry.ToPaintGeometry(),
				VelLine, ESlateDrawEffect::None, Color_Vel, true, 2.0f);
		}
	}

	return RetLayerId + 6;
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::FitScreen(double InX, double InY) const -> FVector2D
{
	const auto Dx = InX - _XfCenterX;
	const auto Dy = InY - _XfCenterY;
	const auto U = -Dx * _XfSinY + Dy * _XfCosY;
	const auto V =  Dx * _XfCosY + Dy * _XfSinY;
	return FVector2D(_XfOriginX + (U - _XfVMinX) * _XfScale, _XfOriginY + (_XfVMaxY - V) * _XfScale);
}

auto SCkCrowdDebugger_ViewportPanel::ToScreen(double InX, double InY) const -> FVector2D
{
	const auto F = FitScreen(InX, InY);
	return FVector2D(
		_XfPanelCx + (F.X - _XfPanelCx) * _XfZoom + _XfPanX,
		_XfPanelCy + (F.Y - _XfPanelCy) * _XfZoom + _XfPanY);
}

auto SCkCrowdDebugger_ViewportPanel::ScreenToWorld(double InScreenX, double InScreenY) const -> FVector2D
{
	// Inverse of ToScreen: undo zoom/pan, then the fit, then the (involutive) camera rotation.
	const auto Fx = _XfPanelCx + (InScreenX - _XfPanelCx - _XfPanX) / _XfZoom;
	const auto Fy = _XfPanelCy + (InScreenY - _XfPanelCy - _XfPanY) / _XfZoom;
	const auto U  = _XfVMinX + (Fx - _XfOriginX) / _XfScale;
	const auto V  = _XfVMaxY - (Fy - _XfOriginY) / _XfScale;
	const auto Dx = -_XfSinY * U + _XfCosY * V;
	const auto Dy =  _XfCosY * U + _XfSinY * V;
	return FVector2D(_XfCenterX + Dx, _XfCenterY + Dy);
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::OnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
	-> FReply
{
	const auto Button = InMouseEvent.GetEffectingButton();

	// Middle drag pans immediately (and drops follow mode so the pan sticks).
	if (Button == EKeys::MiddleMouseButton)
	{
		const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		// Hand off the current follow transform to the manual zoom/pan so panning starts from here.
		if (_AutoFrame)
		{
			_Zoom = _XfZoom;
			_PanX = _XfPanX;
			_PanY = _XfPanY;
			_AutoFrame = false;
		}
		_IsPanning = true;
		_LastDragX = Local.X;
		_LastDragY = Local.Y;
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	// RMB defers: pan engages only past the drag threshold (OnMouseMove); an
	// under-threshold release commands the selected agent to the point (RTS-style).
	if (Button == EKeys::RightMouseButton)
	{
		const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		_RmbDown  = true;
		_RmbDownX = Local.X;
		_RmbDownY = Local.Y;
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	if (Button != EKeys::LeftMouseButton)
	{ return FReply::Unhandled(); }

	// Left click selects the nearest dot (and focuses the panel so F / Home work).
	auto Reply = FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);

	if (NOT _ViewModel.IsValid() || NOT _XformValid)
	{ return Reply; }

	const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const auto& Agents = _ViewModel->Get_AllAgents();

	auto Best = FCk_Handle{};
	auto BestDistSq = -1.0;
	constexpr auto ThresholdSq = 14.0 * 14.0; // ~14px pick radius
	for (const auto& Agent : Agents)
	{
		const auto Screen = ToScreen(Agent.Position.X, Agent.Position.Y);
		const auto DistSq = FVector2D::DistSquared(Screen, Local);
		if (DistSq <= ThresholdSq && (BestDistSq < 0.0 || DistSq < BestDistSq))
		{ BestDistSq = DistSq; Best = Agent.Handle; }
	}

	if (ck::IsValid(Best))
	{
		_ViewModel->Set_SelectedHandle(Best);

		// The agent list applies sync-bus selections as ESelectInfo::Direct and deliberately
		// never re-broadcasts those, so a viewport-picked agent would stay invisible to the
		// ECS / GOAP debuggers. Broadcast here — this IS the user-driven selection. Source name
		// must stay "CrowdDebugger" so our own AgentListPanel receive side ignores it.
		ck::DebugSelectionSync::Broadcast(Best, TEXT("CrowdDebugger"));
	}

	return Reply;
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::OnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
	-> FReply
{
	const auto Button = InMouseEvent.GetEffectingButton();

	if (Button == EKeys::RightMouseButton && _RmbDown)
	{
		const auto WasPanning = _IsPanning;
		_RmbDown   = false;
		_IsPanning = false;

		// Under-threshold release = command. Auto-arms the debug override so no
		// separate "Take Control" click is needed.
		if (NOT WasPanning && _ViewModel.IsValid() && _XformValid)
		{
			const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			auto SelectedHandle = _ViewModel->Get_SelectedHandle();
			auto Agent = UCk_Utils_CrowdAgent_UE::Cast(SelectedHandle);
			const auto* Sel = _ViewModel->Get_SelectedSnapshot();
			if (ck::IsValid(Agent) && Sel != nullptr)
			{
				const auto World = ScreenToWorld(Local.X, Local.Y);
				const auto RawDestination = FVector(World.X, World.Y, Sel->Position.Z);
				auto Destination = FVector{};
				if (NOT ck::crowd_debugger::Try_IssueManualMove(
					Agent, RawDestination, Destination))
				{ return FReply::Handled(); }

				// Destination acknowledgment ping in the WORLD (transient PMG ring,
				// self-destroys) — a map command should still read in the viewport.
				// Lifted slightly so the ring doesn't z-fight the floor.
				if (auto* AgentWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Agent);
					ck::IsValid(AgentWorld))
				{
					UCk_Utils_Pmg_FlatShapes::DrawFilledRing(
						AgentWorld,
						Destination + FVector{0.0f, 0.0f, 4.0f},
						/*InOuterRadius=*/60.0f,
						/*InInnerRadius=*/45.0f,
						/*InSegments=*/32,
						// KEPT LOCAL — in-world PMG paint, not a Slate surface: the destination ping is a
						// game-world marker read against arbitrary level art, not against the debugger palette.
						FLinearColor{0.15f, 1.0f, 0.35f, 0.85f},
						/*InDrawLines=*/false,
						/*InLineThickness=*/2.0f,
						ECk_Plane_Axis::XY,
						/*InDuration=*/1.5f);
				}
			}
		}

		return FReply::Handled().ReleaseMouseCapture();
	}

	if (Button == EKeys::MiddleMouseButton && _IsPanning)
	{
		_IsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::OnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
	-> FReply
{
	const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	// Engage RMB pan once the cursor leaves the click-vs-drag threshold.
	if (_RmbDown && NOT _IsPanning)
	{
		const auto DistSq = FVector2D::DistSquared(Local, FVector2D(_RmbDownX, _RmbDownY));
		const auto Threshold = FSlateApplication::Get().GetDragTriggerDistance();
		if (DistSq > Threshold * Threshold)
		{
			// Hand off the current follow transform so panning starts from here.
			if (_AutoFrame)
			{
				_Zoom = _XfZoom;
				_PanX = _XfPanX;
				_PanY = _XfPanY;
				_AutoFrame = false;
			}
			_IsPanning = true;
			_LastDragX = _RmbDownX;
			_LastDragY = _RmbDownY;
		}
	}

	if (NOT _IsPanning)
	{ return FReply::Unhandled(); }

	_PanX += Local.X - _LastDragX;
	_PanY += Local.Y - _LastDragY;
	_LastDragX = Local.X;
	_LastDragY = Local.Y;
	return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::OnMouseWheel(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
	-> FReply
{
	const auto WheelFactor = FMath::Pow(1.1, static_cast<double>(InMouseEvent.GetWheelDelta()));

	// Follow mode: zoom by resizing the window around the agent (wheel up = zoom in = smaller window).
	if (_AutoFrame)
	{
		_FollowWindowCm = FMath::Clamp(_FollowWindowCm / WheelFactor, 100.0, 20000.0);
		return FReply::Handled();
	}

	const auto OldZoom = _Zoom;
	_Zoom = FMath::Clamp(OldZoom * WheelFactor, 0.2, 40.0);

	// Keep the world point under the cursor fixed while zooming.
	const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Size  = InGeometry.GetLocalSize();
	const auto Cx = Size.X * 0.5;
	const auto Cy = Size.Y * 0.5;
	const auto Ratio = _Zoom / OldZoom;
	_PanX = Local.X - Cx - (Local.X - Cx - _PanX) * Ratio;
	_PanY = Local.Y - Cy - (Local.Y - Cy - _PanY) * Ratio;
	return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::OnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
	-> FReply
{
	const auto Key = InKeyEvent.GetKey();
	if (Key == EKeys::F)
	{ FrameSelectedAgent(); return FReply::Handled(); }
	if (Key == EKeys::Home)
	{ _AutoFrame = false; _Zoom = 1.0; _PanX = 0.0; _PanY = 0.0; return FReply::Handled(); }
	return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ViewportPanel::FrameSelectedAgent() -> void
{
	// Re-engage follow mode at the default ~8 m window; OnPaint centres + zooms on the selected agent.
	_AutoFrame = true;
	_FollowWindowCm = 800.0;
}

// --------------------------------------------------------------------------------------------------------------------
