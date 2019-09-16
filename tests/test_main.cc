// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "xpbase/base/top_log.h"
#include "xpbase/base/top_config.h"
#include "xkad/routing_table/routing_utils.h"

namespace top {
    std::shared_ptr<top::base::KadmliaKey> global_xid;
    uint32_t gloabl_platform_type = kPlatform;
    std::string global_node_id = RandomString(256);
    std::string global_node_id_hash("");
}

int main(int argc, char *argv[]) {
    xinit_log("bitvpn_ut.log", true, true);
    xset_log_level(enum_xlog_level_debug);
    top::base::Config config;
    config.Init("./conf.ut/test_routing_table.conf");
    top::kadmlia::CreateGlobalXid(config);

    testing::GTEST_FLAG(output) = "xml:";
    testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    int ret = RUN_ALL_TESTS();
    TOP_INFO("exit");
    return ret;
}
