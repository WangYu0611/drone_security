#include <gtest/gtest.h>
#include "storage/security_plan_store.h"
#include "conversion/mission_geodesy.h"
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

struct P54Mission:P52Execution {
    O executions(){return store.snapshot().at("executions").as_object();}
    void multi(){
        store.configureMock({{"default_speed_mps",5.},{"safety_separation_m",1.5},{"uavs",O{
            {"UAV-01",O{{"home_position",O{{"latitude",39.},{"longitude",115.9999},{"altitude",60.}}}}},
            {"UAV-02",O{{"home_position",O{{"latitude",39.},{"longitude",116.0001},{"altitude",60.}}}}}}}});
        send({{"action","assign"},{"assigned_uav_ids",A{"UAV-01","UAV-02"}},{"spacing_m",4.}});
        begin();auto r=route();r.at("waypoints").as_array()[1].as_object()["longitude"]=116.;
        send({{"action","save_route"},{"edit_session_id",sid},{"path",r}});
        send({{"action","validate"}});ASSERT_EQ(plan().at("status"),"READY");send({{"action","review"}});send({{"action","deploy"}});
    }
};
TEST_F(P54Mission, MultiAssignmentCreatesSeparateImmutableTrajectories){multi();const auto original=store.snapshot().at("paths");start();auto all=store.snapshot().at("executions").as_object();ASSERT_EQ(all.size(),2u);auto a=all.begin()->value().as_object(),b=std::next(all.begin())->value().as_object();EXPECT_NE(a.at("route_snapshot"),b.at("route_snapshot"));EXPECT_EQ(a.at("plan_route_snapshot"),b.at("plan_route_snapshot"));EXPECT_EQ(store.snapshot().at("paths"),original);}
TEST_F(P54Mission, MultiHoverAndCompletionStaySeparated){multi();start();for(int i=0;i<400;++i){tick(1);auto all=store.snapshot().at("executions").as_object();auto a=all.begin()->value().as_object(),b=std::next(all.begin())->value().as_object();const auto pa=a.at("position").as_object(),pb=b.at("position").as_object();const double dx=(pa.at("longitude").to_number<double>()-pb.at("longitude").to_number<double>())*111194.9*std::cos(39.*3.141592653589793/180.);EXPECT_GE(std::abs(dx),1.499);EXPECT_NE(a.at("state"),"FAILED");}for(const auto& e:executions())EXPECT_EQ(e.value().as_object().at("state"),"COMPLETED");}
TEST_F(P54Mission, GroupPauseIsAtomic){multi();start();tick();tick();tick();control("execution_pause","pause-group");for(const auto& e:executions())EXPECT_EQ(e.value().as_object().at("state"),"PAUSED");auto before=store.snapshot();tick();EXPECT_EQ(store.snapshot(),before);control("execution_resume","resume-group");for(const auto& e:executions())EXPECT_EQ(e.value().as_object().at("state"),"EXECUTING");}
TEST_F(P54Mission, DuplicateAndTooCloseAssignmentsRejectAtomically){error({{"action","assign"},{"assigned_uav_ids",A{"UAV-01","UAV-01"}}},"INVALID_UAV");error({{"action","assign"},{"assigned_uav_ids",A{"UAV-01","UAV-02"}},{"spacing_m",.5}},"INVALID_SEPARATION");error({{"action","assign"},{"assigned_uav_ids",A{"UAV-01"}},{"formation","V"}},"FORMATION_UNSUPPORTED");}
TEST_F(P54Mission, IntersectingApproachesBlockStart){multi();store.configureMock({{"default_speed_mps",5.},{"uavs",O{
    {"UAV-01",O{{"home_position",O{{"latitude",39.},{"longitude",116.0001},{"altitude",60.}}}}},
    {"UAV-02",O{{"home_position",O{{"latitude",39.},{"longitude",115.9999},{"altitude",60.}}}}}}}});error({{"action","execution_start"},{"request_id","cross"}},"TRAJECTORY_CONFLICT");}
