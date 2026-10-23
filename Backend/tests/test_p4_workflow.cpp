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
TEST_F(P4Workflow, OwnerDiscardSurvivesHeartbeatButSaveStillRequiresCurrentVersion) {
    ready();begin();send({{"action","mark_route_dirty"},{"edit_session_id",sid}});
    const auto acknowledged=store.snapshot();const auto oldVersion=acknowledged.at("version");
    store.heartbeat("Map-1",[](const O&){});
    ASSERT_GT(store.snapshot().at("version").to_number<int64_t>(),oldVersion.to_number<int64_t>());
    error({{"action","save_route"},{"edit_session_id",sid},{"path",route()},{"expected_version",oldVersion}},"VERSION_CONFLICT");
    EXPECT_NO_THROW(send({{"action","discard_route_edit"},{"edit_session_id",sid},{"expected_version",oldVersion}}));
    const auto after=store.snapshot();EXPECT_TRUE(after.at("edit_sessions").as_object().empty());
    EXPECT_EQ(after.at("paths"),acknowledged.at("paths"));EXPECT_EQ(plan().at("content_revision"),acknowledged.at("plans").as_object().at(p).as_object().at("content_revision"));
}
TEST_F(P4Workflow, HeartbeatStaleDiscardCannotCrossLeaseIdentityOrUseFutureVersion) {
    ready();begin();const auto oldSession=sid;const auto oldVersion=store.snapshot().at("version");
    store.heartbeat("Map-1",[](const O&){});
    error({{"action","discard_route_edit"},{"edit_session_id",sid},{"instance_id","Other"},{"expected_version",oldVersion}},"EDIT_SESSION_CONFLICT");
    error({{"action","discard_route_edit"},{"edit_session_id",sid},{"mission_id","wrong-mission"},{"expected_version",oldVersion}},"EDIT_SESSION_CONFLICT");
    error({{"action","discard_route_edit"},{"edit_session_id",sid},{"expected_version",store.snapshot().at("version").to_number<int64_t>()+1}},"VERSION_CONFLICT");
    send({{"action","discard_route_edit"},{"edit_session_id",sid}});begin();
    error({{"action","discard_route_edit"},{"edit_session_id",oldSession},{"expected_version",oldVersion}},"EDIT_SESSION_EXPIRED");
    EXPECT_TRUE(store.snapshot().at("edit_sessions").as_object().contains(sid));
}
TEST_F(P4Workflow, HeartbeatStaleDiscardStillRejectsExpiredLease) {
    ready();begin();auto before=store.snapshot();const auto oldVersion=before.at("version");
    before["version"]=oldVersion.to_number<int64_t>()+1;
    before.at("edit_sessions").as_object().at(sid).as_object()["lease_expires_at"]=0.;
    const auto file=std::filesystem::temp_directory_path()/("p4-expired-discard-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    {std::ofstream out(file);out<<boost::json::serialize(before);}
    SecurityPlanStore restored(file.string());
    try {restored.transact({{"action","discard_route_edit"},{"plan_id",p},{"mission_id",m},{"edit_session_id",sid},{"instance_id","Map-1"},{"expected_version",oldVersion}},uavs,"QA",[](const O&){});FAIL();}
    catch(const PlanError& e){EXPECT_EQ(e.code,"EDIT_SESSION_EXPIRED");}
    EXPECT_EQ(restored.snapshot(),before);std::filesystem::remove(file);
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
    const auto copied=store.snapshot().at("plans").as_object().at(id).as_object();
    EXPECT_EQ(before,plan());send({{"action","delete_plan"},{"plan_id",id},{"confirmation_name",copied.at("name")},{"expected_content_revision",copied.at("content_revision")}});EXPECT_EQ(before,plan());
    EXPECT_FALSE(store.snapshot().at("plans").as_object().contains(id));
    reviewed();send({{"action","deploy"}});error({{"action","delete_plan"}},"DELETE_CONFIRMATION_REQUIRED");
}

namespace {
struct PlanLifecycle : P4Workflow {
    O reopen() { return {{"action","reopen_plan"},{"expected_content_revision",plan().at("content_revision")}}; }
    O deletion() { return {{"action","delete_plan"},{"confirmation_name",plan().at("name")},{"expected_content_revision",plan().at("content_revision")}}; }
    void deployed() { reviewed();send({{"action","deploy"}}); }
    void withExecution(const std::string& status,const std::function<void(SecurityPlanStore&,const O&)>& check) {
        deployed();auto snapshot=store.snapshot();
        snapshot.at("executions").as_object()["history-1"]=O{{"execution_id","history-1"},{"plan_id",p},{"mission_id",m},
            {"state",status},{"simulation",false},{"route_snapshot",route()},{"deployment_id",plan().at("deployment_id")}};
        const auto file=std::filesystem::temp_directory_path()/("plan-lifecycle-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
        {std::ofstream out(file);out<<boost::json::serialize(snapshot);}
        SecurityPlanStore restored(file.string());check(restored,restored.snapshot());std::filesystem::remove(file);
    }
};
}
TEST_F(PlanLifecycle, EditingDeployedPlanKeepsPlanMissionRouteAndHistoryIdentity) {
    deployed();const auto before=store.snapshot();const auto revision=plan().at("content_revision").to_number<int64_t>();
    send({{"action","begin_configuration"}});const auto lease=store.snapshot().at("configuration_session");
    const auto result=send(reopen());EXPECT_EQ(SecurityPlanStore::str(result,"plan_id"),p);EXPECT_EQ(store.snapshot().at("plans").as_object().size(),1u);
    EXPECT_EQ(plan().at("mission_ids"),before.at("plans").as_object().at(p).as_object().at("mission_ids"));
    EXPECT_EQ(store.snapshot().at("paths"),before.at("paths"));EXPECT_EQ(store.snapshot().at("deployments"),before.at("deployments"));
    EXPECT_EQ(store.snapshot().at("configuration_session"),lease);EXPECT_EQ(plan().at("status"),"DRAFT");
    EXPECT_GT(plan().at("content_revision").to_number<int64_t>(),revision);
    send({{"action","update_plan"},{"name","Edited original"}});
    send({{"action","validate"}});send({{"action","review"}});send({{"action","deploy"}});
    EXPECT_EQ(plan().at("status"),"DEPLOYED");EXPECT_EQ(store.snapshot().at("plans").as_object().size(),1u);
    EXPECT_EQ(store.snapshot().at("deployments").as_object().size(),2u);
    const auto oldId=SecurityPlanStore::str(before.at("plans").as_object().at(p).as_object(),"deployment_id");
    EXPECT_EQ(store.snapshot().at("deployments").as_object().at(oldId),before.at("deployments").as_object().at(oldId));
}
TEST_F(PlanLifecycle, ConflictingReeditCanExitCorrectAndRedeploySamePlan) {
    store.configureMock({{"default_speed_mps",5.},{"safety_separation_m",1.5},{"uavs",O{
        {"UAV-01",O{{"home_position",O{{"latitude",39.},{"longitude",115.9999},{"altitude",60.}}}}},
        {"UAV-02",O{{"home_position",O{{"latitude",39.},{"longitude",116.0001},{"altitude",60.}}}}}}}});
    send({{"action","assign"},{"assigned_uav_ids",A{"UAV-01","UAV-02"}},{"spacing_m",4.}});
    auto safe=route();safe.at("waypoints").as_array()[1].as_object()["longitude"]=116.;
    begin();send({{"action","save_route"},{"edit_session_id",sid},{"path",safe}});
    send({{"action","validate"}});ASSERT_EQ(plan().at("status"),"READY");
    send({{"action","review"}});send({{"action","deploy"}});
    const auto deployedState=store.snapshot();const auto oldDeployment=plan().at("deployment_id");
    const auto routeId=deployedState.at("missions").as_object().at(m).as_object().at("route_id");
    send({{"action","begin_configuration"}});send(reopen());
    // Reversing the route reverses the line slots: both approaches now cross.
    auto crossing=safe;crossing.at("waypoints").as_array()[1].as_object()["latitude"]=38.999;
    begin(true);send({{"action","save_route"},{"edit_session_id",sid},{"path",crossing},{"keep_editing",true}});
    send({{"action","finish_route_edit"},{"edit_session_id",sid}});send({{"action","validate"}});
    ASSERT_EQ(plan().at("status"),"DRAFT");EXPECT_EQ(plan().at("workflow_step"),"PreDeployReview");
    EXPECT_EQ(store.snapshot().at("missions").as_object().at(m).as_object().at("status"),"DRAFT");
    const auto validation=plan().at("validation").as_object();EXPECT_FALSE(validation.at("ready").as_bool());
    ASSERT_EQ(validation.at("issues").as_array().size(),1u);
    const auto issue=validation.at("issues").as_array()[0].as_object();
    EXPECT_EQ(std::string(issue.at("mission_id").as_string()),m);EXPECT_EQ(issue.at("code"),"TRAJECTORY_CONFLICT");
    const auto params=issue.at("params").as_object();EXPECT_EQ(params.at("uav_a"),"UAV-01");EXPECT_EQ(params.at("uav_b"),"UAV-02");
    EXPECT_LT(params.at("minimum_separation_m").to_number<double>(),params.at("required_separation_m").to_number<double>());
    error({{"action","review"}},"PLAN_NOT_READY");error({{"action","deploy"}},"PLAN_NOT_READY");
    const auto blocked=store.snapshot();send({{"action","end_configuration"}});
    EXPECT_TRUE(store.snapshot().at("configuration_session").is_null());
    EXPECT_EQ(store.snapshot().at("paths"),blocked.at("paths"));EXPECT_EQ(store.snapshot().at("deployments"),deployedState.at("deployments"));
    send({{"action","begin_configuration"}});begin(true);
    send({{"action","save_route"},{"edit_session_id",sid},{"path",safe},{"keep_editing",true}});
    send({{"action","finish_route_edit"},{"edit_session_id",sid}});send({{"action","validate"}});
    ASSERT_EQ(plan().at("status"),"READY");EXPECT_EQ(store.snapshot().at("missions").as_object().at(m).as_object().at("status"),"READY");
    send({{"action","review"}});send({{"action","deploy"}});const auto after=store.snapshot();
    EXPECT_EQ(plan().at("status"),"DEPLOYED");EXPECT_EQ(after.at("plans").as_object().size(),1u);
    EXPECT_EQ(plan().at("mission_ids"),deployedState.at("plans").as_object().at(p).as_object().at("mission_ids"));
    EXPECT_EQ(after.at("missions").as_object().at(m).as_object().at("route_id"),routeId);
    EXPECT_EQ(after.at("deployments").as_object().size(),2u);EXPECT_NE(plan().at("deployment_id"),oldDeployment);
    EXPECT_EQ(after.at("deployments").as_object().at(oldDeployment.as_string()),deployedState.at("deployments").as_object().at(oldDeployment.as_string()));
    EXPECT_TRUE(after.at("configuration_session").is_null());EXPECT_TRUE(after.at("executions").as_object().empty());
}
TEST_F(PlanLifecycle, ReopenRequiresOwnedLeaseAndCurrentRevision) {
    deployed();error(reopen(),"CONFIGURATION_REQUIRED");send({{"action","begin_configuration"}});
    auto wrong=reopen();wrong["instance_id"]="Other";error(wrong,"CONFIGURATION_IN_PROGRESS");
    auto stale=reopen();stale["expected_content_revision"]=0;error(stale,"CONTENT_REVISION_CONFLICT");
    auto staleVersion=reopen();staleVersion["expected_version"]=0;error(staleVersion,"VERSION_CONFLICT");
}
TEST_F(PlanLifecycle, DeleteReadyPlanRequiresExactNameAndRevision) {
    ready();auto wrong=deletion();wrong["confirmation_name"]="Other plan";error(wrong,"DELETE_CONFIRMATION_REQUIRED");
    auto stale=deletion();stale["expected_content_revision"]=0;error(stale,"CONTENT_REVISION_CONFLICT");
    const auto before=store.snapshot();send(deletion());const auto after=store.snapshot();
    EXPECT_FALSE(after.at("plans").as_object().contains(p));EXPECT_FALSE(after.at("missions").as_object().contains(m));
    EXPECT_TRUE(after.at("paths").as_object().empty());
    EXPECT_EQ(after.at("deleted_plans").as_object().at(p).as_object().at("snapshot").as_object().at("plan"),before.at("plans").as_object().at(p));
    EXPECT_EQ(after.at("events").as_array().back().as_object().at("params").as_object().at("plan_name"),before.at("plans").as_object().at(p).as_object().at("name"));
}
TEST_F(PlanLifecycle, DeleteBlocksConfigurationAndRouteEditAtomically) {
    begin();error(deletion(),"PLAN_EDIT_IN_PROGRESS");send({{"action","discard_route_edit"},{"edit_session_id",sid}});
    send({{"action","begin_configuration"}});error(deletion(),"CONFIGURATION_IN_PROGRESS");
    send({{"action","end_configuration"}});send(deletion());EXPECT_FALSE(store.snapshot().at("plans").as_object().contains(p));
}
TEST_F(PlanLifecycle, DeleteAndConfigurationRejectEveryNonterminalExecutionIncludingScheduled) {
    // A persisted execution fixture verifies the gate without dispatching hardware.
    const std::vector<std::string> states{"SCHEDULED","CREATED","PREFLIGHT","STARTING","START_REQUESTED","WAITING_ACK","EXECUTING","PAUSED","RETURNING"};
    deployed();const auto base=store.snapshot();
    for(const auto& status:states) {
        auto snapshot=base;snapshot.at("executions").as_object()["busy"]=O{{"execution_id","busy"},{"plan_id",p},{"mission_id",m},{"state",status}};
        const auto file=std::filesystem::temp_directory_path()/("plan-lifecycle-busy-"+status+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
        {std::ofstream out(file);out<<boost::json::serialize(snapshot);}SecurityPlanStore restored(file.string());
        auto req=deletion();req["plan_id"]=p;req["instance_id"]="Map-1";req["expected_version"]=snapshot.at("version");
        try{restored.transact(req,uavs,"QA",[](const O&){});FAIL()<<status;}catch(const PlanError& e){EXPECT_EQ(e.code,"PLAN_EXECUTING")<<status;}
        EXPECT_EQ(restored.snapshot(),snapshot);
        try{restored.transact({{"action","begin_configuration"},{"plan_id",p},{"instance_id","Map-1"}},uavs,"QA",[](const O&){});FAIL()<<status;}
        catch(const PlanError& e){EXPECT_EQ(e.code,"PLAN_EXECUTING")<<status;}
        EXPECT_EQ(restored.snapshot(),snapshot);EXPECT_TRUE(restored.snapshot().at("configuration_session").is_null());
        auto before=restored.snapshot();req=reopen();req["plan_id"]=p;req["instance_id"]="Map-1";req["expected_version"]=before.at("version");
        try{restored.transact(req,uavs,"QA",[](const O&){});FAIL()<<status;}catch(const PlanError& e){EXPECT_EQ(e.code,"CONFIGURATION_REQUIRED")<<status;}
        EXPECT_EQ(restored.snapshot(),before);std::filesystem::remove(file);
    }
}
TEST_F(PlanLifecycle, DeletePreservesRealTerminalExecutionAndImmutableDeployment) {
    withExecution("COMPLETED",[&](SecurityPlanStore& restored,const O& before){
        auto request=deletion();request["plan_id"]=p;request["instance_id"]="Map-1";request["expected_version"]=before.at("version");
        restored.transact(request,uavs,"QA",[](const O&){});auto after=restored.snapshot();
        EXPECT_EQ(after.at("executions"),before.at("executions"));EXPECT_EQ(after.at("deployments"),before.at("deployments"));
        EXPECT_FALSE(after.at("plans").as_object().contains(p));EXPECT_TRUE(after.at("deleted_plans").as_object().contains(p));
    });
}
TEST_F(PlanLifecycle, ConfirmedDeletionCannotTargetRenamedOrChangedContentAndRetryIsSafe) {
    auto consent=deletion();send({{"action","update_plan"},{"name","Changed after confirmation"}});
    error(consent,"DELETE_CONFIRMATION_REQUIRED");
    consent=deletion();send({{"action","update_plan"},{"name","Changed after confirmation"},{"description","Different content"}});
    error(consent,"CONTENT_REVISION_CONFLICT");
    consent=deletion();send(consent);const auto removed=store.snapshot();
    error(consent,"PLAN_NOT_FOUND");EXPECT_EQ(removed,store.snapshot());
}
TEST_F(PlanLifecycle, LifecyclePublisherFailureRollsBackArchiveAndCurrentState) {
    const auto before=store.snapshot();auto request=deletion();request["plan_id"]=p;request["expected_version"]=before.at("version");
    EXPECT_THROW(store.transact(request,uavs,"QA",[](const O&){throw std::runtime_error("publisher rejected");}),std::runtime_error);
    EXPECT_EQ(before,store.snapshot());
    deployed();send({{"action","begin_configuration"}});const auto deployedBefore=store.snapshot();
    request=reopen();request["plan_id"]=p;request["instance_id"]="Map-1";request["expected_version"]=deployedBefore.at("version");
    EXPECT_THROW(store.transact(request,uavs,"QA",[](const O&){throw std::runtime_error("publisher rejected");}),std::runtime_error);
    EXPECT_EQ(deployedBefore,store.snapshot());
}

namespace {
struct GuardedDeployment : P4Workflow {
    void SetUp() override {P4Workflow::SetUp();ready();send({{"action","begin_configuration"}});send({{"action","review"}});}
    O guarded(const char* action) {
        const auto state=store.snapshot();const auto chosen=plan();
        return {{"action",action},{"expected_version",state.at("version")},{"expected_plan_version",chosen.at("version")},
            {"expected_content_revision",chosen.at("content_revision")},
            {"configuration_acquired_version",state.at("configuration_session").as_object().at("acquired_version")}};
    }
};
}
TEST_F(GuardedDeployment, HeartbeatBetweenAllThreeStagesPreservesConfirmedPlan) {
    const auto revision=plan().at("content_revision");const auto lease=store.snapshot().at("configuration_session").as_object().at("acquired_version");
    for(const auto* action:{"validate","review","deploy"}){
        auto request=guarded(action);const auto expectedPlan=plan().at("version");
        store.heartbeat("Map-1",[](const O&){});
        EXPECT_GT(store.snapshot().at("version").to_number<int64_t>(),request.at("expected_version").to_number<int64_t>());
        EXPECT_EQ(plan().at("version"),expectedPlan);EXPECT_EQ(request.at("configuration_acquired_version"),lease);
        const auto reply=send(request);EXPECT_EQ(reply.at("state"),store.snapshot());
        EXPECT_EQ(plan().at("content_revision"),revision);
        EXPECT_EQ(plan().at("version").to_number<int64_t>(),expectedPlan.to_number<int64_t>()+1);
    }
    EXPECT_EQ(plan().at("status"),"DEPLOYED");EXPECT_EQ(store.snapshot().at("deployments").as_object().size(),1u);
    EXPECT_TRUE(store.snapshot().at("configuration_session").is_null());EXPECT_TRUE(store.snapshot().at("executions").as_object().empty());
}
TEST_F(GuardedDeployment, GuardsRejectMismatchEvenAtCurrentGlobalVersion) {
    for(const auto* action:{"validate","review","deploy"}){
        auto bad=guarded(action);bad["expected_content_revision"]=plan().at("content_revision").to_number<int64_t>()-1;
        error(bad,"CONTENT_REVISION_CONFLICT");
        bad=guarded(action);bad["expected_plan_version"]=plan().at("version").to_number<int64_t>()-1;error(bad,"VERSION_CONFLICT");
        bad=guarded(action);bad["configuration_acquired_version"]=0;error(bad,"VERSION_CONFLICT");
        bad=guarded(action);bad["instance_id"]="Other";error(bad,"CONFIGURATION_IN_PROGRESS");
        bad=guarded(action);bad["plan_id"]="another-plan";error(bad,"CONFIGURATION_IN_PROGRESS");
        for(const auto* key:{"expected_content_revision","expected_plan_version","configuration_acquired_version"}){
            bad=guarded(action);bad[key]="wrong-type";error(bad,"VERSION_CONFLICT");
        }
    }
}
TEST_F(GuardedDeployment, StaleContentAndPlanStateCannotBeReconfirmed) {
    for(const auto* action:{"validate","review","deploy"}){
        auto old=guarded(action);send({{"action","rename_mission"},{"name","Edited after confirmation"}});
        old["expected_version"]=store.snapshot().at("version");old["expected_plan_version"]=plan().at("version");
        error(old,"CONTENT_REVISION_CONFLICT");
        old=guarded(action);send({{"action","validate"}});old["expected_version"]=store.snapshot().at("version");
        error(old,"VERSION_CONFLICT");
    }
}
TEST_F(GuardedDeployment, ReacquiredAndMissingLeaseCannotUseOldGuard) {
    std::vector<O> requests;for(const auto* action:{"validate","review","deploy"})requests.push_back(guarded(action));
    send({{"action","end_configuration"}});
    for(auto request:requests){request["expected_version"]=store.snapshot().at("version");error(request,"VERSION_CONFLICT");}
    send({{"action","begin_configuration"}});
    for(auto request:requests){request["expected_version"]=store.snapshot().at("version");error(request,"VERSION_CONFLICT");}
}
TEST_F(GuardedDeployment, LegacyAndIncompleteGuardsKeepStrictVersions) {
    for(const auto* action:{"validate","review","deploy"}){
        const auto stale=store.snapshot().at("version");store.heartbeat("Map-1",[](const O&){});
        error({{"action",action},{"expected_version",stale}},"VERSION_CONFLICT");
        for(const auto* key:{"expected_content_revision","expected_plan_version","configuration_acquired_version"}){
            auto incomplete=guarded(action);incomplete.erase(key);error(incomplete,"VERSION_CONFLICT");
        }
        auto missingVersion=guarded(action);missingVersion.erase("expected_version");
        // Bypass send's default expected_version so the protocol omission is real.
        missingVersion["plan_id"]=p;missingVersion["instance_id"]="Map-1";
        const auto before=store.snapshot();
        try{store.transact(missingVersion,uavs,"QA",[](const O&){});FAIL()<<action;}
        catch(const PlanError& e){EXPECT_EQ(e.code,"VERSION_CONFLICT");}EXPECT_EQ(store.snapshot(),before);
    }
}
TEST_F(GuardedDeployment, FutureAndPreLeaseVersionsRejected) {
    for(const auto* action:{"validate","review","deploy"}){
        auto bad=guarded(action);bad["expected_version"]=store.snapshot().at("version").to_number<int64_t>()+1;error(bad,"VERSION_CONFLICT");
        bad=guarded(action);bad["expected_version"]=bad.at("configuration_acquired_version").to_number<int64_t>()-1;error(bad,"VERSION_CONFLICT");
        bad=guarded(action);bad["expected_version"]=store.snapshot().at("version").to_number<double>()-.5;error(bad,"VERSION_CONFLICT");
    }
}
TEST_F(GuardedDeployment, HeartbeatAllowanceStillChecksRouteLeaseAndReadiness) {
    begin();
    for(const auto* action:{"review","deploy"}){auto request=guarded(action);store.heartbeat("Map-1",[](const O&){});error(request,"PLAN_EDIT_IN_PROGRESS");}
    send({{"action","discard_route_edit"},{"edit_session_id",sid}});send({{"action","assign"},{"assigned_uav_ids",A{}}});
    auto check=guarded("validate");store.heartbeat("Map-1",[](const O&){});send(check);
    EXPECT_EQ(plan().at("status"),"DRAFT");EXPECT_FALSE(plan().at("validation").as_object().at("ready").as_bool());
    for(const auto* action:{"review","deploy"}){auto request=guarded(action);store.heartbeat("Map-1",[](const O&){});error(request,"PLAN_NOT_READY");}
    EXPECT_TRUE(store.snapshot().at("deployments").as_object().empty());
}

TEST_F(GuardedDeployment, WorkflowOnlyChangesInvalidatePlanGuardButNoopsDoNot) {
    std::vector<O> requests;for(const auto* action:{"validate","review","deploy"})requests.push_back(guarded(action));
    const auto revision=plan().at("content_revision");const auto version=plan().at("version").to_number<int64_t>();
    send({{"action","set_workflow_step"},{"workflow_step","BasicInfo"}});
    EXPECT_EQ(plan().at("content_revision"),revision);EXPECT_EQ(plan().at("version").to_number<int64_t>(),version+1);
    for(auto request:requests){request["expected_version"]=store.snapshot().at("version");error(request,"VERSION_CONFLICT");}
    const auto unchanged=plan();send({{"action","set_workflow_step"},{"workflow_step","BasicInfo"}});
    EXPECT_EQ(plan().at("version"),unchanged.at("version"));EXPECT_EQ(plan().at("updated_at"),unchanged.at("updated_at"));
    begin(true);const auto editingVersion=plan().at("version");send({{"action","mark_route_dirty"},{"edit_session_id",sid}});
    EXPECT_EQ(plan().at("version"),editingVersion);EXPECT_EQ(plan().at("content_revision"),revision);
    send({{"action","discard_route_edit"},{"edit_session_id",sid}});
    EXPECT_EQ(plan().at("version").to_number<int64_t>(),editingVersion.to_number<int64_t>()+1);
}
