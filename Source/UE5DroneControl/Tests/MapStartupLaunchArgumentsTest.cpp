#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Map/MapStartupSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapStartupLaunchArgumentsTest,"DroneOps.Map.StartupLaunchArguments",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapStartupLaunchArgumentsTest::RunTest(const FString& Parameters)
{
    const auto Defaults=[]()
    {
        FMapStartupSettings Value;
        Value.bEnabled=true;
        Value.LongitudeLatitudeHeight=FVector(103.862188,1.279445,15.);
        Value.ViewDistanceMeters=2400.;
        return Value;
    };
    for(const TCHAR* Arguments:{TEXT(""),TEXT("-ClientRole=Map -unrelated=1"),TEXT("-NotMapStartupMode=Realtime")})
    {
        auto Value=Defaults();
        Value.ResolveLaunchArguments(Arguments);
        TestTrue(TEXT("No explicit launch origin retains enabled defaults"),Value.bEnabled);
        TestFalse(TEXT("Unrelated arguments do not claim an override"),Value.bLaunchOverride);
        TestFalse(TEXT("Without a real-time launch the configured map mode is retained"),Value.StartsIn2D());
        TestTrue(TEXT("No explicit launch origin retains the configured city"),Value.LongitudeLatitudeHeight.Equals(Defaults().LongitudeLatitudeHeight,1.e-9));
        TestEqual(TEXT("No explicit origin retains configured framing"),Value.ViewDistanceMeters,2400.);
    }
    auto Demo=Defaults();
    Demo.ResolveLaunchArguments(TEXT("-MapStartupMode=Demo -MapStartupLongitude=103.862188 -MapStartupLatitude=1.279445 -MapStartupHeight=15"));
    TestTrue(TEXT("Complete Demo origin enables launch presentation"),Demo.bEnabled && Demo.bLaunchOverride && Demo.IsValid());
    TestFalse(TEXT("Demo retains the configured 3D startup rather than forcing the global 2D basemap"),Demo.StartsIn2D());
    TestTrue(TEXT("Demo tuple matches the Singapore lawn"),Demo.LongitudeLatitudeHeight.Equals(Defaults().LongitudeLatitudeHeight,1.e-9));
    TestEqual(TEXT("Launch origin retains camera framing defaults"),Demo.ViewDistanceMeters,2400.);

    auto Realtime=Defaults();
    Realtime.bEnabled=false;
    Realtime.ResolveLaunchArguments(TEXT("-MapStartupMode=Realtime -MapStartupLongitude=116.34703 -MapStartupLatitude=39.9807 -MapStartupHeight=43"));
    TestTrue(TEXT("Complete Realtime tuple can explicitly enable presentation"),Realtime.bEnabled && Realtime.bLaunchOverride && Realtime.IsValid());
    TestTrue(TEXT("Realtime starts on the global basemap without relying on Google 3D coverage"),Realtime.StartsIn2D());
    TestTrue(TEXT("Realtime uses supplied geographic position rather than Singapore"),Realtime.LongitudeLatitudeHeight.Equals(FVector(116.34703,39.9807,43.),1.e-9));
    TestFalse(TEXT("Launch tuple does not masquerade as a main-menu override"),Realtime.bMenuOverride);

    auto Zero=Defaults();
    Zero.ResolveLaunchArguments(TEXT("-MapStartupMode=realtime -MapStartupLongitude=0 -MapStartupLatitude=0 -MapStartupHeight=-10"));
    TestTrue(TEXT("Equator, prime meridian and negative ellipsoid heights remain valid"),Zero.bEnabled && Zero.LongitudeLatitudeHeight.Equals(FVector(0.,0.,-10.),1.e-9));
    TestTrue(TEXT("Real-time mode matching is case-insensitive for the initial 2D policy"),Zero.StartsIn2D());
    auto Quoted=Defaults();
    Quoted.ResolveLaunchArguments(TEXT("-MapStartupMode=\"Demo\" -MapStartupLongitude=\"1.03862188e2\" -MapStartupLatitude=+1.279445 -MapStartupHeight=15.0"));
    TestTrue(TEXT("Quoted numeric arguments and complete scientific notation remain valid"),Quoted.bEnabled && Quoted.LongitudeLatitudeHeight.Equals(Defaults().LongitudeLatitudeHeight,1.e-9));

    const TCHAR* InvalidArguments[]={
        TEXT("-MapStartupMode=Demo"),
        TEXT("-MapStartupMode=Realtime"),
        TEXT("-MapStartupMode"),
        TEXT("-MapStartupMode="),
        TEXT("-MapStartupMode=Unknown -MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=15"),
        TEXT("-MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Demo -MapStartupLatitude=1 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupLatitude=1"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude= -MapStartupLatitude=1 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude=east -MapStartupLatitude=1 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupLatitude=NaN -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=inf"),
        TEXT("-MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=1e9999"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude=103degrees -MapStartupLatitude=1 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude=103 -MapStartupLatitude=. -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude=103 -MapStartupLatitude=1e -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=15f"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude=103 -MapStartupLatitude=\"1 2\" -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude=181 -MapStartupLatitude=1 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Demo -MapStartupLongitude=103 -MapStartupLatitude=86 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=-1001"),
        TEXT("-MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=100001"),
        TEXT("-MapStartupMode=Demo -MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=15"),
        TEXT("-MapStartupMode=Realtime -MapStartupLongitude=103 -MapStartupLatitude=1 -MapStartupHeight=15 -MapStartupHeight=16")
    };
    AddExpectedMessagePlain(TEXT("[MapStartup]"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,UE_ARRAY_COUNT(InvalidArguments));
    for(const TCHAR* Arguments:InvalidArguments)
    {
        auto Value=Defaults();
        Value.ResolveLaunchArguments(Arguments);
        TestFalse(*FString::Printf(TEXT("Invalid explicit origin is disabled: %s"),Arguments),Value.bEnabled);
        TestFalse(TEXT("An invalid tuple cannot select the real-time initial basemap policy"),Value.StartsIn2D());
        TestTrue(TEXT("Invalid explicit origin is still identified as a launch override"),Value.bLaunchOverride);
        TestTrue(TEXT("Invalid tuple never retains the Singapore coordinate fallback"),Value.LongitudeLatitudeHeight.IsZero());
    }
    return true;
}
#endif