TEST_F(P54Mission, MissingAltitudeDatumRejected){begin();auto r=route();r.at("waypoints").as_array()[0].as_object()["altitude_reference"]="AGL";error({{"action","save_route"},{"edit_session_id",sid},{"path",r}},"ALTITUDE_DATUM_UNRESOLVED");}
TEST_F(P54Mission, AGLResolvesOnceInRuntimeSnapshot){send({{"action","assign"},{"assigned_uav_id","UAV-01"}});begin();auto r=route();for(auto& v:r.at("waypoints").as_array()){v.as_object()["altitude_reference"]="AGL";v.as_object()["terrain_ellipsoid_m"]=20.;}send({{"action","save_route"},{"edit_session_id",sid},{"path",r}});send({{"action","validate"}});send({{"action","review"}});send({{"action","deploy"}});start();EXPECT_EQ(exec().at("route_snapshot").as_object().at("waypoints").as_array()[0].as_object().at("altitude"),80.);EXPECT_EQ(exec().at("plan_route_snapshot").as_object().at("waypoints").as_array()[0].as_object().at("altitude"),60.);}
TEST_F(P54Mission, SchedulePersistsAndCancelsWithoutMoving){multi();auto reply=send({{"action","execution_start"},{"request_id","scheduled"},{"scheduled_start_at","2099-09-22T13:30:00Z"}});execution=SecurityPlanStore::str(reply,"execution_id");EXPECT_EQ(exec().at("state"),"SCHEDULED");const auto before=store.snapshot();tick(1);EXPECT_EQ(store.snapshot(),before);control("execution_cancel","cancel");for(const auto& e:executions())EXPECT_EQ(e.value().as_object().at("state"),"CANCELLED");}
TEST_F(P54Mission, ScheduleStartNowUsesSameStateMachine){multi();execution=SecurityPlanStore::str(send({{"action","execution_start"},{"request_id","scheduled"},{"scheduled_start_at","2099-09-22T13:30:00Z"}}),"execution_id");control("execution_start_now","now");tick();EXPECT_EQ(exec().at("state"),"PREFLIGHT");tick();tick();EXPECT_EQ(exec().at("state"),"EXECUTING");}
TEST_F(P54Mission, InvalidScheduleDoesNotPartiallyCreateGroup){multi();for(const auto* time:{"2026-02-30T12:00:00Z","2099-09-22T13:30:00+08:00","2000-01-01T00:00:00Z"})error({{"action","execution_start"},{"request_id",time},{"scheduled_start_at",time}},"INVALID_SCHEDULE");}
TEST_F(P54Mission, ScheduleRestartAndReschedule){multi();execution=SecurityPlanStore::str(send({{"action","execution_start"},{"request_id","scheduled"},{"scheduled_start_at","2099-09-22T13:30:00Z"}}),"execution_id");const auto file=std::filesystem::temp_directory_path()/("p54-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");{std::ofstream out(file);out<<boost::json::serialize(store.snapshot());}SecurityPlanStore restored(file.string());EXPECT_EQ(restored.snapshot(),store.snapshot());restored.configureMock(store.snapshot().at("mock_execution").as_object());restored.tickExecutions(1,uavs,[](const O&){});EXPECT_EQ(restored.snapshot().at("executions"),store.snapshot().at("executions"));std::filesystem::remove(file);send({{"action","execution_reschedule"},{"execution_id",execution},{"request_id","reschedule"},{"control_version",exec().at("control_version")},{"scheduled_start_at","2099-09-23T13:30:00Z"}});EXPECT_EQ(exec().at("scheduled_start_at"),"2099-09-23T13:30:00Z");}

TEST_F(P54Mission, GroupMovePreservesRelativePositions){multi();auto response=send({{"action","execution_group_move"},{"request_id","group-move"},{"assigned_uav_ids",A{"UAV-01","UAV-02"}},{"target",O{{"latitude",39.0001},{"longitude",115.9999},{"altitude",80.}}},{"target_area_radius_m",100.}});auto all=executions();ASSERT_EQ(all.size(),2u);const auto a=all.begin()->value().as_object(),b=std::next(all.begin())->value().as_object();const auto pa=a.at("route_snapshot").as_object().at("waypoints").as_array()[1].as_object(),pb=b.at("route_snapshot").as_object().at("waypoints").as_array()[1].as_object();EXPECT_NEAR(std::abs(pa.at("longitude").to_number<double>()-pb.at("longitude").to_number<double>()),.0002,1e-10);for(int i=0;i<100;++i)tick(1);for(const auto& e:executions())EXPECT_EQ(e.value().as_object().at("state"),"COMPLETED");}
TEST_F(P54Mission, InsufficientGroupSpaceRejectsAtomically){multi();error({{"action","execution_group_move"},{"request_id","narrow"},{"assigned_uav_ids",A{"UAV-01","UAV-02"}},{"target",O{{"latitude",39.0001},{"longitude",116.},{"altitude",60.}}},{"target_area_radius_m",.1}},"TRAJECTORY_CONFLICT");}
TEST_F(P54Mission, SharedGroupSelectionIsDurableAndPrimaryIsMember){OperationalContext context("");auto snapshot=context.update({{"active_uav_id","UAV-01"},{"selected_uav_ids",A{"UAV-01","UAV-02"}}},"Command",[](const O&){});EXPECT_EQ(snapshot.at("selected_uav_ids").as_array().size(),2u);EXPECT_THROW(context.update({{"active_uav_id","UAV-03"},{"selected_uav_ids",A{"UAV-01","UAV-02"}}},"Map",[](const O&){}),std::invalid_argument);EXPECT_EQ(context.snapshot(),snapshot);auto single=context.update({{"active_uav_id","UAV-02"}},"Map",[](const O&){});EXPECT_EQ(single.at("selected_uav_ids"),A{"UAV-02"});}

