// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/include/gossip_filter.h"

#include <cassert>

#include "xpbase/base/top_log.h"
#include "xpbase/base/top_timer.h"
#include "xpbase/base/kad_key/kadmlia_key.h"

namespace top {

namespace gossip {

GossipFilter* GossipFilter::Instance() {
    static GossipFilter ins;


    return &ins;
}

bool GossipFilter::Init() {
    assert(!inited_);
    for (uint32_t i = 0; i < 3; ++i) {
        auto hash_ptr = std::make_shared<std::unordered_map<uint32_t, uint32_t>>();
        time_filter_.push_back(hash_ptr);
    }
    timer_ = std::make_shared<base::TimerRepeated>(base::TimerManager::Instance(), "GossipFilter");
    timer_->Start(
            500ll * 1000ll,
            kClearRstPeriod,
            std::bind(&GossipFilter::do_clear_and_reset, this));

#ifndef NDEBUG
    remap_timer_ = std::make_shared<base::TimerRepeated>(base::TimerManager::Instance(), "GossipFilter:repeatmap");
    remap_timer_->Start(
            500ll * 1000ll,
            kClearRstPeriod,
            std::bind(&GossipFilter::PrintRepeatMap, this));
#endif

    inited_ = true;
    return true;
}

GossipFilter::GossipFilter() {
}

GossipFilter::~GossipFilter() {
    timer_->Join();
    remap_timer_->Join();
    timer_ = nullptr;
    remap_timer_ = nullptr;
}

void GossipFilter::AddRepeatMsg(uint32_t key) {
#ifndef NDEBUG
    std::unique_lock<std::mutex> lock(repeat_map_mutex_);
    repeat_map_[key] += 1;
#endif
}

void GossipFilter::PrintRepeatMap() {
#ifndef NDEBUG
    {
        std::unique_lock<std::mutex> lock(repeat_map_mutex_);
        if (repeat_map_.size() <= 0) {
            return;
        }
        uint32_t total_recv_count = 0;
        uint32_t max_recv = 0;
        for (auto& item : repeat_map_) {
            auto key = item.first;
            auto value = item.second;
            total_recv_count += value;
            if (value > max_recv) {
                max_recv = value;
            }
        }
        auto avg_recv_times = total_recv_count / repeat_map_.size();
        TOP_DEBUG("gossipfilter repeat recv avg:%d max:%d mapsize:%d", avg_recv_times, max_recv, repeat_map_.size());

        // do clear
        if (repeat_map_.size() > 100000) {
            repeat_map_.clear();
        }
    }
#endif
}

bool GossipFilter::FilterMessage(transport::protobuf::RoutingMessage& message) {
    assert(inited_);
    auto gossip = message.gossip();
    if (!gossip.has_msg_hash()) {
        TOP_WARN("filter failed, gossip msg(%d) should set msg_hash", message.type());
        return true;;
    }
#ifndef NDEBUG
    AddRepeatMsg(gossip.msg_hash());
#endif
    if (message.xid() == global_xid->Get()) {
        TOP_WARN("message come back this original node,msg.type(%d)", message.type());
        return true;
    }
    if (FindData(gossip.msg_hash())) {
        TOP_DEBUG("GossipFilter FindData, filter msg");
        return true;
    }
    if (!AddData(gossip.msg_hash())) {
        TOP_WARN("GossipFilter already exist, filter msg");
        return true;
    }
    return false;
}

bool GossipFilter::FindData(uint32_t key) {
    assert(current_index_ < 3 && current_index_ >= 0);
    std::unique_lock<std::mutex> lock(current_index_mutex_);
    auto it_find = time_filter_[current_index_]->find(key);
    if (it_find != time_filter_[current_index_]->end()) {
        return it_find->second >= kRepeatedValue;
    }
    // last available index
    uint32_t last_index = (current_index_ + 2) % 3;
    auto last_it_find = time_filter_[last_index]->find(key);
    if (last_it_find != time_filter_[last_index]->end()) {
        return last_it_find->second >= kRepeatedValue;
    }
    return false;
}

bool GossipFilter::AddData(uint32_t key) {
    assert(current_index_ < 3 && current_index_ >= 0);
    std::unique_lock<std::mutex> lock(current_index_mutex_);
    auto it_find = time_filter_[current_index_]->find(key);
    if (it_find != time_filter_[current_index_]->end()) {
        if (it_find->second >= kRepeatedValue) {
            return false;
        }
        ++it_find->second;
        return true;
    }

    // last available index
    uint32_t last_index = (current_index_ + 2) % 3;
    auto last_it_find = time_filter_[last_index]->find(key);
    if (last_it_find != time_filter_[last_index]->end()) {
        if (last_it_find->second >= kRepeatedValue) {
            return false;
        }
        ++last_it_find->second;
        return true;
    }

    time_filter_[current_index_]->insert(std::make_pair(key, 1));
    return true;
}

void GossipFilter::do_clear_and_reset() {
    {
        assert(current_index_ < 3 && current_index_ >= 0);
        std::unique_lock<std::mutex> lock(current_index_mutex_);
        uint32_t not_used_index = (current_index_ + 1) % 3;
        time_filter_[not_used_index]->clear();
        current_index_ = not_used_index;
    }
}

} // end namespace gossip

} // end namespace top
