#include "Map/MapStartupSettings.h"
#include "CesiumGeoreference.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/Parse.h"

namespace
{
bool ParseLaunchCoordinate(const FString& Text,double& OutValue)
{
    int32 Cursor=0;
    if(Cursor<Text.Len() && (Text[Cursor]==TEXT('+') || Text[Cursor]==TEXT('-')))++Cursor;
    auto IsDigit=[](TCHAR Character){return Character>=TEXT('0') && Character<=TEXT('9');};
    bool bHasDigit=false;
    while(Cursor<Text.Len() && IsDigit(Text[Cursor])){bHasDigit=true;++Cursor;}
    if(Cursor<Text.Len() && Text[Cursor]==TEXT('.'))
    {
        ++Cursor;
        while(Cursor<Text.Len() && IsDigit(Text[Cursor])){bHasDigit=true;++Cursor;}
    }
    if(!bHasDigit)return false;
    if(Cursor<Text.Len() && (Text[Cursor]==TEXT('e') || Text[Cursor]==TEXT('E')))
    {
        ++Cursor;
        if(Cursor<Text.Len() && (Text[Cursor]==TEXT('+') || Text[Cursor]==TEXT('-')))++Cursor;
        const int32 ExponentStart=Cursor;
        while(Cursor<Text.Len() && IsDigit(Text[Cursor]))++Cursor;
        if(Cursor==ExponentStart)return false;
    }
    return Cursor==Text.Len() && FDefaultValueHelper::ParseDouble(Text,OutValue) && FMath::IsFinite(OutValue);
}
}

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
    Value.ResolveLaunchArguments(FCommandLine::Get());
    return Value;
}

void FMapStartupSettings::ResolveLaunchArguments(const TCHAR* CommandLine)
{
    const TCHAR* Names[]={TEXT("-MapStartupMode"),TEXT("-MapStartupLongitude"),TEXT("-MapStartupLatitude"),TEXT("-MapStartupHeight")};
    FString Values[4];
    int32 Counts[4]={0,0,0,0};
    bool bExplicit=false;
    const TCHAR* Cursor=CommandLine?CommandLine:TEXT("");
    FString Token;
    while(FParse::Token(Cursor,Token,false))
    {
        for(int32 Index=0;Index<4;++Index)
        {
            const FString Name(Names[Index]);
            if(!Token.Equals(Name,ESearchCase::IgnoreCase) && !Token.StartsWith(Name+TEXT("="),ESearchCase::IgnoreCase))continue;
            bExplicit=true;
            ++Counts[Index];
            Values[Index]=Token.Len()>Name.Len()?Token.Mid(Name.Len()+1):FString();
            if(Values[Index].Len()>=2 && Values[Index].StartsWith(TEXT("\"")) && Values[Index].EndsWith(TEXT("\"")))
                Values[Index]=Values[Index].Mid(1,Values[Index].Len()-2);
        }
    }
    if(!bExplicit)return;

    // A partial/invalid explicit origin must never inherit the Singapore default.
    bLaunchOverride=true;
    bMenuOverride=false;
    bRealtimeLaunch=false;
    bEnabled=false;
    LongitudeLatitudeHeight=FVector::ZeroVector;
    FVector Origin=FVector::ZeroVector;
    if(Counts[0]!=1 || Counts[1]!=1 || Counts[2]!=1 || Counts[3]!=1
        || (!Values[0].Equals(TEXT("Demo"),ESearchCase::IgnoreCase) && !Values[0].Equals(TEXT("Realtime"),ESearchCase::IgnoreCase))
        || !ParseLaunchCoordinate(Values[1],Origin.X) || !ParseLaunchCoordinate(Values[2],Origin.Y)
        || !ParseLaunchCoordinate(Values[3],Origin.Z))
    {
        UE_LOG(LogTemp,Warning,TEXT("[MapStartup] Invalid or incomplete explicit launch origin; configured city fallback is disabled."));
        return;
    }
    LongitudeLatitudeHeight=Origin;
    bEnabled=IsValid();
    bRealtimeLaunch=bEnabled && Values[0].Equals(TEXT("Realtime"),ESearchCase::IgnoreCase);
    if(!bEnabled)
    {
        LongitudeLatitudeHeight=FVector::ZeroVector;
        UE_LOG(LogTemp,Warning,TEXT("[MapStartup] Explicit launch origin is out of range; configured city fallback is disabled."));
    }
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
            bMenuOverride?TEXT("menu"):bLaunchOverride?TEXT("launch"):TEXT("configured"),LongitudeLatitudeHeight.X,LongitudeLatitudeHeight.Y,LongitudeLatitudeHeight.Z);
        return true;
    }
    UE_LOG(LogTemp,Warning,TEXT("[MapStartup] No CesiumGeoreference exists; configured city origin could not be applied."));
    return false;
}