TEST_F(P54Mission, ThreeAircraftCompleteDistinctSlots){
    uavs.insert("UAV-03");O aircraft;
    for(int i=0;i<3;++i)aircraft["UAV-0"+std::to_string(i+1)]=O{{"home_position",O{{"latitude",39.},{"longitude",116.+(i-1)*.0001},{"altitude",60.}}}};
    store.configureMock({{"default_speed_mps",10.},{"uavs",aircraft}});
    send({{"action","assign"},{"assigned_uav_ids",A{"UAV-01","UAV-02","UAV-03"}},{"spacing_m",4.}});begin();auto path=route();path.at("waypoints").as_array()[1].as_object()["longitude"]=116.;
    send({{"action","save_route"},{"edit_session_id",sid},{"path",path}});send({{"action","validate"}});ASSERT_EQ(plan().at("status"),"READY");send({{"action","review"}});send({{"action","deploy"}});start();ASSERT_EQ(executions().size(),3u);
    for(int i=0;i<100;++i)tick(1);std::set<double> longitudes;for(const auto& e:executions()){EXPECT_EQ(e.value().as_object().at("state"),"COMPLETED");longitudes.insert(e.value().as_object().at("position").as_object().at("longitude").to_number<double>());}EXPECT_EQ(longitudes.size(),3u);
}
TEST_F(P54Mission, MissingGroupTargetCoordinateRejectsAtomically){multi();error({{"action","execution_group_move"},{"request_id","missing-target"},{"assigned_uav_ids",A{"UAV-01","UAV-02"}},{"target",O{{"latitude",39.},{"longitude",116.}}}},"INVALID_WAYPOINT");}


