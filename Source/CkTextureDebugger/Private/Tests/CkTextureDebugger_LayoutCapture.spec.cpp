#include "CkTextureDebugger/Window/SCkTextureDebugger_TextureHealthTable.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_SceneAuditTable.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_DiagnosticPages.h"
#include <type_traits>

#include "CkSlateLayout/SCkUiTable.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/Views/SListView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_texture_debugger_layout_capture_tests
{
    struct FCaptureCase
    {
        FIntPoint LogicalViewport;
        float DrawScale = 1.0f;
    };

    struct FCaptureResult
    {
        bool Succeeded = false;
        FString Failure;
        FVector2D DesiredSize = FVector2D::ZeroVector;
        int32 WidgetCount = 0;
        int32 InstantiatedRowCount = 0;
        double AverageDrawMilliseconds = 0.0;
        double MaximumDrawMilliseconds = 0.0;
        TArray<double> BatchAverageDrawMilliseconds;
    };

    auto Find_HealthListWidget(const TSharedRef<SWidget>& InWidget) -> TSharedPtr<SWidget>
    {
        if (InWidget->GetTag() == FName{TEXT("Ck.TextureHealth.List")}) { return InWidget; }
        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            if (const auto Found = Find_HealthListWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto Count_Widgets(const TSharedRef<SWidget>& InWidget) -> int32
    {
        auto Count = 1;
        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        { Count += Count_Widgets(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); }
        return Count;
    }

    using FLegacyHealthList = SListView<TSharedPtr<SCkTextureDebugger_TextureHealthTable::FRow>>;
    using FAuthoredHealthList = SListView<SCkUiTable::FRecord>;

    auto Find_LegacyHealthList(const TSharedRef<SCkTextureDebugger_TextureHealthTable>& InTable) -> TSharedPtr<FLegacyHealthList>
    {
        const auto ListWidget = Find_HealthListWidget(InTable);
        return ListWidget.IsValid()
            ? StaticCastSharedPtr<FLegacyHealthList>(ListWidget)
            : TSharedPtr<FLegacyHealthList>{};
    }

    auto Find_AuthoredHealthList(const TSharedRef<SCkTextureDebugger_TextureHealthTable>& InTable) -> TSharedPtr<FAuthoredHealthList>
    {
        const TSharedPtr<SCkUiTable> AuthoredTable = InTable->Get_AuthoredTable();
        return AuthoredTable.IsValid() ? AuthoredTable->GetList() : TSharedPtr<FAuthoredHealthList>{};
    }

    auto Get_TextField(const SCkUiTable::FRecord& InRecord, const FString& InField) -> FString
    {
        const FCkUiFieldValue* Field = InRecord.IsValid() ? InRecord->FindField(InField) : nullptr;
        return Field != nullptr ? Field->Text.ToString() : FString{};
    }

    auto Get_CapturePhase() -> FString
    {
        auto Phase = FString{TEXT("Baseline")};
        if (NOT FParse::Value(FCommandLine::Get(), TEXT("CkTextureCapturePhase="), Phase))
        { Phase = FPlatformMisc::GetEnvironmentVariable(TEXT("CK_TEXTURE_CAPTURE_PHASE")); }
        Phase = FPaths::MakeValidFileName(Phase);
        return Phase.IsEmpty() ? FString{TEXT("Baseline")} : Phase;
    }

    auto Make_Snapshot(
        int32 InRowCount,
        TArray<TStrongObjectPtr<UStaticMeshComponent>>& OutComponents,
        TArray<TStrongObjectPtr<UTexture2D>>& OutTextures) -> FCkTextureDebugger_LoadedWorldSnapshot
    {
        auto Snapshot = FCkTextureDebugger_LoadedWorldSnapshot{};
        OutComponents.Reserve(InRowCount);
        OutTextures.Reserve(InRowCount);

        for (auto Index = 0; Index < InRowCount; ++Index)
        {
            auto Component = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>(GetTransientPackage())};
            auto Texture = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>(GetTransientPackage())};

            auto TextureRow = FCkTextureDebugger_TextureRow{};
            TextureRow.NavigationTarget = Texture.Get();
            TextureRow.Health.DisplayName = Index == 0
                ? TEXT("LayoutCapture_Texture_0000_With_A_Long_Runtime_Identity_For_Selected_Detail_Wrapping")
                : FString::Printf(TEXT("LayoutCapture_Texture_%04d"), Index);
            TextureRow.Health.AssetPath = FSoftObjectPath{Texture.Get()};
            TextureRow.Health.CookedWidth = 2048;
            TextureRow.Health.CookedHeight = 1024;
            TextureRow.Health.MipCount = 12;
            TextureRow.Health.ClassName = TEXT("Texture2D");
            TextureRow.Health.FormatName = TEXT("DXT5_LongFormatDescriptor_For_Capture");
            TextureRow.Health.LodGroupName = TEXT("World_NormalMap_With_Long_Streaming_Group_Name");
            TextureRow.Health.HasStreamingMetrics = true;
            TextureRow.Health.ResidentMipCount = 6 + Index % 5;
            TextureRow.Health.RequestedMipCount = 10;
            TextureRow.Health.StreamingAvailability = ECkTextureDebugger_StreamingAvailability::Available;

            auto Slot = FCkTextureDebugger_MaterialSlotRow{};
            Slot.SlotIndex = 0;
            Slot.DisplayName = Index == 0
                ? TEXT("LayoutCapture_Material_0000_With_A_Long_Resolved_Material_Slot_Name")
                : FString::Printf(TEXT("LayoutCapture_Material_%04d"), Index);
            Slot.Textures.Add(MoveTemp(TextureRow));

            auto ComponentRow = FCkTextureDebugger_ComponentRow{};
            ComponentRow.NavigationTarget = Component.Get();
            ComponentRow.ActorPath = FSoftObjectPath{FString::Printf(TEXT("/Game/LayoutCapture.Actor_%04d"), Index)};
            ComponentRow.ActorDisplayName = FString::Printf(TEXT("LayoutCapture_Actor_%04d"), Index);
            ComponentRow.ComponentDisplayName = FString::Printf(TEXT("LayoutCapture_Component_%04d"), Index);
            ComponentRow.ComponentClassName = TEXT("StaticMeshComponent");
            ComponentRow.Kind = ECkTextureDebugger_ComponentKind::StaticMesh;
            ComponentRow.MaterialSlots.Add(MoveTemp(Slot));

            Snapshot.Components.Add(MoveTemp(ComponentRow));
            OutComponents.Add(MoveTemp(Component));
            OutTextures.Add(MoveTemp(Texture));
        }

        return Snapshot;
    }

    auto Select_FirstRealRow(const TSharedRef<SCkTextureDebugger_TextureHealthTable>& InTable, const bool bUseYogaLayout) -> bool
    {
        if (bUseYogaLayout)
        {
            const TSharedPtr<FAuthoredHealthList> List = Find_AuthoredHealthList(InTable);
            if (NOT List.IsValid() || List->GetNumItemsBeingObserved() <= 0) { return false; }
            const auto Items = List->GetItems();
            const SCkUiTable::FRecord* LongItem = Items.FindByPredicate([](const SCkUiTable::FRecord& InItem)
            { return Get_TextField(InItem, TEXT("texture")).Contains(TEXT("With_A_Long_Runtime_Identity")); });
            if (LongItem == nullptr) { return false; }
            List->SetItemSelection(*LongItem, true, ESelectInfo::OnMouseClick);
        }
        else
        {
            const TSharedPtr<FLegacyHealthList> List = Find_LegacyHealthList(InTable);
            if (NOT List.IsValid() || List->GetNumItemsBeingObserved() <= 0) { return false; }
            const auto Items = List->GetItems();
            const auto* LongItem = Items.FindByPredicate([](const auto& InItem)
            { return InItem.IsValid() && InItem->Health.DisplayName.Contains(TEXT("With_A_Long_Runtime_Identity")); });
            if (LongItem == nullptr) { return false; }
            List->SetItemSelection(*LongItem, true, ESelectInfo::OnMouseClick);
        }
        return InTable->Get_Selection().IsSet();
    }

    template<typename TTable>
    auto Capture(
        const TSharedRef<TTable>& InTable,
        const FCaptureCase& InCase,
        const FString& InOutputPath,
        const bool bUseYogaLayout) -> FCaptureResult
    {
        auto Result = FCaptureResult{};
        if (NOT FApp::CanEverRender() || IsRunningDedicatedServer() || NOT FSlateApplication::IsInitialized())
        {
            Result.Failure = TEXT("Layout capture requires an initialized Slate application and a real RHI; -nullrhi, commandlets, and dedicated-server contexts cannot render this surface.");
            return Result;
        }

        const auto LogicalSize = FVector2D{
            static_cast<float>(InCase.LogicalViewport.X),
            static_cast<float>(InCase.LogicalViewport.Y)};
        const auto PhysicalSize = FIntPoint{
            FMath::RoundToInt(static_cast<float>(InCase.LogicalViewport.X) * InCase.DrawScale),
            FMath::RoundToInt(static_cast<float>(InCase.LogicalViewport.Y) * InCase.DrawScale)};
        if (PhysicalSize.X <= 0 || PhysicalSize.Y <= 0)
        {
            Result.Failure = TEXT("Capture case produced an invalid physical render-target size.");
            return Result;
        }

        const auto RenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>{NewObject<UTextureRenderTarget2D>(GetTransientPackage())};
        RenderTarget->ClearColor = FLinearColor::Transparent;
        RenderTarget->bAutoGenerateMips = false;
        RenderTarget->InitCustomFormat(PhysicalSize.X, PhysicalSize.Y, PF_B8G8R8A8, /*bInForceLinearGamma*/ false);
        RenderTarget->UpdateResourceImmediate(true);

        const TSharedRef<SVirtualWindow> VirtualWindow = SNew(SVirtualWindow).Size(LogicalSize);
        VirtualWindow->SetContent(InTable);
        VirtualWindow->Resize(LogicalSize);

        const TSharedRef<FHittestGrid> HitTestGrid = MakeShared<FHittestGrid>();
        auto* WidgetRenderer = new FWidgetRenderer(/*bUseGammaCorrection*/ false);
        if (WidgetRenderer == nullptr)
        {
            Result.Failure = TEXT("Could not allocate FWidgetRenderer.");
            return Result;
        }

        WidgetRenderer->SetIsPrepassNeeded(true);
        // FWidgetRenderer derives the logical root size from DrawSize / Scale. Supplying the physical
        // target size keeps the requested logical viewport stable at both capture densities.
        const auto DrawSize = FVector2D{static_cast<float>(PhysicalSize.X), static_cast<float>(PhysicalSize.Y)};
        // Off-screen drawing queues SVG sizes but does not service the RHI vector atlas.
        // Prime this scale, let Slate rasterize queued vector resources, then measure/capture.
        WidgetRenderer->DrawWindow(RenderTarget.Get(), HitTestGrid.Get(), VirtualWindow, InCase.DrawScale, DrawSize, 0.0f);
        {
            auto& SlateRenderer = *FSlateApplication::Get().GetRenderer();
            FSlateRenderer::FScopedAcquireDrawBuffer DrawBuffer{SlateRenderer};
            SlateRenderer.DrawWindows(DrawBuffer.GetDrawBuffer());
        }
        FlushRenderingCommands();
        constexpr auto WarmDrawIterations = 3;
        constexpr auto MeasuredDrawIterations = 30;
        constexpr auto MeasurementBatches = 3;
        auto TotalMeasuredMilliseconds = 0.0;

        for (auto BatchIndex = 0; BatchIndex < MeasurementBatches; ++BatchIndex)
        {
            for (auto WarmIndex = 0; WarmIndex < WarmDrawIterations; ++WarmIndex)
            { WidgetRenderer->DrawWindow(RenderTarget.Get(), HitTestGrid.Get(), VirtualWindow, InCase.DrawScale, DrawSize, 0.0f); }

            auto BatchMeasuredMilliseconds = 0.0;
            for (auto DrawIndex = 0; DrawIndex < MeasuredDrawIterations; ++DrawIndex)
            {
                const auto DrawStartSeconds = FPlatformTime::Seconds();
                WidgetRenderer->DrawWindow(RenderTarget.Get(), HitTestGrid.Get(), VirtualWindow, InCase.DrawScale, DrawSize, 0.0f);
                const auto DrawMilliseconds = (FPlatformTime::Seconds() - DrawStartSeconds) * 1000.0;
                BatchMeasuredMilliseconds += DrawMilliseconds;
                TotalMeasuredMilliseconds += DrawMilliseconds;
                Result.MaximumDrawMilliseconds = FMath::Max(Result.MaximumDrawMilliseconds, DrawMilliseconds);
            }
            Result.BatchAverageDrawMilliseconds.Add(BatchMeasuredMilliseconds / MeasuredDrawIterations);
            // Keep each batch self-contained, but deliberately exclude the render-thread synchronization
            // from the per-draw CPU statistic above.
            FlushRenderingCommands();
        }

        Result.DesiredSize = InTable->GetDesiredSize();
        Result.WidgetCount = Count_Widgets(InTable);
        if constexpr (std::is_same_v<TTable, SCkTextureDebugger_SceneAuditTable>
            || std::is_same_v<TTable, SCkTextureDebugger_MaterialInputsPage>)
        {
            const auto AuthoredTable = InTable->Get_AuthoredTable();
            const auto List = AuthoredTable.IsValid() ? AuthoredTable->GetList() : nullptr;
            if (!List.IsValid())
            {
                BeginCleanup(WidgetRenderer);
                Result.Failure = TEXT("The rendered page did not contain its authored list.");
                return Result;
            }
            for (const auto& Item : List->GetItems())
            {
                if (List->WidgetFromItem(Item).IsValid()) { ++Result.InstantiatedRowCount; }
            }
        }
        else if constexpr (std::is_same_v<TTable, SCkTextureDebugger_TextureHealthTable>)
        {
            if (bUseYogaLayout)
            {
                const TSharedPtr<FAuthoredHealthList> HealthList = Find_AuthoredHealthList(InTable);
                if (NOT HealthList.IsValid())
                {
                    BeginCleanup(WidgetRenderer);
                    Result.Failure = TEXT("The rendered authored table did not contain its expected SCkUiTable SListView.");
                    return Result;
                }
                for (const SCkUiTable::FRecord& Item : HealthList->GetItems())
                {
                    if (HealthList->WidgetFromItem(Item).IsValid()) { ++Result.InstantiatedRowCount; }
                }
            }
            else
            {
                const TSharedPtr<FLegacyHealthList> HealthList = Find_LegacyHealthList(InTable);
                if (NOT HealthList.IsValid())
                {
                    BeginCleanup(WidgetRenderer);
                    Result.Failure = TEXT("The rendered legacy table did not contain its expected SListView.");
                    return Result;
                }
                for (const TSharedPtr<SCkTextureDebugger_TextureHealthTable::FRow>& Item : HealthList->GetItems())
                {
                    if (HealthList->WidgetFromItem(Item).IsValid()) { ++Result.InstantiatedRowCount; }
                }
            }

        }

        auto* Resource = RenderTarget->GameThread_GetRenderTargetResource();
        if (Resource == nullptr)
        {
            BeginCleanup(WidgetRenderer);
            Result.Failure = TEXT("The off-screen render target did not expose a game-thread resource.");
            return Result;
        }

        auto Pixels = TArray<FColor>{};
        auto ReadFlags = FReadSurfaceDataFlags{RCM_UNorm, CubeFace_MAX};
        ReadFlags.SetLinearToGamma(false);
        if (NOT Resource->ReadPixels(Pixels, ReadFlags) || Pixels.Num() != PhysicalSize.X * PhysicalSize.Y)
        {
            BeginCleanup(WidgetRenderer);
            Result.Failure = TEXT("The off-screen Slate render did not return the expected BGRA pixel count.");
            return Result;
        }

        const auto Image = FImageView{Pixels.GetData(), PhysicalSize.X, PhysicalSize.Y, ERawImageFormat::BGRA8};
        if (NOT FImageUtils::SaveImageByExtension(*InOutputPath, Image))
        {
            BeginCleanup(WidgetRenderer);
            Result.Failure = FString::Printf(TEXT("Could not write PNG '%s'."), *InOutputPath);
            return Result;
        }

        BeginCleanup(WidgetRenderer);
        Result.AverageDrawMilliseconds = TotalMeasuredMilliseconds / (MeasuredDrawIterations * MeasurementBatches);
        Result.Succeeded = true;
        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_LayoutCapture_Surface,
    "Ck.TextureDebugger.LayoutCapture.Surface",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_LayoutCapture_Surface::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_layout_capture_tests;

    const auto Phase = Get_CapturePhase();
    const auto Viewports = TArray<FIntPoint>{FIntPoint{1280, 720}, FIntPoint{800, 600}, FIntPoint{640, 480}};
    const auto DrawScales = TArray<float>{1.0f, 1.5f};
    const auto RowCounts = TArray<int32>{0, 1, 1000};

    auto Succeeded = true;
    for (const auto RowCount : RowCounts)
    {
        auto Components = TArray<TStrongObjectPtr<UStaticMeshComponent>>{};
        auto Textures = TArray<TStrongObjectPtr<UTexture2D>>{};
        const auto Snapshot = Make_Snapshot(RowCount, Components, Textures);
        for (const auto bUseYogaLayout : {false, true})
        {
            const auto LayoutName = bUseYogaLayout ? TEXT("Authored") : TEXT("Legacy");
            const auto OutputDirectory = FPaths::Combine(
                FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("TextureDebugger"), TEXT("LayoutCapture"), Phase, LayoutName);
            if (NOT IFileManager::Get().DirectoryExists(*OutputDirectory)
                && NOT IFileManager::Get().MakeDirectory(*OutputDirectory, /*Tree*/ true))
            {
                AddError(FString::Printf(TEXT("Could not create Layout Capture output directory '%s'."), *OutputDirectory));
                Succeeded = false;
                continue;
            }

            const auto Table = SNew(SCkTextureDebugger_TextureHealthTable)
                .UseYogaLayout(bUseYogaLayout);
            Table->Set_Snapshot(Snapshot);
            TestEqual(FString::Printf(TEXT("%s capture dataset has %d table rows"), LayoutName, RowCount), Table->Get_TotalRowCount(), RowCount);
            TestEqual(FString::Printf(TEXT("%s capture dataset visibly publishes %d rows before virtualization"), LayoutName, RowCount), Table->Get_VisibleRowCount(), RowCount);
            if (RowCount > 0)
            {
                const auto Selected = Select_FirstRealRow(Table, bUseYogaLayout);
                TestTrue(FString::Printf(TEXT("%s selects a real first row before capture"), LayoutName), Selected);
                Succeeded &= Selected;
            }

            for (const auto Viewport : Viewports)
            {
                for (const auto Scale : DrawScales)
                {
                    const auto ScaleToken = FString::Printf(TEXT("%.1f"), Scale).Replace(TEXT("."), TEXT("p"));
                    const auto Filename = FString::Printf(TEXT("Rows_%04d_View_%dx%d_Scale_%s.png"), RowCount, Viewport.X, Viewport.Y, *ScaleToken);
                    const auto OutputPath = FPaths::Combine(OutputDirectory, Filename);
                    const auto Result = Capture(Table, FCaptureCase{Viewport, Scale}, OutputPath, bUseYogaLayout);

                    TestTrue(FString::Printf(TEXT("Rendered %s Texture Health Table: %s"), LayoutName, *Filename), Result.Succeeded);
                    if (NOT Result.Succeeded)
                    {
                        AddError(FString::Printf(TEXT("%s: %s"), *Filename, *Result.Failure));
                        Succeeded = false;
                        continue;
                    }

                    const auto PhysicalWidth = FMath::RoundToInt(static_cast<float>(Viewport.X) * Scale);
                    const auto PhysicalHeight = FMath::RoundToInt(static_cast<float>(Viewport.Y) * Scale);
                    const auto BatchAverages = FString::JoinBy(Result.BatchAverageDrawMilliseconds, TEXT(","), [](double Value)
                    { return FString::Printf(TEXT("%.3f"), Value); });
                    AddInfo(FString::Printf(
                        TEXT("TextureLayoutCapture phase=%s layout=%s rows=%d logical=%dx%d physical=%dx%d scale=%.1f desired=%.1fx%.1f widgetCount=%d viewportRows=%d drawAvgMs=%.3f drawMaxMs=%.3f batchDrawAvgMs=[%s] output=%s"),
                        *Phase, LayoutName, RowCount, Viewport.X, Viewport.Y, PhysicalWidth, PhysicalHeight, Scale,
                        Result.DesiredSize.X, Result.DesiredSize.Y, Result.WidgetCount, Result.InstantiatedRowCount,
                        Result.AverageDrawMilliseconds, Result.MaximumDrawMilliseconds, *BatchAverages, *OutputPath));

                    if (RowCount == 0)
                    { TestEqual(FString::Printf(TEXT("Empty table has no live viewport rows: %s"), *Filename), Result.InstantiatedRowCount, 0); }
                    else
                    { TestTrue(FString::Printf(TEXT("Non-empty table has live viewport rows: %s"), *Filename), Result.InstantiatedRowCount > 0); }
                    if (RowCount > 1)
                    {
                        TestTrue(FString::Printf(TEXT("Large table remains viewport-bounded: %s"), *Filename),
                            Result.InstantiatedRowCount < RowCount);
                    }
                }
            }
        }
    }

    return Succeeded;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_LayoutCapture_SceneAudit,
    "Ck.TextureDebugger.LayoutCapture.SceneAudit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_LayoutCapture_SceneAudit::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_layout_capture_tests;
    TArray<TStrongObjectPtr<UStaticMeshComponent>> Components;
    TArray<TStrongObjectPtr<UTexture2D>> Textures;
    auto Snapshot = Make_Snapshot(1000, Components, Textures);
    for (int32 Index = 0; Index < Snapshot.Components.Num(); ++Index)
    {
        auto& Row = Snapshot.Components[Index];
        Row.SupportsCheckerOverride = true;
        Row.HasComponentSlotOverlay = Index % 3 == 0;
        Row.InstanceCount = Index % 3 == 1 ? 24 : 0;
    }
    const auto Table = SNew(SCkTextureDebugger_SceneAuditTable);
    Table->SetSnapshot(Snapshot);
    if (!TestTrue(TEXT("Scene Audit capture loads installed markup"), Table->Get_LayoutRevision() > 0))
    {
        AddError(Table->Get_LayoutError().ToString());
        return false;
    }
    Table->SetSelectedComponent(Components[0].Get());
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/TextureDebugger/SceneAudit"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true)) { AddError(TEXT("Cannot create Scene Audit capture directory")); return false; }
    for (const FIntPoint Viewport : {FIntPoint{960, 640}, FIntPoint{640, 480}})
    {
        for (const float Scale : {1.0f, 1.5f})
        {
            const FString ScaleToken = FString::SanitizeFloat(Scale).Replace(TEXT("."), TEXT("p"));
            const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("Rows1000_%dx%d_%s.png"), Viewport.X, Viewport.Y, *ScaleToken));
            const auto Result = Capture(Table, FCaptureCase{Viewport, Scale}, Path, true);
            TestTrue(TEXT("Scene Audit native capture succeeds"), Result.Succeeded);
            if (!Result.Succeeded) { AddError(Result.Failure); return false; }
            TestTrue(TEXT("Scene Audit realizes a bounded visible subset"), Result.InstantiatedRowCount > 0 && Result.InstantiatedRowCount < 1000);
            AddInfo(TEXT("Scene Audit capture: ") + Path);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_LayoutCapture_UvDensity,
    "Ck.TextureDebugger.LayoutCapture.UvDensity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_LayoutCapture_UvDensity::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_layout_capture_tests;
    const auto Page = SNew(SCkTextureDebugger_UvDensityPage);
    if (!TestTrue(TEXT("UV capture loads installed markup"), Page->Get_LayoutRevision() > 0))
    {
        AddError(Page->Get_LayoutError().ToString());
        return false;
    }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/TextureDebugger/UvDensity"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true)) { AddError(TEXT("Cannot create UV capture directory")); return false; }
    for (const FIntPoint Viewport : {FIntPoint{960, 640}, FIntPoint{640, 480}})
    {
        const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("NoContext_%dx%d.png"), Viewport.X, Viewport.Y));
        const auto Result = Capture(Page, FCaptureCase{Viewport, 1.0f}, Path, true);
        if (!TestTrue(TEXT("UV page capture succeeds"), Result.Succeeded)) { AddError(Result.Failure); return false; }
        AddInfo(TEXT("UV page capture: ") + Path);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_LayoutCapture_MaterialInputs,
    "Ck.TextureDebugger.LayoutCapture.MaterialInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_LayoutCapture_MaterialInputs::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_layout_capture_tests;
    const auto Material = TStrongObjectPtr<UMaterialInterface>{LoadObject<UMaterialInterface>(nullptr,
        TEXT("/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker.M_CkTextureChecker"))};
    if (!TestNotNull(TEXT("Material capture loads the installed checker material"), Material.Get())) { return false; }
    const auto Component = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto Context = FCkTextureDebugger_ComponentRow{};
    Context.NavigationTarget = Component.Get();
    Context.ActorDisplayName = TEXT("Material capture actor");
    Context.ComponentDisplayName = TEXT("Checker material slots");
    auto Slots = TArray<int32>{};
    for (int32 Index = 0; Index < 100; ++Index)
    {
        auto Slot = FCkTextureDebugger_MaterialSlotRow{};
        Slot.SlotIndex = Index;
        Slot.NavigationTarget = Material.Get();
        Slot.MaterialPath = FSoftObjectPath{Material.Get()};
        Slot.DisplayName = Material->GetName();
        Context.MaterialSlots.Add(MoveTemp(Slot));
        Slots.Add(Index);
    }
    const auto Page = SNew(SCkTextureDebugger_MaterialInputsPage);
    if (!TestTrue(TEXT("Material capture loads installed markup"), Page->Get_LayoutRevision() > 0))
    {
        AddError(Page->Get_LayoutError().ToString());
        return false;
    }
    // Supply a value snapshot, then exercise the page's real per-material analysis and authored projection.
    Page->Set_Context(Context, {}, Slots);
    const auto Table = Page->Get_AuthoredTable();
    if (Table.IsValid()) { Table->TryRefresh(); }
    if (!TestTrue(TEXT("Material capture publishes real analysis rows"),
        Table.IsValid() && Table->GetList().IsValid() && Table->GetList()->GetItems().Num() >= 100)) { return false; }
    const int32 TotalRows = Table->GetList()->GetItems().Num();
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/TextureDebugger/MaterialInputs"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true)) { AddError(TEXT("Cannot create material capture directory")); return false; }
    for (const FIntPoint Viewport : {FIntPoint{960, 640}, FIntPoint{640, 480}})
    {
        for (const float Scale : {1.0f, 1.5f})
        {
            const FString Path = FPaths::Combine(Directory,
                FString::Printf(TEXT("Populated_%dx%d_%s.png"), Viewport.X, Viewport.Y, Scale > 1.0f ? TEXT("1p5") : TEXT("1p0")));
            const auto Result = Capture(Page, FCaptureCase{Viewport, Scale}, Path, true);
            if (!TestTrue(TEXT("Material page capture succeeds"), Result.Succeeded)) { AddError(Result.Failure); return false; }
            TestTrue(TEXT("Material table realizes a bounded subset"),
                Result.InstantiatedRowCount > 0 && Result.InstantiatedRowCount < TotalRows);
            AddInfo(TEXT("Material page capture: ") + Path);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_LayoutCapture_SurfaceLighting,
    "Ck.TextureDebugger.LayoutCapture.SurfaceLighting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_LayoutCapture_SurfaceLighting::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_layout_capture_tests;
    const auto Material = TStrongObjectPtr<UMaterialInterface>{LoadObject<UMaterialInterface>(nullptr,
        TEXT("/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker.M_CkTextureChecker"))};
    if (!TestNotNull(TEXT("Surface capture loads checker material"), Material.Get())) { return false; }
    const auto Component = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto Context = FCkTextureDebugger_ComponentRow{};
    Context.NavigationTarget = Component.Get();
    Context.ActorDisplayName = TEXT("Surface capture actor");
    Context.ComponentDisplayName = TEXT("Independent material slot cards");
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto Slot = FCkTextureDebugger_MaterialSlotRow{};
        Slot.SlotIndex = Index;
        Slot.NavigationTarget = Index == 2 ? nullptr : Material.Get();
        Slot.MaterialPath = Index == 2 ? FSoftObjectPath{} : FSoftObjectPath{Material.Get()};
        Slot.DisplayName = Index == 2 ? TEXT("Unresolved slot") : Material->GetName();
        Context.MaterialSlots.Add(MoveTemp(Slot));
    }
    const auto Page = SNew(SCkTextureDebugger_SurfaceLightingPage);
    if (!TestTrue(TEXT("Surface capture loads installed authored layout"), Page->Get_LayoutRevision() > 0))
    { AddError(Page->Get_LayoutError().ToString()); return false; }
    Page->Set_Context(Context, {}, {0, 1, 2});
    const auto Repeat = Page->Get_AuthoredRepeat();
    if (Repeat.IsValid()) { Repeat->TryRefresh(); }
    if (!TestTrue(TEXT("Surface capture projects all material cards"), Repeat.IsValid() && Repeat->GetItemCount() == 3)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/TextureDebugger/SurfaceLighting"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true)) { AddError(TEXT("Cannot create surface capture directory")); return false; }
    for (const FIntPoint Viewport : {FIntPoint{960, 640}, FIntPoint{640, 480}})
    {
        for (const float Scale : {1.0f, 1.5f})
        {
            const FString Path = FPaths::Combine(Directory,
                FString::Printf(TEXT("Populated_%dx%d_%s.png"), Viewport.X, Viewport.Y, Scale > 1.0f ? TEXT("1p5") : TEXT("1p0")));
            const auto Result = Capture(Page, FCaptureCase{Viewport, Scale}, Path, true);
            if (!TestTrue(TEXT("Surface page capture succeeds"), Result.Succeeded)) { AddError(Result.Failure); return false; }
            AddInfo(TEXT("Surface page capture: ") + Path);
        }
    }
    return true;
}

#endif
