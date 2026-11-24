#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Command/CommandActionButton.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Shared/PlanConfiguration.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWindow.h"

namespace
{
constexpr TCHAR DraftPrefix[] = TEXT("Mock MCP UI QA ");

bool IsLoopbackEndpoint(FString Url, const TCHAR* Prefix, const TCHAR* Suffix = TEXT(""))
{
    if (!Url.RemoveFromStart(Prefix) || (*Suffix && !Url.RemoveFromEnd(Suffix)))
    {
        return false;
    }
    const int32 Port = FCString::Atoi(*Url);
    return Port >= 1024 && Port <= 65535 && Url == FString::FromInt(Port);
}

struct FButtonHit
{
    FWidgetPath Path;
    FVector2D Center = FVector2D::ZeroVector;
};

FButtonHit FindVisibleButton(UWorld* World, const TCHAR* LabelKey)
{
    auto& Slate = FSlateApplication::Get();
    const FText Caption = PlanUI::T(LabelKey);
    const auto Window = World->GetGameViewport()->GetWindow();
    if (!Window)
    {
        return {};
    }

    FButtonHit Hit;
    for (TObjectIterator<UCommandActionButton> It; It; ++It)
    {
        const UCommandActionButton* Button = *It;
        const auto* Text = Cast<UTextBlock>(Button->GetContent());
        const auto Widget = Button->GetCachedWidget();
        const auto& Geometry = Button->GetCachedGeometry();
        if (Button->GetWorld() != World || !Text || !Text->GetText().EqualTo(Caption) ||
            !Button->GetIsEnabled() || !Widget || Geometry.GetLocalSize().GetMin() < 1)
        {
            continue;
        }

        const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
        auto Path = Slate.LocateWindowUnderMouse(Point, {Window.ToSharedRef()});
        for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        {
            if (Path.Widgets[Index].Widget != Widget)
            {
                continue;
            }
            if (Hit.Path.IsValid())
            {
                return {};
            }
            Hit.Center = Point;
            Hit.Path = MoveTemp(Path);
            break;
        }
    }
    return Hit;
}

bool ClickVisibleButton(UWorld* World, const TCHAR* LabelKey)
{
    const auto Hit = FindVisibleButton(World, LabelKey);
    if (!Hit.Path.IsValid())
    {
        return false;
    }

    // Enter Slate at pointer routing, using the rendered hit-test path. This
    // exercises SButton, UButton and the normal Command delegates and Backend.
    auto& Slate = FSlateApplication::Get();
    const FPointerEvent Move(0, Hit.Center, Slate.GetCursorPos(), {}, EKeys::Invalid, 0, FModifierKeysState());
    Slate.RoutePointerMoveEvent(Hit.Path, Move, false);
    const FPointerEvent Press(0, Hit.Center, Hit.Center, {EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState());
    const FPointerEvent Release(0, Hit.Center, Hit.Center, {}, EKeys::LeftMouseButton, 0, FModifierKeysState());
    const bool PressHandled = Slate.RoutePointerDownEvent(Hit.Path, Press).IsEventHandled();
    const bool ReleaseHandled = Slate.RoutePointerUpEvent(Hit.Path, Release).IsEventHandled();
    return PressHandled && ReleaseHandled;
}

bool HasConfigurationCaption(UWorld* World, const FText& Expected)
{
    for (TObjectIterator<UTextBlock> It; It; ++It)
    {
        if (It->GetWorld() == World && It->IsVisible() && It->GetText().EqualTo(Expected) &&
            It->GetCachedGeometry().GetLocalSize().GetMin() >= 1)
        {
            return true;
        }
    }
    return false;
}

class FMockMcpDraftUiSteps final : public IAutomationLatentCommand
{
public:
    FMockMcpDraftUiSteps(FAutomationTestBase& InTest, FString InPlanId, FString InDraftName, FString InOutput)
        : Test(InTest), PlanId(MoveTemp(InPlanId)), DraftName(MoveTemp(InDraftName)), Output(MoveTemp(InOutput))
    {
    }

    bool Update() override
    {
        if (FPlatformTime::Seconds() >= Deadline)
        {
            Test.AddError(FString::Printf(TEXT("Timed out during Mock draft UI step %d"), Step));
            return true;
        }

        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE)
            {
                World = Context.World();
                break;
            }
        }
        if (!World || !World->GetGameViewport() || !World->GetGameInstance())
        {
            return false;
        }
        auto* Sync = World->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        if (!Sync || !Sync->IsHydrated())
        {
            return false;
        }

        if (!Workspace.IsValid())
        {
            for (TObjectIterator<USecurityPlanWorkspaceWidget> It; It; ++It)
            {
                if (It->GetWorld() == World)
                {
                    Workspace = *It;
                    break;
                }
            }
        }
        if (!Workspace.IsValid())
        {
            return false;
        }

        const auto State = Sync->GetPlans();
        const auto Plans = PlanUI::Object(State, TEXT("plans"));
        const auto Draft = PlanUI::Find(State, TEXT("plans"), PlanId);
        const auto Executions = PlanUI::Object(State, TEXT("executions"));
        const TArray<TSharedPtr<FJsonValue>>* Missions = nullptr;
        if (!Test.TestTrue(TEXT("Only the uniquely named isolated MCP draft exists, with no missions or executions"),
            Plans && Plans->Values.Num() == 1 && PlanUI::Field(Draft, TEXT("name")) == DraftName &&
            PlanUI::Field(Draft, TEXT("status")) == TEXT("DRAFT") &&
            Draft->TryGetArrayField(TEXT("mission_ids"), Missions) && Missions && Missions->IsEmpty() &&
            Executions && Executions->Values.IsEmpty()))
        {
            return true;
        }

        switch (Step)
        {
        case 0:
            if (!Test.TestFalse(TEXT("The fixture starts without a configuration lease"), PlanConfiguration::Active(State)))
            {
                return true;
            }
            if (ClickVisibleButton(World, TEXT("Nav.Plans")))
            {
                ++Step;
            }
            return false;
        case 1:
            if (ClickVisibleButton(World, TEXT("Workflow.OpenPlan")))
            {
                ++Step;
            }
            return false;
        case 2:
            if (PlanUI::Field(Sync->GetContext(), TEXT("active_security_plan_id")) != PlanId ||
                !PlanConfiguration::IsOwner(State, Sync->GetInstanceId()) || PlanConfiguration::PlanId(State) != PlanId ||
                !Workspace->IsConfiguring() || Workspace->IsMissionNavigationBlocked() ||
                !HasConfigurationCaption(World, Workspace->ConfigurationStatusText()) ||
                !FindVisibleButton(World, TEXT("Configuration.Finish")).Path.IsValid())
            {
                return false;
            }
            if (!Capture(World, TEXT("01-opened-and-leased.png")))
            {
                return true;
            }
            ++Step;
            return false;
        case 3:
            if (ClickVisibleButton(World, TEXT("Configuration.Finish")))
            {
                ++Step;
            }
            return false;
        default:
            if (!State->HasField(TEXT("configuration_session")) ||
                !State->GetField<EJson::Null>(TEXT("configuration_session")).IsValid() ||
                Workspace->IsConfiguring() || Workspace->IsMissionNavigationBlocked() ||
                FindVisibleButton(World, TEXT("Configuration.Finish")).Path.IsValid() ||
                !FindVisibleButton(World, TEXT("Workflow.OpenPlan")).Path.IsValid())
            {
                return false;
            }
            Capture(World, TEXT("02-finished-and-released.png"));
            return true;
        }
    }

private:
    bool Capture(UWorld* World, const TCHAR* Filename)
    {
        const auto View = World->GetGameViewport()->GetGameViewportWidget();
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!Test.TestTrue(TEXT("The Command viewport rendered"), View.IsValid() &&
            FSlateApplication::Get().TakeScreenshot(View.ToSharedRef(), Pixels, Size) && !Pixels.IsEmpty()))
        {
            return false;
        }

        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
        return Test.TestTrue(TEXT("The Command screenshot was saved"),
            !Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *(Output / Filename)));
    }

    FAutomationTestBase& Test;
    FString PlanId;
    FString DraftName;
    FString Output;
    TWeakObjectPtr<USecurityPlanWorkspaceWidget> Workspace;
    double Deadline = FPlatformTime::Seconds() + 45;
    int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMockMcpDraftUiTest, "DroneOps.MockMcp.DraftUi",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMockMcpDraftUiTest::RunTest(const FString& Parameters)
{
    const TCHAR* CommandLine = FCommandLine::Get();
    FString PlanId, DraftName, Evidence, Http, Ws, Role;
    const bool CanRun = FParse::Param(CommandLine, TEXT("P55OfflineVisualQA")) &&
        !FParse::Param(CommandLine, TEXT("NullRHI")) &&
        FParse::Value(CommandLine, TEXT("ClientRole="), Role) && Role == TEXT("Command") &&
        FParse::Value(CommandLine, TEXT("P1Http="), Http) && IsLoopbackEndpoint(Http, TEXT("http://127.0.0.1:")) &&
        FParse::Value(CommandLine, TEXT("P1Ws="), Ws) && IsLoopbackEndpoint(Ws, TEXT("ws://127.0.0.1:"), TEXT("/ws")) &&
        FParse::Value(CommandLine, TEXT("MockMcpDraftId="), PlanId) && !PlanId.IsEmpty() &&
        FParse::Value(CommandLine, TEXT("MockMcpDraftName="), DraftName) && DraftName.StartsWith(DraftPrefix) &&
        FParse::Value(CommandLine, TEXT("MockMcpEvidence="), Evidence) && FPaths::DirectoryExists(Evidence);
    if (!TestTrue(TEXT("Run with the isolated Mock MCP UI verifier and an active renderer"), CanRun))
    {
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FMockMcpDraftUiSteps(*this, PlanId, DraftName, Evidence));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
