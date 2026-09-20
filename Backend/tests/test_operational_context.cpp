#include "storage/operational_context.h"
#include <gtest/gtest.h>
#include <thread>
TEST(P1BackendContext, SnapshotVersionAtomicUpdateAndNoop) {
    OperationalContext context(""); int events=0;
    EXPECT_EQ(context.snapshot().at("context_version").to_number<int64_t>(),0);
    auto publish=[&](const boost::json::object& e) { ++events; EXPECT_EQ(e.at("changes").as_array().size(),2); };
    auto result=context.update({{"active_uav_id","UAV-03"},{"active_alert_id","ALERT-3"}},"Command",publish);
    EXPECT_EQ(result.at("context_version").to_number<int64_t>(),1);
    EXPECT_EQ(result.at("active_uav_id"),"UAV-03");
    context.update({{"active_uav_id","UAV-03"}},"Map",publish); EXPECT_EQ(events,1);
    EXPECT_THROW(context.update({{"active_area_id","AREA-A"},{"operation_mode","INVALID"}},"Command",publish),std::invalid_argument);
    EXPECT_TRUE(context.snapshot().at("active_area_id").is_null());
    EXPECT_THROW(context.update({{"context_version",99}},"Command",publish),std::invalid_argument);
}
TEST(P1BackendContext, ConcurrentWritersPublishMonotonically) {
    OperationalContext context(""); std::vector<int64_t> versions;
    auto publish=[&](const boost::json::object& e) { versions.push_back(e.at("version").to_number<int64_t>()); };
    std::vector<std::thread> threads;
    for(int i=0;i<20;++i) threads.emplace_back([&,i]{context.update({{"active_mission_id","MISSION-"+std::to_string(i)}},"Command",publish);});
    for(auto& t:threads)t.join(); ASSERT_EQ(versions.size(),20);
    for(int i=0;i<20;++i) EXPECT_EQ(versions[i],i+1);
}
TEST(P1BackendContext, RestartPreservesVersionAndAllFields) {
    const auto path=(std::filesystem::temp_directory_path()/ ("p1-context-test-"+std::to_string(OperationalContext::now())+".json")).string();
    { OperationalContext context(path);
      context.update({{"active_security_plan_id","PLAN-A"},{"active_area_id","AREA-B"},{"operation_mode","PLAN_EDIT"}},"Command",[](const auto&){}); }
    { OperationalContext context(path); EXPECT_EQ(context.snapshot().at("context_version").to_number<int64_t>(),1);
      EXPECT_EQ(context.snapshot().at("active_area_id"),"AREA-B"); EXPECT_EQ(context.snapshot().at("operation_mode"),"PLAN_EDIT"); }
    std::filesystem::remove(path);
}
