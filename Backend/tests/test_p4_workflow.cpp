#include <gtest/gtest.h>
#include "storage/shared_view_store.h"
using O=boost::json::object;using A=boost::json::array;
namespace {
struct P4Workflow:testing::Test {
    SecurityPlanStore store{""};std::set<std::string> uavs{"UAV-01","UAV-02"};std::string p,m,sid;
    O send(O r) {if(!r.contains("expected_version"))r["expected_version"]=store.snapshot().at("version");
        if(!r.contains("instance_id"))r["instance_id"]="Map-1";
        if(!r.contains("plan_id"))r["plan_id"]=p;if(!r.contains("mission_id"))r["mission_id"]=m;
        return store.transact(r,uavs,"QA",[](const O&){});}
    O plan(){return store.snapshot().at("plans").as_object().at(p).as_object();}
    void SetUp() override {p=SecurityPlanStore::str(send({{"action","create_plan"},{"name","原名"}}),"plan_id");
        m=SecurityPlanStore::str(send({{"action","add_mission"},{"name","East Perimeter"}}),"mission_id");}
    void begin(bool workflow=false){sid=SecurityPlanStore::str(send({{"action","begin_route_edit"},{"content_revision",plan().at("content_revision")},{"workflow",workflow}}),"edit_session_id");}
    O route(){return {{"pathId",1},{"bClosedLoop",false},{"waypoints",A{
        O{{"sequence",1},{"latitude",39.},{"longitude",116.},{"altitude",60.},{"segmentSpeed",0},{"waitTime",0}},
        O{{"sequence",2},{"latitude",39.001},{"longitude",116.001},{"altitude",60.},{"segmentSpeed",5},{"waitTime",2}}}}};}
    void ready(){send({{"action","assign"},{"assigned_uav_id","UAV-01"}});begin();
        send({{"action","save_route"},{"edit_session_id",sid},{"path",route()}});send({{"action","validate"}});}
    void reviewed(){ready();send({{"action","review"}});}
    void error(O r,const char* code){const auto before=store.snapshot();try{send(r);FAIL()<<code;}catch(const PlanError& e){EXPECT_EQ(e.code,code);}EXPECT_EQ(before,store.snapshot());}
};
TEST_F(P4Workflow, RevisionValidationReviewInvalidation){
    EXPECT_EQ(plan().at("content_revision"),2);reviewed();const auto revision=plan().at("content_revision");
    send({{"action","validate"}});send({{"action","review"}});EXPECT_EQ(plan().at("content_revision"),revision);
    EXPECT_EQ(plan().at("review").as_object().at("content_revision"),revision);
    send({{"action","rename_mission"},{"name","Changed"}});EXPECT_EQ(plan().at("status"),"DRAFT");EXPECT_TRUE(plan().at("review").is_null());
    EXPECT_EQ(plan().at("content_revision").to_number<int64_t>(),revision.to_number<int64_t>()+1);error({{"action","deploy"}},"PLAN_NOT_READY");
}
TEST_F(P4Workflow, ReviewRequiredAndDeploymentCopyImmutable){
    ready();error({{"action","deploy"}},"PLAN_REVIEW_REQUIRED");send({{"action","review"}});const auto rev=plan().at("content_revision");
    send({{"action","deploy"}});EXPECT_EQ(plan().at("content_revision"),rev);const auto old=store.snapshot();
    EXPECT_EQ(old.at("deployments").as_object().size(),1u);error({{"action","deploy"}},"PLAN_DEPLOYED");
    error({{"action","rename_mission"},{"name","forbidden"}},"PLAN_DEPLOYED");
    const auto copy=send({{"action","copy_plan"}});const auto cp=SecurityPlanStore::str(copy,"plan_id");EXPECT_NE(cp,p);
    const auto copied=store.snapshot().at("plans").as_object().at(cp).as_object();EXPECT_EQ(copied.at("content_revision"),1);
    const auto cm=std::string(copied.at("mission_ids").as_array()[0].as_string());EXPECT_NE(cm,m);
    const auto all=store.snapshot().at("missions").as_object();EXPECT_NE(all.at(cm).as_object().at("route_id"),all.at(m).as_object().at("route_id"));
    send({{"action","update_plan"},{"plan_id",cp},{"name","Copy changed"}});
    EXPECT_EQ(old.at("deployments"),store.snapshot().at("deployments"));EXPECT_EQ(old.at("plans").as_object().at(p),store.snapshot().at("plans").as_object().at(p));
}
TEST_F(P4Workflow, SessionBlocksReviewDeployAndSaveFailureRetains){
    reviewed();begin();error({{"action","review"}},"PLAN_EDIT_IN_PROGRESS");error({{"action","deploy"}},"PLAN_EDIT_IN_PROGRESS");
    send({{"action","mark_route_dirty"},{"edit_session_id",sid}});
    auto bad=route();bad.at("waypoints").as_array()[0].as_object()["latitude"]=95;
    error({{"action","save_route"},{"edit_session_id",sid},{"path",bad}},"INVALID_WAYPOINT");
    error({{"action","save_route"},{"edit_session_id",sid},{"path",route()},{"expected_version",0}},"VERSION_CONFLICT");
    EXPECT_EQ(store.snapshot().at("edit_sessions").as_object().at(sid).as_object().at("state"),"DIRTY");
    send({{"action","save_route"},{"edit_session_id",sid},{"path",route()}});EXPECT_TRUE(store.snapshot().at("edit_sessions").as_object().empty());
}
TEST_F(P4Workflow, OwnerRevisionAndDiscard){
    begin();error({{"action","begin_route_edit"},{"content_revision",plan().at("content_revision")},{"instance_id","Other"}},"EDIT_SESSION_CONFLICT");
    error({{"action","discard_route_edit"},{"edit_session_id",sid},{"instance_id","Other"}},"EDIT_SESSION_CONFLICT");
    send({{"action","update_plan"},{"name","Modified"}});error({{"action","save_route"},{"edit_session_id",sid},{"path",route()}},"VERSION_CONFLICT");
    send({{"action","discard_route_edit"},{"edit_session_id",sid}});EXPECT_TRUE(store.snapshot().at("edit_sessions").as_object().empty());
}
TEST_F(P4Workflow, IndependentViewsNoopValidationAndBusinessIsolation){
    reviewed();const auto before=store.snapshot();UIPreferenceStore lang("");VideoViewStore video("");int events=0;auto pub=[&](const O&){++events;};
    EXPECT_EQ(lang.snapshot().at("language"),"en");lang.update({{"language","zh-Hans"}},uavs,"Command",pub);lang.update({{"language","zh-Hans"}},uavs,"Video",pub);
    EXPECT_EQ(events,1);EXPECT_EQ(lang.snapshot().at("ui_preferences_version"),1);
    video.update({{"video_target_uav_id","UAV-01"}},uavs,"Map",pub);video.update({{"video_target_uav_id","UAV-01"}},uavs,"Video",pub);EXPECT_EQ(events,2);
    EXPECT_THROW(video.update({{"video_target_uav_id","UAV-99"}},uavs,"Video",pub),PlanError);
    EXPECT_THROW(lang.update({{"language","zh"}},uavs,"Video",pub),PlanError);EXPECT_EQ(before,store.snapshot());
}
}

