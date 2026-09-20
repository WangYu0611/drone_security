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
