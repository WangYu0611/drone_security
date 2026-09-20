#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Command/CommandScreenManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommandStartupRoleTest, "DroneOps.Command.StartupRole",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommandStartupRoleTest::RunTest(const FString& Parameters)
{
    auto Resolve = &UCommandScreenManager::ResolveClientRoleFromSettings;
    TestEqual(TEXT("plain Editor Play uses project Command default"), Resolve(TEXT(""), TEXT("Command")), EDroneClientRole::Command);
    TestEqual(TEXT("Video CLI overrides Command project"), Resolve(TEXT("-ClientRole=Video"), TEXT("Command")), EDroneClientRole::Video);
    TestEqual(TEXT("Standalone CLI preserves legacy role"), Resolve(TEXT("-ClientRole=Standalone"), TEXT("Command")), EDroneClientRole::Standalone);
    TestEqual(TEXT("Command CLI overrides another project default"), Resolve(TEXT("-ClientRole=Command"), TEXT("Video")), EDroneClientRole::Command);
    TestEqual(TEXT("another project may default to Video"), Resolve(TEXT("-windowed"), TEXT("Video")), EDroneClientRole::Video);
    TestEqual(TEXT("role names are case insensitive"), Resolve(TEXT("-ClientRole=command"), TEXT("Standalone")), EDroneClientRole::Command);
    TestEqual(TEXT("unknown explicit role never falls back to Command"), Resolve(TEXT("-ClientRole=Unknown"), TEXT("Command")), EDroneClientRole::Standalone);
    TestEqual(TEXT("Map CLI overrides Command default"), Resolve(TEXT("-ClientRole=Map"), TEXT("Command")), EDroneClientRole::Map);
    TestEqual(TEXT("Map is case insensitive"), Resolve(TEXT("-ClientRole=mAp"), TEXT("Command")), EDroneClientRole::Map);
    return true;
}
#endif
