#pragma once
#include "storage/security_plan_store.h"

// Independent persisted context domains, sharing transport but never Plan revisions.
class SharedViewStore {
    using O=boost::json::object;
public:
    SharedViewStore(std::string path,bool language):path_(std::move(path)),language_(language) {
        key_=language?"language":"video_target_uav_id";
        version_=language?"ui_preferences_version":"video_view_version";
        event_=language?"UIPreferencesChanged":"VideoViewChanged";
        state_={{key_,language?boost::json::value("en"):boost::json::value(nullptr)},
            {version_,0},{"updated_at",0},{"updated_by","Backend"}};
        if(!path_.empty() && std::filesystem::exists(path_)) {
            std::ifstream in(path_);std::string text((std::istreambuf_iterator<char>(in)),{});
            state_=boost::json::parse(text).as_object();
            mock_receipts_.load(state_);
        }
    }
    O snapshot() const {std::lock_guard<std::mutex> l(mutex_);return state_;}
    O update(const O& body,const std::set<std::string>& uavs,const std::string& source,const std::function<void(const O&)>& publish,
             const MockViewReceipts::Write* mock=nullptr) {
        std::lock_guard<std::mutex> l(mutex_);
        if(mock) {
            if(const auto* receipt=mock_receipts_.lookup(*mock))return *receipt;
            if(mock->arguments.at("expected_version")!=state_.at(version_))throw MockViewError("VERSION_CONFLICT");
            if(mock->validate)mock->validate();
        }
        const auto* value=body.if_contains(key_);
        if(language_) {
            if(!value || !value->is_string() || (*value!="en" && *value!="zh-Hans"))throw PlanError("INVALID_LANGUAGE","Supported languages: en, zh-Hans");
        } else if(!value || (!value->is_null() && (!value->is_string() || !uavs.count(std::string(value->as_string())))))
            throw PlanError("INVALID_VIDEO_TARGET","Target must be null or a registered canonical UAV ID");
        const boost::json::value null_source;
        const auto* previous_source=state_.if_contains("mock_source_id");
        const auto* requested_source=mock && !language_?body.if_contains("mock_source_id"):nullptr;
        const auto& next_source=requested_source?*requested_source:null_source;
        auto next=state_;
        const bool same_source=(previous_source?*previous_source:null_source)==next_source;
        const auto store=[this](const O& stored){persist(stored);};
        if(*value==state_.at(key_) && (language_ || same_source))return mock?mock_receipts_.commit(state_,mock,store):state_;
        next[key_]=*value;
        if(!language_) {
            if(next_source.is_null()) next.erase("mock_source_id");
            else next["mock_source_id"]=next_source;
        }
        next[version_]=state_.at(version_).to_number<int64_t>()+1;
        next["updated_at"]=OperationalContext::now();next["updated_by"]=source;
        auto result=mock_receipts_.commit(next,mock,store);
        state_=next;publish({{"type",event_},{"payload",state_}});return result;
    }
private:
    void persist(const O& value) {
        if(!path_.empty()) {
            auto path=std::filesystem::path(path_);if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
            auto temp=path_+".tmp";
            {std::ofstream out(temp);out<<boost::json::serialize(value);out.flush();if(!out)throw std::runtime_error("view persistence failed");}
#ifdef _WIN32
            if(!MoveFileExA(temp.c_str(),path_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("view persistence replace failed");
#else
            std::filesystem::rename(temp,path_);
#endif
        }
    }
    std::string path_,key_,version_,event_;bool language_;mutable std::mutex mutex_;O state_;
    MockViewReceipts mock_receipts_;
};
class UIPreferenceStore:public SharedViewStore {public:explicit UIPreferenceStore(std::string p):SharedViewStore(std::move(p),true){}};
class VideoViewStore:public SharedViewStore {public:explicit VideoViewStore(std::string p):SharedViewStore(std::move(p),false){}};
