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
}
