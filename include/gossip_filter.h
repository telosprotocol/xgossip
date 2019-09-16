// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <unordered_map>
#include <vector>
#include <memory>

#include "xtransport/proto/transport.pb.h"

namespace top {

namespace base {
class TimerRepeated;
}

namespace gossip {

static const uint32_t kRepeatedValue = 1;
static const uint64_t kClearRstPeriod = 15ll * 1000ll * 1000ll; // 5 seconds
typedef std::shared_ptr<std::unordered_map<uint32_t, uint32_t>> HashMapPtr;

class GossipFilter {
public:
    static GossipFilter* Instance();

    bool Init();
    bool FilterMessage(transport::protobuf::RoutingMessage& message);

protected:
    bool AddData(uint32_t);
    bool FindData(uint32_t);
    void do_clear_and_reset();
    void AddRepeatMsg(uint32_t key);
    void PrintRepeatMap();

private:
    GossipFilter();
    ~GossipFilter();

private:
    bool inited_{false};
    std::vector<HashMapPtr> time_filter_;
    std::mutex current_index_mutex_;
    uint32_t current_index_;
    std::shared_ptr<base::TimerRepeated> timer_{nullptr};
    std::mutex repeat_map_mutex_;
    std::map<uint32_t, uint32_t> repeat_map_;
    std::shared_ptr<base::TimerRepeated> remap_timer_{nullptr};
};

}

}
