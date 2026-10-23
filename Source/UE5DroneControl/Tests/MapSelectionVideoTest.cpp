#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Shared/MapSelectionVideo.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapSelectionVideoAckTest,"DroneOps.Map.SelectionVideoAck",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapSelectionVideoAckTest::RunTest(const FString&) {
    auto Ack=MakeShared<FJsonObject>();Ack->SetNumberField(TEXT("context_version"),9);
    Ack->SetStringField(TEXT("active_uav_id"),TEXT("UAV-01"));
    Ack->SetArrayField(TEXT("selected_uav_ids"),{MakeShared<FJsonValueString>(TEXT("UAV-01")),MakeShared<FJsonValueString>(TEXT("UAV-02"))});
    TestTrue(TEXT("Matching primary, full group and current version authorize the second phase"),MapSelectionVideo::SelectionAckMatches(Ack,9,1,{1,2}));
    TestFalse(TEXT("Same-primary group ACK cannot stand in for a singleton ACK"),MapSelectionVideo::SelectionAckMatches(Ack,9,1,{1}));
    TestFalse(TEXT("Older context ACK cannot retarget Video after newer context"),MapSelectionVideo::SelectionAckMatches(Ack,10,1,{1,2}));
    TestFalse(TEXT("Different primary ACK cannot retarget Video"),MapSelectionVideo::SelectionAckMatches(Ack,9,2,{1,2}));
    Ack->SetArrayField(TEXT("selected_uav_ids"),{MakeShared<FJsonValueString>(TEXT("UAV-01"))});
    TestTrue(TEXT("An exact same-version no-op selection ACK remains valid"),MapSelectionVideo::SelectionAckMatches(Ack,9,1,{1}));
    Ack->RemoveField(TEXT("selected_uav_ids"));
    TestFalse(TEXT("Missing members never authorize a video write"),MapSelectionVideo::SelectionAckMatches(Ack,9,1,{1}));
    Ack->SetArrayField(TEXT("selected_uav_ids"),{MakeShared<FJsonValueNumber>(1)});
    TestFalse(TEXT("Malformed member type never authorizes a video write"),MapSelectionVideo::SelectionAckMatches(Ack,9,1,{1}));
    Ack->SetField(TEXT("active_uav_id"),MakeShared<FJsonValueNull>());Ack->SetArrayField(TEXT("selected_uav_ids"),{});
    TestTrue(TEXT("Ctrl-deselect of the last member can ACK empty selection before following the actual hit"),MapSelectionVideo::SelectionAckMatches(Ack,9,0,{}));
    TestFalse(TEXT("Missing ACK never authorizes Video"),MapSelectionVideo::SelectionAckMatches(nullptr,9,0,{}));
    return true;
}
#endif
