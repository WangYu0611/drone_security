from pathlib import Path
p=Path('Backend/tests/test_security_plans.cpp');s=p.read_text(encoding='utf-8-sig')
s=s.replace('O call(O r) { return store.transact(r,uavs,"QA",[](const O&){}); }','''O call(O r) {
        const auto action=SecurityPlanStore::str(r,"action");r["instance_id"]="qa-map";
        if(!r.contains("expected_version"))r["expected_version"]=store.snapshot().at("version");
        if(action=="save_route") {
            const auto p=SecurityPlanStore::str(r,"plan_id"),m=SecurityPlanStore::str(r,"mission_id");
            auto reply=call({{"action","begin_route_edit"},{"plan_id",p},{"mission_id",m},
                {"content_revision",store.snapshot().at("plans").as_object().at(p).as_object().at("content_revision")}});
            r["edit_session_id"]=reply.at("edit_session_id");r["expected_version"]=store.snapshot().at("version");
        }
        return store.transact(r,uavs,"QA",[](const O&){});
    }''')
s=s.replace('ready(p,m);call({{"action","deploy"}', 'ready(p,m);call({{"action","review"},{"plan_id",p}});call({{"action","deploy"}')
# Invalid route now creates a session before failing; assert unchanged content and retained session explicitly.
s=s.replace('''    EXPECT_EQ(before,store.snapshot());
}
TEST_F(PlanFixture, SelectionAndReadinessInvalidation)''','''    EXPECT_EQ(before.at("plans"),store.snapshot().at("plans"));
    EXPECT_EQ(before.at("paths"),store.snapshot().at("paths"));
    EXPECT_EQ(store.snapshot().at("edit_sessions").as_object().size(),1u);
}
TEST_F(PlanFixture, SelectionAndReadinessInvalidation)''')
p.write_text(s,encoding='utf-8')