struct P54RealAdapter:P52Execution {
    security_mission::Position measured{39.,116.,60.};
    int sent=0,held=0;bool accepts=true,holds=true;
    std::shared_ptr<security_mission::RealAdapter> real(){return std::make_shared<security_mission::RealAdapter>(
        [this](const std::string&){return measured;},[](const std::string&){return security_mission::Position{39.,116.,60.};},
        [this](const security_mission::Command& c){++sent;EXPECT_GT(c.speed_mps,0);return accepts;},[this](const std::string&){++held;return holds;});}
    void live(){deployed();store.configureAdapter(store.snapshot().at("mock_execution").as_object(),real());start();tick();tick();tick();}
};
TEST_F(P54RealAdapter, SameControllerUsesMeasuredArrivalAndHover){
    live();EXPECT_FALSE(exec().at("simulation").as_bool());EXPECT_EQ(exec().at("adapter"),"Real");
    tick(1);EXPECT_EQ(exec().at("completed_waypoints"),1);tick(1);const auto stationary=exec().at("position");
    for(int i=0;i<5;++i)tick(1);EXPECT_EQ(exec().at("position"),stationary);EXPECT_EQ(exec().at("completed_waypoints"),1);EXPECT_EQ(sent,2);
    measured={39.001,116.001,60.};tick(1);EXPECT_EQ(exec().at("phase"),"HOVER");EXPECT_EQ(exec().at("wait_remaining"),2.);
    tick(1);EXPECT_EQ(exec().at("wait_remaining"),1.);tick(1);tick(1);EXPECT_EQ(exec().at("state"),"COMPLETED");EXPECT_EQ(held,1);
}
TEST_F(P54RealAdapter, PauseHoldsAndResumeReissuesThroughSameController){
    live();tick(1);tick(1);const int before=sent;control("execution_pause","real-pause");EXPECT_EQ(held,1);tick(1);EXPECT_EQ(sent,before);
    control("execution_resume","real-resume");tick(1);EXPECT_EQ(sent,before+1);control("execution_abort","real-abort");EXPECT_EQ(exec().at("state"),"ABORTED");EXPECT_EQ(held,2);
}
TEST_F(P54RealAdapter, RejectedTransportCannotFabricateArrival){live();accepts=false;tick(1);EXPECT_EQ(exec().at("state"),"FAILED");EXPECT_EQ(exec().at("failure_reason"),"REAL_COMMAND_REJECTED");EXPECT_EQ(exec().at("completed_waypoints"),0);EXPECT_EQ(held,1);}
TEST_F(P54RealAdapter, RejectedHoldIsReported){live();holds=false;control("execution_abort","real-abort");EXPECT_EQ(exec().at("state"),"FAILED");EXPECT_EQ(exec().at("failure_reason"),"REAL_HOLD_REJECTED");}
TEST_F(P54RealAdapter, RestartPausesRealWithoutDispatch){
    live();tick(1);const auto file=std::filesystem::temp_directory_path()/("p54-real-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    {std::ofstream out(file);out<<boost::json::serialize(store.snapshot());}
    const int before=sent;SecurityPlanStore restored(file.string());restored.configureAdapter(store.snapshot().at("mock_execution").as_object(),real());
    EXPECT_EQ(restored.snapshot().at("executions").as_object().at(execution).as_object().at("state"),"PAUSED");EXPECT_EQ(sent,before);
    SecurityPlanStore second(file.string());EXPECT_EQ(second.snapshot().at("executions").as_object().at(execution).as_object().at("state"),"PAUSED");std::filesystem::remove(file);
}
TEST_F(P54RealAdapter, MockDocumentCannotBecomeRealFlight){
    running();store.configureAdapter(store.snapshot().at("mock_execution").as_object(),real());tick(1);EXPECT_EQ(exec().at("state"),"FAILED");EXPECT_EQ(exec().at("failure_reason"),"ADAPTER_MODE_MISMATCH");EXPECT_EQ(sent,0);EXPECT_EQ(held,0);
}
TEST_F(P54RealAdapter, PublicationFailureKeepsDurableRealIntent){
    deployed();store.configureAdapter(store.snapshot().at("mock_execution").as_object(),real());
    O r{{"action","execution_start"},{"request_id","real-publish-fail"},{"plan_id",p},{"mission_id",m}};
    EXPECT_NO_THROW(store.transact(r,uavs,"QA",[](const O&){throw std::runtime_error("WS disconnected");}));
    EXPECT_EQ(store.snapshot().at("executions").as_object().size(),1u);EXPECT_EQ(sent,0);
}
TEST(P54MissionGeodesy, CalibratedNonzeroNedRoundTrip){
    const security_mission::NedFrame frame{{39.98,116.34,60.},{123.,-45.,-12.}};
    const auto atAnchor=frame.toNed(frame.anchor);EXPECT_NEAR(atAnchor[0],123.,1e-7);EXPECT_NEAR(atAnchor[1],-45.,1e-7);EXPECT_NEAR(atAnchor[2],-12.,1e-7);
    for(double height:{40.,80.,120.,60.}){security_mission::Position target{39.981,116.341,height};const auto result=frame.fromNed(frame.toNed(target));EXPECT_NEAR(result.latitude,target.latitude,1e-9);EXPECT_NEAR(result.longitude,target.longitude,1e-9);EXPECT_NEAR(result.altitude,height,1e-5);}
    auto higher=frame.anchor;higher.altitude+=10.;EXPECT_NEAR(frame.toNed(higher)[2],-22.,1e-6);
}
TEST(P54MissionGeodesy, NonfiniteAndDistantFramesReject){const security_mission::NedFrame frame{{39.,116.,60.},{0,0,0}};EXPECT_THROW(frame.toNed({40.,116.,60.}),std::runtime_error);
EXPECT_THROW(frame.fromNed({NAN,0,0}),std::runtime_error);}

TEST_F(P54Mission, ScheduledDeadlineSurvivesGracefulShutdown){multi();execution=SecurityPlanStore::str(send({{"action","execution_start"},{"request_id","scheduled-shutdown"},{"scheduled_start_at","2099-09-22T13:30:00Z"}}),"execution_id");send({{"action","execution_shutdown"},{"request_id","stop-host"}});for(const auto& e:executions())EXPECT_EQ(e.value().as_object().at("state"),"SCHEDULED");}
TEST_F(P54Mission, RealGroupFailureHoldsRemainingMembers){
    multi();const auto cfg=store.snapshot().at("mock_execution").as_object();std::set<std::string> held;
    auto observe=[cfg](const std::string& id){const auto& p=cfg.at("uavs").as_object().at(id).as_object().at("home_position").as_object();return security_mission::Position{p.at("latitude").to_number<double>(),p.at("longitude").to_number<double>(),p.at("altitude").to_number<double>()};};
    auto adapter=std::make_shared<security_mission::RealAdapter>(observe,observe,[](const security_mission::Command& c){return c.uav_id!="UAV-01";},[&held](const std::string& id){held.insert(id);return true;});
    store.configureAdapter(cfg,adapter);start();tick();tick();tick();tick(1);
    for(const auto& e:executions()){const auto& x=e.value().as_object();EXPECT_EQ(x.at("state"),"FAILED");}EXPECT_EQ(held.size(),2u);
}
TEST_F(P54Mission, RealMeasuredCrossingHoldsBothMembers){
    multi();const auto cfg=store.snapshot().at("mock_execution").as_object();std::map<std::string,security_mission::Position> measured;std::set<std::string> held;
    for(const auto& u:cfg.at("uavs").as_object()){const auto& p=u.value().as_object().at("home_position").as_object();measured[std::string(u.key())]={p.at("latitude").to_number<double>(),p.at("longitude").to_number<double>(),p.at("altitude").to_number<double>()};}
    auto observe=[&](const std::string& id){return measured.at(id);};auto homes=measured;
    auto adapter=std::make_shared<security_mission::RealAdapter>(observe,[homes](const std::string& id){return homes.at(id);},[](const security_mission::Command&){return true;},[&held](const std::string& id){held.insert(id);return true;});
    store.configureAdapter(cfg,adapter);start();tick();tick();tick();std::swap(measured["UAV-01"],measured["UAV-02"]);tick(1);
    for(const auto& e:executions())EXPECT_EQ(e.value().as_object().at("state"),"PAUSED");EXPECT_EQ(held.size(),2u);
}

TEST_F(P54Mission, RealFaultWhilePausedReleasesWholeGroup){
    multi();const auto cfg=store.snapshot().at("mock_execution").as_object();bool unavailable=false;
    auto position=[cfg](const std::string& id){const auto& p=cfg.at("uavs").as_object().at(id).as_object().at("home_position").as_object();return security_mission::Position{p.at("latitude").to_number<double>(),p.at("longitude").to_number<double>(),p.at("altitude").to_number<double>()};};
    auto observe=[&](const std::string& id){if(unavailable && id=="UAV-01")throw std::runtime_error("REAL_TELEMETRY_UNAVAILABLE");return position(id);};
    store.configureAdapter(cfg,std::make_shared<security_mission::RealAdapter>(observe,position,[](const security_mission::Command&){return true;},[](const std::string&){return true;}));
    start();tick();tick();tick();control("execution_pause","pause-before-fault");unavailable=true;tick(1);
    for(const auto& e:executions())EXPECT_EQ(e.value().as_object().at("state"),"FAILED");
    unavailable=false;EXPECT_NO_THROW(start("new-after-failure"));EXPECT_EQ(exec().at("state"),"CREATED");
}
TEST_F(P54RealAdapter, FailedPreflightDoesNotSendOrHold){deployed();store.configureAdapter(store.snapshot().at("mock_execution").as_object(),real());start();measured.latitude=NAN;tick();EXPECT_EQ(exec().at("state"),"FAILED");EXPECT_EQ(sent,0);EXPECT_EQ(held,0);}

TEST_F(P54RealAdapter, ScheduledObservationLossFailsAtomically){
    deployed();const auto cfg=store.snapshot().at("mock_execution").as_object();store.configureAdapter(cfg,real());
    execution=SecurityPlanStore::str(send({{"action","execution_start"},{"request_id","schedule-observation-loss"},{"scheduled_start_at","2099-09-22T13:30:00Z"}}),"execution_id");
    auto state=store.snapshot();state.at("executions").as_object().at(execution).as_object()["scheduled_start_epoch"]=0.;
    const auto file=std::filesystem::temp_directory_path()/("p54-schedule-observation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    {std::ofstream out(file);out<<boost::json::serialize(state);}
    int observations=0;auto adapter=std::make_shared<security_mission::RealAdapter>([&](const std::string&){if(++observations==2)throw std::runtime_error("REAL_TELEMETRY_UNAVAILABLE");return measured;},[&](const std::string&){return measured;},[&](const security_mission::Command&){++sent;return true;},[&](const std::string&){++held;return true;});
    SecurityPlanStore restored(file.string());restored.configureAdapter(cfg,adapter);
    EXPECT_NO_THROW(restored.tickExecutions(.1,uavs,[](const O&){}));
    EXPECT_EQ(restored.snapshot().at("executions").as_object().at(execution).as_object().at("state"),"FAILED");EXPECT_EQ(sent,0);EXPECT_EQ(held,0);std::filesystem::remove(file);
}

}
