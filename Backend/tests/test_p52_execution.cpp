#include <gtest/gtest.h>
#include "storage/security_plan_store.h"
using O=boost::json::object;using A=boost::json::array;
namespace {
struct P52Execution:testing::Test {
    SecurityPlanStore store{""};std::set<std::string> uavs{"UAV-01","UAV-02"};std::string p,m,sid;
    O send(O r) {if(!r.contains("expected_version"))r["expected_version"]=store.snapshot().at("version");
        if(!r.contains("instance_id"))r["instance_id"]="Map-1";
        if(!r.contains("plan_id"))r["plan_id"]=p;if(!r.contains("mission_id"))r["mission_id"]=m;
        return store.transact(r,uavs,"QA",[](const O&){});}
    O plan(){return store.snapshot().at("plans").as_object().at(p).as_object();}
    void SetUp() override {store.configureMock({{"default_speed_mps",50.},{"uavs",O{{"UAV-01",O{{"home_position",O{{"latitude",39.},{"longitude",116.},{"altitude",60.}}}}}}}});p=SecurityPlanStore::str(send({{"action","create_plan"},{"name","原名"}}),"plan_id");
        m=SecurityPlanStore::str(send({{"action","add_mission"},{"name","East Perimeter"}}),"mission_id");}
    void begin(bool workflow=false){sid=SecurityPlanStore::str(send({{"action","begin_route_edit"},{"content_revision",plan().at("content_revision")},{"workflow",workflow}}),"edit_session_id");}
    O route(){return {{"pathId",1},{"bClosedLoop",false},{"waypoints",A{
        O{{"sequence",1},{"latitude",39.},{"longitude",116.},{"altitude",60.},{"segmentSpeed",0},{"waitTime",0}},
        O{{"sequence",2},{"latitude",39.001},{"longitude",116.001},{"altitude",60.},{"segmentSpeed",5},{"waitTime",2}}}}};}
    void ready(){send({{"action","assign"},{"assigned_uav_id","UAV-01"}});begin();
        send({{"action","save_route"},{"edit_session_id",sid},{"path",route()}});send({{"action","validate"}});}
    void reviewed(){ready();send({{"action","review"}});}
    void error(O r,const char* code){const auto before=store.snapshot();try{send(r);FAIL()<<code;}catch(const PlanError& e){EXPECT_EQ(e.code,code);}EXPECT_EQ(before,store.snapshot());}
    std::string execution;
    O start(const std::string& key="start-1") {auto r=send({{"action","execution_start"},{"request_id",key}});execution=SecurityPlanStore::str(r,"execution_id");return r;}
    O exec(){return store.snapshot().at("executions").as_object().at(execution).as_object();}
    void tick(double dt=.1){store.tickExecutions(dt,uavs,[](const O&){});}
    void deployed(){reviewed();send({{"action","deploy"}});}
    void running(){deployed();start();tick();tick();tick();}
    void control(const char* action,const char* key){send({{"action",action},{"execution_id",execution},{"request_id",key},{"control_version",exec().at("control_version")}});}

};

TEST_F(P52Execution, CreateAndExplicitStartStateMachine){deployed();EXPECT_TRUE(store.snapshot().at("executions").as_object().empty());start();EXPECT_EQ(exec().at("state"),"CREATED");tick();EXPECT_EQ(exec().at("state"),"PREFLIGHT");tick();EXPECT_EQ(exec().at("state"),"STARTING");tick();EXPECT_EQ(exec().at("state"),"EXECUTING");EXPECT_EQ(plan().at("status"),"DEPLOYED");}
TEST_F(P52Execution, PauseResumePreservesPosition){running();tick();control("execution_pause","p");auto e=exec();tick(1);EXPECT_EQ(exec(),e);control("execution_resume","r");EXPECT_EQ(exec().at("position"),e.at("position"));tick(1);EXPECT_NE(exec().at("position"),e.at("position"));}
TEST_F(P52Execution, Completion){running();for(int i=0;i<400 && exec().at("state")!="COMPLETED";++i)tick(1);EXPECT_EQ(exec().at("state"),"COMPLETED");EXPECT_EQ(exec().at("progress"),1.);EXPECT_EQ(exec().at("completion_reason"),"ROUTE_FINISHED");EXPECT_EQ(exec().at("completed_waypoints"),2);}
TEST_F(P52Execution, AbortStops){running();tick();control("execution_abort","a");const auto e=exec();tick(1);EXPECT_EQ(exec(),e);EXPECT_EQ(e.at("state"),"ABORTED");}
TEST_F(P52Execution, ReturnToExplicitHome){running();tick(1);control("execution_return","r");for(int i=0;i<400 && exec().at("state")!="COMPLETED";++i)tick(1);EXPECT_EQ(exec().at("completion_reason"),"RETURNED_HOME");EXPECT_EQ(exec().at("position"),exec().at("home_position"));}
TEST_F(P52Execution, DuplicateAndConflictingStarts){deployed();auto first=start();start();EXPECT_EQ(store.snapshot().at("executions").as_object().size(),1u);error({{"action","execution_start"},{"request_id","start-2"}},"EXECUTION_CONFLICT");}
TEST_F(P52Execution, ImmutableDeploymentRoute){running();auto old=exec();auto copied=send({{"action","copy_plan"}});const auto id=SecurityPlanStore::str(copied,"plan_id");send({{"action","update_plan"},{"plan_id",id},{"name","New draft"}});tick();EXPECT_EQ(exec().at("route_snapshot"),old.at("route_snapshot"));EXPECT_EQ(exec().at("route_revision"),old.at("route_revision"));}
TEST_F(P52Execution, MissingFixtureFailsWithReason){running();uavs.clear();tick();EXPECT_EQ(exec().at("state"),"FAILED");EXPECT_EQ(exec().at("failure_reason"),"MOCK_UAV_UNAVAILABLE");}
TEST_F(P52Execution, StaleControlDoesNotMutate){running();error({{"action","execution_pause"},{"execution_id",execution},{"request_id","p"},{"control_version",-1}},"EXECUTION_STALE");}
TEST_F(P52Execution, ProgressDoesNotInvalidatePlanVersion){running();auto version=store.snapshot().at("version");tick();EXPECT_EQ(store.snapshot().at("version"),version);}
TEST_F(P52Execution, ShutdownPausesAndResumesReturn){running();tick(1);control("execution_return","r");send({{"action","execution_shutdown"},{"request_id","shutdown"}});EXPECT_EQ(exec().at("state"),"PAUSED");auto pos=exec().at("position");tick();EXPECT_EQ(exec().at("position"),pos);control("execution_resume","resume");EXPECT_EQ(exec().at("state"),"RETURNING");}
TEST_F(P52Execution, DurableRestartRetainsPositionAndContinues){running();tick(1);const auto path=std::filesystem::temp_directory_path()/("p52-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");{std::ofstream out(path);out<<boost::json::serialize(store.snapshot());}SecurityPlanStore recovered(path.string());EXPECT_EQ(recovered.snapshot(),store.snapshot());recovered.configureMock({{"uavs",O{{"UAV-01",O{}}}}});recovered.tickExecutions(1,uavs,[](const O&){});EXPECT_NE(recovered.snapshot().at("executions").as_object().at(execution).as_object().at("position"),exec().at("position"));std::filesystem::remove(path);}
TEST_F(P52Execution, PersistencePublisherFailureIsAtomic){deployed();const auto before=store.snapshot();EXPECT_THROW(store.transact({{"action","execution_start"},{"plan_id",p},{"mission_id",m},{"request_id","x"}},uavs,"QA",[](const O&){throw std::runtime_error("rejected");}),std::runtime_error);EXPECT_EQ(store.snapshot(),before);}
// P5.3 synthetic protocol geometry fixtures. Native coordinate tests exercise Cesium separately.
O geometryRoute(){O r{{"pathId",1},{"bClosedLoop",true}};A points;
    for(int i=0;i<4;++i)points.emplace_back(O{{"sequence",i+1},{"latitude",39.+(i>=2?.0001:0)},{"longitude",116.+(i==1 || i==2?.0001:0)},{"altitude",60.},{"segmentSpeed",50.},{"waitTime",0.},{"location",O{{"x",i>=2?1000.:0.},{"y",i==1 || i==2?1000.:0.},{"z",6000.}}}});
    r["waypoints"]=points;return r;
}
struct P53Geometry:P52Execution {
    void readyGeometry(){send({{"action","assign"},{"assigned_uav_id","UAV-01"}});begin();send({{"action","save_route"},{"edit_session_id",sid},{"path",geometryRoute()}});send({{"action","validate"}});}
    void deployedGeometry(){readyGeometry();send({{"action","review"}});send({{"action","deploy"}});}
    std::string move(){return SecurityPlanStore::str(send({{"action","begin_plan_move"}}),"edit_session_id");}
    O translated(){auto routes=store.snapshot().at("paths").as_object();for(auto& item:routes)for(auto& v:item.value().as_object().at("waypoints").as_array()){auto& p=v.as_object();p["latitude"]=p.at("latitude").to_number<double>()+.001;auto& l=p.at("location").as_object();l["x"]=l.at("x").to_number<double>()+10000.;l["y"]=l.at("y").to_number<double>()+20000.;}return routes;}
};
TEST_F(P53Geometry, ClosedRoundTripAndOnceCompletion){deployedGeometry();const auto original=store.snapshot().at("paths");start();for(int i=0;i<200 && exec().at("state")!="COMPLETED";++i)tick(1);EXPECT_EQ(exec().at("state"),"COMPLETED");EXPECT_EQ(exec().at("completed_waypoints"),4);EXPECT_EQ(exec().at("total_waypoints"),4);EXPECT_TRUE(exec().at("closure_completed").as_bool());EXPECT_EQ(exec().at("position"),exec().at("home_position"));EXPECT_EQ(store.snapshot().at("paths"),original);}
TEST_F(P53Geometry, RejectShortClosedAndInvalidFlag){begin();auto r=route();r["bClosedLoop"]=true;error({{"action","save_route"},{"edit_session_id",sid},{"path",r}},"CLOSED_ROUTE_TOO_SHORT");r["bClosedLoop"]="true";error({{"action","save_route"},{"edit_session_id",sid},{"path",r}},"INVALID_ROUTE");}
TEST_F(P53Geometry, CancelPreservesAllGeometry){readyGeometry();auto paths=store.snapshot().at("paths");auto p0=plan();auto session=move();send({{"action","cancel_plan_move"},{"edit_session_id",session}});EXPECT_EQ(store.snapshot().at("paths"),paths);EXPECT_EQ(plan(),p0);}
TEST_F(P53Geometry, DeployedTranslationCreatesImmutableReplacement){deployedGeometry();const auto old=store.snapshot().at("deployments");const auto previous=plan().at("deployment_id");auto session=move();auto routes=translated();send({{"action","move_plan"},{"edit_session_id",session},{"paths",routes}});EXPECT_EQ(plan().at("status"),"DEPLOYED");EXPECT_NE(plan().at("deployment_id"),previous);EXPECT_EQ(store.snapshot().at("deployments").as_object().at(previous.as_string()),old.as_object().at(previous.as_string()));const auto current=store.snapshot().at("deployments").as_object().at(plan().at("deployment_id").as_string()).as_object();EXPECT_EQ(current.at("snapshot").as_object().at("paths"),store.snapshot().at("paths"));start();EXPECT_EQ(exec().at("route_snapshot"),store.snapshot().at("paths").as_object().begin()->value());}
TEST_F(P53Geometry, RejectNonRigidAndMissingRoutesAtomically){readyGeometry();const auto session=move();auto routes=translated();auto& pts=routes.begin()->value().as_object().at("waypoints").as_array();pts[1].as_object().at("location").as_object()["x"]=999.;error({{"action","move_plan"},{"edit_session_id",session},{"paths",routes}},"INVALID_TRANSLATION");error({{"action","move_plan"},{"edit_session_id",session},{"paths",O{}}},"INVALID_ROUTE");}
TEST_F(P53Geometry, ActiveExecutionBlocksAllMoveStates){deployedGeometry();start();error({{"action","begin_plan_move"}},"PLAN_EXECUTING");tick();tick();tick();error({{"action","begin_plan_move"}},"PLAN_EXECUTING");control("execution_pause","pause");error({{"action","begin_plan_move"}},"PLAN_EXECUTING");control("execution_resume","resume");control("execution_return","return");error({{"action","begin_plan_move"}},"PLAN_EXECUTING");}
TEST_F(P53Geometry, MoveLeaseBlocksExecutionAndRouteEditing){deployedGeometry();auto session=move();error({{"action","execution_start"},{"request_id","move-start"}},"PLAN_EDIT_IN_PROGRESS");send({{"action","cancel_plan_move"},{"edit_session_id",session}});start();EXPECT_EQ(exec().at("state"),"CREATED");}
TEST_F(P53Geometry, StaleMoveCannotOverwritePlan){readyGeometry();const auto session=move();auto routes=translated();send({{"action","update_plan"},{"name","new"}});error({{"action","move_plan"},{"edit_session_id",session},{"paths",routes}},"VERSION_CONFLICT");}
TEST_F(P53Geometry, UnauthorizedMoveCannotCommit){readyGeometry();const auto session=move();error({{"action","move_plan"},{"instance_id","stranger"},{"edit_session_id",session},{"paths",translated()}},"EDIT_SESSION_CONFLICT");}
TEST_F(P53Geometry, ReloadPreservesTranslatedDeployment){deployedGeometry();const auto session=move();send({{"action","move_plan"},{"edit_session_id",session},{"paths",translated()}});const auto file=std::filesystem::temp_directory_path()/("p53-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");{std::ofstream out(file);out<<boost::json::serialize(store.snapshot());}SecurityPlanStore restored(file.string());EXPECT_EQ(restored.snapshot(),store.snapshot());std::filesystem::remove(file);}

TEST_F(P53Geometry, AllMissionRoutesUseOneDelta){readyGeometry();const auto first=m;const auto added=send({{"action","add_mission"},{"name","second"}});m=SecurityPlanStore::str(added,"mission_id");begin();auto route2=geometryRoute();route2["bClosedLoop"]=false;send({{"action","save_route"},{"edit_session_id",sid},{"path",route2}});m=first;const auto session=move();auto paths=translated();EXPECT_EQ(paths.size(),2u);send({{"action","move_plan"},{"edit_session_id",session},{"paths",paths}});for(const auto& e:paths)EXPECT_EQ(store.snapshot().at("paths").as_object().at(e.key()).as_object().at("waypoints"),e.value().as_object().at("waypoints"));}
TEST_F(P53Geometry, TranslationPublisherFailureRollsBack){readyGeometry();const auto session=move();const auto before=store.snapshot();O r{{"action","move_plan"},{"instance_id","Map-1"},{"plan_id",p},{"mission_id",m},{"edit_session_id",session},{"paths",translated()}};EXPECT_THROW(store.transact(r,uavs,"QA",[](const O&){throw std::runtime_error("publisher rejected");}),std::runtime_error);EXPECT_EQ(store.snapshot(),before);}

TEST_F(P53Geometry, MoveLeaseCannotBeReusedForWaypointEdits){readyGeometry();const auto session=move();error({{"action","begin_route_edit"},{"content_revision",plan().at("content_revision")}},"EDIT_SESSION_CONFLICT");error({{"action","save_route"},{"edit_session_id",session},{"path",geometryRoute()}},"EDIT_SESSION_CONFLICT");error({{"action","mark_route_dirty"},{"edit_session_id",session}},"EDIT_SESSION_CONFLICT");}

TEST_F(P53Geometry, CanonicalClosedRouteAliasAndConflict){begin();auto r=geometryRoute();r.erase("bClosedLoop");r["closedRoute"]=true;send({{"action","save_route"},{"edit_session_id",sid},{"path",r},{"keep_editing",true}});auto saved=store.snapshot().at("paths").as_object().begin()->value().as_object();EXPECT_TRUE(saved.at("closedRoute").as_bool());EXPECT_TRUE(saved.at("bClosedLoop").as_bool());r["bClosedLoop"]=false;error({{"action","save_route"},{"edit_session_id",sid},{"path",r}},"INVALID_ROUTE");}

}
