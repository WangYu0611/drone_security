#include <gtest/gtest.h>
#include "storage/security_plan_store.h"

using O=boost::json::object;
using A=boost::json::array;
namespace {
struct PlanFixture : testing::Test {
    SecurityPlanStore store{""};
    std::set<std::string> uavs{"UAV-01","UAV-02","UAV-03"};
    O call(O r) {
        const auto action=SecurityPlanStore::str(r,"action");r["instance_id"]="qa-map";
        if(!r.contains("expected_version"))r["expected_version"]=store.snapshot().at("version");
        if(action=="save_route") {
            const auto p=SecurityPlanStore::str(r,"plan_id"),m=SecurityPlanStore::str(r,"mission_id");
            auto reply=call({{"action","begin_route_edit"},{"plan_id",p},{"mission_id",m},
                {"content_revision",store.snapshot().at("plans").as_object().at(p).as_object().at("content_revision")}});
            r["edit_session_id"]=reply.at("edit_session_id");r["expected_version"]=store.snapshot().at("version");
        }
        return store.transact(r,uavs,"QA",[](const O&){});
    }
    std::string plan() { return SecurityPlanStore::str(call({{"action","create_plan"},{"name","QA plan"}}),"plan_id"); }
    std::string mission(const std::string& p) {return SecurityPlanStore::str(call({{"action","add_mission"},{"plan_id",p},{"name","Mission-01"}}),"mission_id");}
    O route() {return {{"pathId",1},{"bClosedLoop",false},{"waypoints",A{
        O{{"sequence",1},{"latitude",39.98},{"longitude",116.34},{"altitude",60},{"segmentSpeed",0},{"waitTime",0}},
        O{{"sequence",2},{"latitude",39.981},{"longitude",116.341},{"altitude",60},{"segmentSpeed",5},{"waitTime",0}}}}};}
    void ready(const std::string& p,const std::string& m) {
        call({{"action","assign"},{"plan_id",p},{"mission_id",m},{"assigned_uav_id","UAV-01"}});
        call({{"action","save_route"},{"plan_id",p},{"mission_id",m},{"path",route()}});
        call({{"action","validate"},{"plan_id",p}});
    }
};
TEST_F(PlanFixture, Create) {auto p=plan();EXPECT_EQ(store.snapshot().at("plans").as_object().at(p).as_object().at("status"),"DRAFT");}
TEST_F(PlanFixture, RehearsalCompletesWithRealAdapterConfiguredAndCopiesOnlyReviewedRoute) {
    int sends=0,holds=0;
    auto adapter=std::make_shared<security_mission::RealAdapter>(
        [](const std::string&)->security_mission::Position {throw std::runtime_error("REAL_TELEMETRY_UNAVAILABLE");},
        [](const std::string&){return security_mission::Position{39.98,116.34,60.};},
        [&](const security_mission::Command&){++sends;return true;},
        [&](const std::string&){++holds;return true;});
    store.configureAdapter({{"uavs",O{{"UAV-03",O{{"drone_id",3}}}}}},adapter);
    call({{"action","simulation_create"},{"request_id","create-sim"},{"count",1},
        {"latitude",39.98},{"longitude",116.34},{"altitude",60.}});
    const auto p=plan(),m=mission(p);
    EXPECT_THROW(call({{"action","assign"},{"plan_id",p},{"mission_id",m},
        {"assigned_uav_ids",A{"UAV-1001","UAV-03"}}}),PlanError);
    call({{"action","assign"},{"plan_id",p},{"mission_id",m},{"assigned_uav_id","UAV-1001"}});
    call({{"action","save_route"},{"plan_id",p},{"mission_id",m},{"path",route()}});
    const auto start=call({{"action","simulation_start"},{"request_id","rehearse"},{"plan_id",p},{"mission_id",m}});
    const auto id=SecurityPlanStore::str(start,"execution_id");
    for(int i=0;i<60;++i)store.tickExecutions(1.,uavs,[](const O&){});
    const auto state=store.snapshot();
    const auto& run=state.at("executions").as_object().at(id).as_object();
    EXPECT_EQ(run.at("state"),"COMPLETED");
    EXPECT_TRUE(run.at("simulation").as_bool());
    EXPECT_GT(run.at("replay_samples").as_array().size(),2u);
    EXPECT_EQ(sends,0);
    EXPECT_EQ(holds,0);
    EXPECT_THROW(call({{"action","simulation_assign_real"},{"request_id","unreviewed"},
        {"execution_id",id},{"assigned_uav_id","UAV-03"}}),PlanError);
    call({{"action","simulation_review"},{"request_id","review-sim"},{"execution_id",id}});
    const O handoff{{"action","simulation_assign_real"},{"request_id","promote"},{"execution_id",id},
        {"assigned_uav_id","UAV-03"},{"assigned_drone_id",3}};
    const auto pending=call(handoff);
    const auto realPlan=SecurityPlanStore::str(pending,"plan_id");
    EXPECT_NE(realPlan,p);
    const auto pendingState=pending.at("state").as_object();
    const auto& draft=pendingState.at("plans").as_object().at(realPlan).as_object();
    EXPECT_EQ(draft.at("status"),"DRAFT");
    EXPECT_EQ(SecurityPlanStore::str(draft.at("real_assignment").as_object(),"source_execution_id"),id);
    const auto& issues=draft.at("validation").as_object().at("issues").as_array();
    ASSERT_FALSE(issues.empty());
    EXPECT_EQ(issues.front().as_object().at("code"),"REAL_TELEMETRY_UNAVAILABLE");
    EXPECT_EQ(SecurityPlanStore::str(call(handoff),"plan_id"),realPlan);
    store.configureAdapter({{"uavs",O{{"UAV-03",O{{"drone_id",4}}}}}},adapter);
    const auto recheck=call({{"action","validate"},{"plan_id",realPlan}});
    EXPECT_EQ(recheck.at("state").as_object().at("plans").as_object().at(realPlan).as_object()
        .at("validation").as_object().at("issues").as_array().front().as_object().at("code"),"REAL_UAV_REGISTRATION_CHANGED");
    call({{"action","save_route"},{"plan_id",p},{"mission_id",m},{"path",route()}});
    auto stale=handoff;stale["request_id"]="stale";
    EXPECT_THROW(call(stale),PlanError);
    EXPECT_EQ(sends,0);
    EXPECT_EQ(holds,0);
}
TEST_F(PlanFixture, AssignmentAndDuplicateRejected) {
    auto p=plan(),m=mission(p),m2=mission(p);
    call({{"action","assign"},{"plan_id",p},{"mission_id",m},{"assigned_uav_id","UAV-01"}});
    const auto before=store.snapshot();
    try {call({{"action","assign"},{"plan_id",p},{"mission_id",m2},{"assigned_uav_id","UAV-01"}});FAIL();}
    catch(const PlanError& e){EXPECT_EQ(e.code,"UAV_ALREADY_ASSIGNED");}
    EXPECT_EQ(before,store.snapshot());
}
TEST_F(PlanFixture, ValidationFailureAndAtomicDeploy) {
    auto p=plan(),m=mission(p);const auto before=store.snapshot();
    EXPECT_THROW(call({{"action","deploy"},{"plan_id",p}}),PlanError);EXPECT_EQ(before,store.snapshot());
    ready(p,m);call({{"action","review"},{"plan_id",p}});call({{"action","deploy"},{"plan_id",p}});
    auto s=store.snapshot();EXPECT_EQ(s.at("plans").as_object().at(p).as_object().at("status"),"DEPLOYED");
    EXPECT_EQ(s.at("missions").as_object().at(m).as_object().at("status"),"DEPLOYED");
    EXPECT_THROW(call({{"action","delete_mission"},{"plan_id",p},{"mission_id",m}}),PlanError);
}
TEST_F(PlanFixture, InvalidCoordinatesAndStaleVersion) {
    auto p=plan(),m=mission(p);auto r=route();r.at("waypoints").as_array()[0].as_object()["latitude"]=91;
    const auto before=store.snapshot();
    EXPECT_THROW(call({{"action","save_route"},{"plan_id",p},{"mission_id",m},{"path",r}}),PlanError);
    EXPECT_THROW(call({{"action","add_mission"},{"plan_id",p},{"name","late"},{"expected_version",0}}),PlanError);
    EXPECT_EQ(before.at("plans"),store.snapshot().at("plans"));
    EXPECT_EQ(before.at("paths"),store.snapshot().at("paths"));
    EXPECT_EQ(store.snapshot().at("edit_sessions").as_object().size(),1u);
}
TEST_F(PlanFixture, SelectionAndReadinessInvalidation) {
    auto p=plan(),m=mission(p);ready(p,m);store.checkSelection(p,m);
    EXPECT_THROW(store.checkSelection(plan(),m),PlanError);
    call({{"action","assign"},{"plan_id",p},{"mission_id",m},{"assigned_uav_id",nullptr}});
    EXPECT_EQ(store.snapshot().at("plans").as_object().at(p).as_object().at("status"),"DRAFT");
}
TEST_F(PlanFixture, PersistenceRestart) {
    auto path=(std::filesystem::temp_directory_path()/("p3-qa-"+std::to_string(OperationalContext::now())+".json")).string();
    O saved;
    {SecurityPlanStore disk(path);disk.transact({{"action","create_plan"},{"name","Persisted QA"}},uavs,"QA",[](const O&){});saved=disk.snapshot();}
    {SecurityPlanStore disk(path);EXPECT_EQ(saved,disk.snapshot());}
    std::filesystem::remove(path);
}
}
