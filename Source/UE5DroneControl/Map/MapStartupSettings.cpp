#include "Map/MapStartupSettings.h"
#include "CesiumGeoreference.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"

bool FMapStartupSettings::IsValid() const
{
    return !LongitudeLatitudeHeight.ContainsNaN()
        && FMath::Abs(LongitudeLatitudeHeight.X)<=180. && FMath::Abs(LongitudeLatitudeHeight.Y)<=85.05112878
        && LongitudeLatitudeHeight.Z>=-1000. && LongitudeLatitudeHeight.Z<=100000.
        && FMath::IsFinite(ViewDistanceMeters) && ViewDistanceMeters>=5. && ViewDistanceMeters<=20000.
        && FMath::IsFinite(PitchDegrees) && PitchDegrees<=-1. && PitchDegrees>=-89.
        && FMath::IsFinite(YawDegrees);
}

FMapStartupSettings FMapStartupSettings::Read()
{
    FMapStartupSettings Value;
    GConfig->GetBool(TEXT("MapStartup"),TEXT("Enabled"),Value.bEnabled,GGameIni);
    const bool HasLongitude=GConfig->GetDouble(TEXT("MapStartup"),TEXT("Longitude"),Value.LongitudeLatitudeHeight.X,GGameIni);
    const bool HasLatitude=GConfig->GetDouble(TEXT("MapStartup"),TEXT("Latitude"),Value.LongitudeLatitudeHeight.Y,GGameIni);
    const bool HasHeight=GConfig->GetDouble(TEXT("MapStartup"),TEXT("HeightMeters"),Value.LongitudeLatitudeHeight.Z,GGameIni);
    GConfig->GetDouble(TEXT("MapStartup"),TEXT("ViewDistanceMeters"),Value.ViewDistanceMeters,GGameIni);
    GConfig->GetDouble(TEXT("MapStartup"),TEXT("PitchDegrees"),Value.PitchDegrees,GGameIni);
    GConfig->GetDouble(TEXT("MapStartup"),TEXT("YawDegrees"),Value.YawDegrees,GGameIni);
    if(Value.bEnabled && (!HasLongitude || !HasLatitude || !HasHeight || !Value.IsValid()))
    {
        UE_LOG(LogTemp,Warning,TEXT("[MapStartup] Invalid or incomplete city view configuration; retaining existing map startup."));
        Value.bEnabled=false;
    }
    return Value;
}

void FMapStartupSettings::ResolveMenuOverride(UWorld* World)
{
    if(!bEnabled || !World || !World->GetGameInstance())return;
    const auto* Network=World->GetGameInstance()->GetSubsystem<UDroneNetworkManager>();
    if(!Network || !Network->HasPendingGeoreferenceOrigin())return;
    LongitudeLatitudeHeight=FVector(Network->PendingOriginLongitude,Network->PendingOriginLatitude,Network->PendingOriginAltitude);
    bMenuOverride=true;
    if(!IsValid())
    {
        bEnabled=false;
        UE_LOG(LogTemp,Warning,TEXT("[MapStartup] Invalid explicit menu origin; city default will not replace it."));
    }
}

bool FMapStartupSettings::ApplyOrigin(UWorld* World) const
{
    if(!bEnabled || !IsValid() || !World)return false;
    for(TActorIterator<ACesiumGeoreference> It(World);It;++It)
    {
        if(It->GetOriginPlacement()!=EOriginPlacement::CartographicOrigin)
            It->SetOriginPlacement(EOriginPlacement::CartographicOrigin);
        It->SetOriginLongitudeLatitudeHeight(LongitudeLatitudeHeight);
        UE_LOG(LogTemp,Log,TEXT("[MapStartup] Applied %s origin longitude=%.6f latitude=%.6f height=%.1fm before map presentation."),
            bMenuOverride?TEXT("menu"):TEXT("configured"),LongitudeLatitudeHeight.X,LongitudeLatitudeHeight.Y,LongitudeLatitudeHeight.Z);
        return true;
    }
    UE_LOG(LogTemp,Warning,TEXT("[MapStartup] No CesiumGeoreference exists; configured city origin could not be applied."));
    return false;
}