TEST_F(P4Workflow, PublisherFailureRetainsCompleteDeploymentState) {
    reviewed();const auto before=store.snapshot();
    EXPECT_THROW(store.transact({{"action","deploy"},{"plan_id",p},{"expected_version",before.at("version")}},uavs,"QA",[](const O&){throw std::runtime_error("publisher rejected");}),std::runtime_error);
    EXPECT_EQ(before,store.snapshot());EXPECT_TRUE(store.snapshot().at("deployments").as_object().empty());
    send({{"action","deploy"}});EXPECT_EQ(store.snapshot().at("deployments").as_object().size(),1u);
}
TEST(P4Atomicity, PersistenceFailurePublishesNothing) {
    const auto root=std::filesystem::temp_directory_path()/("p4-persistence-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);const auto path=root/"plans.json";
    SecurityPlanStore store(path.string());const auto before=store.snapshot();
    std::filesystem::create_directory(path.string()+".tmp");int published=0;
    EXPECT_THROW(store.transact({{"action","create_plan"},{"name","Never committed"}},{},"QA",[&](const O&){++published;}),std::runtime_error);
    EXPECT_EQ(before,store.snapshot());EXPECT_EQ(published,0);EXPECT_FALSE(std::filesystem::exists(path));
    std::filesystem::remove(path.string()+".tmp");std::filesystem::remove(root);
}

// P5.1 additive workflow contract, including legacy behavior above.
TEST_F(P4Workflow, WorkflowCreateHasAtomicDefaultTask) {
    const auto result=send({{"action","create_plan"},{"name","Workflow"},{"default_mission_name","Task 01"}});
    const auto id=SecurityPlanStore::str(result,"plan_id"),mid=SecurityPlanStore::str(result,"mission_id");
    const auto state=store.snapshot();const auto plan=state.at("plans").as_object().at(id).as_object();
    ASSERT_FALSE(mid.empty());EXPECT_EQ(plan.at("mission_ids").as_array().size(),1u);
    EXPECT_EQ(plan.at("workflow_step"),"TaskConfig");
    EXPECT_EQ(SecurityPlanStore::str(state.at("missions").as_object().at(mid).as_object(),"plan_id"),id);
}
TEST_F(P4Workflow, WorkflowSaveRetainsLeaseAndFinishRequiresCleanValidRoute) {
    send({{"action","assign"},{"assigned_uav_id","UAV-01"}});begin(true);
    error({{"action","finish_route_edit"},{"edit_session_id",sid}},"ROUTE_TOO_SHORT");
    send({{"action","mark_route_dirty"},{"edit_session_id",sid}});
    error({{"action","finish_route_edit"},{"edit_session_id",sid}},"PLAN_EDIT_IN_PROGRESS");
    send({{"action","save_route"},{"edit_session_id",sid},{"path",route()},{"keep_editing",true}});
    const auto session=store.snapshot().at("edit_sessions").as_object().at(sid).as_object();
    EXPECT_EQ(session.at("state"),"EDITING");EXPECT_EQ(session.at("base_content_revision"),plan().at("content_revision"));
    EXPECT_EQ(plan().at("workflow_step"),"RouteEditing");
    error({{"action","review"}},"PLAN_EDIT_IN_PROGRESS");
    send({{"action","save_route"},{"edit_session_id",sid},{"path",route()},{"keep_editing",true}});
    send({{"action","finish_route_edit"},{"edit_session_id",sid}});
    EXPECT_TRUE(store.snapshot().at("edit_sessions").as_object().empty());EXPECT_EQ(plan().at("workflow_step"),"PreDeployReview");
    send({{"action","validate"}});send({{"action","review"}});send({{"action","deploy"}});EXPECT_EQ(plan().at("status"),"DEPLOYED");
}
TEST_F(P4Workflow, WorkflowFinishOwnerAndInvalidSaveAreAtomic) {
    begin(true);const auto before=store.snapshot();
    error({{"action","finish_route_edit"},{"edit_session_id",sid},{"instance_id","Other"}},"EDIT_SESSION_CONFLICT");
    auto bad=route();bad.at("waypoints").as_array()[0].as_object()["latitude"]=100;
    error({{"action","save_route"},{"edit_session_id",sid},{"path",bad},{"keep_editing",true}},"INVALID_WAYPOINT");
    EXPECT_EQ(before,store.snapshot());
}
TEST_F(P4Workflow, WorkflowDraftCopyAndDeleteDoNotChangeOriginal) {
    const auto before=plan();const auto result=send({{"action","copy_plan"},{"allow_draft",true}});
    const auto id=SecurityPlanStore::str(result,"plan_id");EXPECT_NE(id,p);EXPECT_FALSE(SecurityPlanStore::str(result,"mission_id").empty());
    EXPECT_EQ(before,plan());send({{"action","delete_plan"},{"plan_id",id}});EXPECT_EQ(before,plan());
    EXPECT_FALSE(store.snapshot().at("plans").as_object().contains(id));
    reviewed();send({{"action","deploy"}});error({{"action","delete_plan"}},"PLAN_DEPLOYED");
}
