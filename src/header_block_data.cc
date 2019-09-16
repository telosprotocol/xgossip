// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/include/header_block_data.h"

#include "xdata/xdataobject.h"

namespace top {

namespace gossip {

HeaderBlockData::HeaderBlockData(std::shared_ptr<top::ledger::xledger_face_t> ledger_face)
        : ledger_face_(ledger_face) {}

HeaderBlockData::~HeaderBlockData() {}

void HeaderBlockData::AddData(const std::string& header_hash, const std::string& block) {
    assert(ledger_face_);
    xdataobject_string_ptr_t str_obj = make_object_ptr<xdataobject_string_t>();
    str_obj->set(block);
    ledger_face_->set(
            header_hash,
            str_obj,
            0,
            0,
            top::ledger::enum_xledger_set_obj_expire_flag);
}

void HeaderBlockData::GetData(const std::string& header_hash, std::string& block) {
    assert(ledger_face_);
    xdataobject_string_ptr_t str_obj = ledger_face_->get(header_hash);
    if (str_obj == nullptr) {
        return;
    }
    block = str_obj->get();
}

bool HeaderBlockData::HasData(const std::string& header_hash) {
    assert(ledger_face_);
    xdataobject_string_ptr_t str_obj = ledger_face_->get(header_hash);
    if (str_obj == nullptr) {
        return false;
    }
    return true;
}

void HeaderBlockData::RemoveData(const std::string& header_hash) {
    assert(ledger_face_);
    ledger_face_->remove(header_hash);
}

}  // namespace gossip

}  // namespace top
