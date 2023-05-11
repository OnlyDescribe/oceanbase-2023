/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#define USING_LOG_PREFIX SHARE_PT

#include "share/ls/ob_ls_info.h"      // for decalrations of functions in this cpp
#include "share/config/ob_server_config.h"            // for KR(), common::ob_error_name(x)
#include "share/ls/ob_ls_replica_filter.h" // ObLSReplicaFilter
#include "share/ob_share_util.h"           // ObShareUtils
#include "lib/string/ob_sql_string.h"      // ObSqlString

namespace oceanbase
{
namespace share
{
using namespace common;

static const char *replica_display_status_strs[] = {
  "NORMAL",
  "OFFLINE",
  "FLAG",
  "UNMERGED",
};

const char *ob_replica_status_str(const ObReplicaStatus status)
{
  STATIC_ASSERT(ARRAYSIZEOF(replica_display_status_strs) == REPLICA_STATUS_MAX,
                "type string array size mismatch with enum replica status count");
  const char *str = NULL;
  if (status >= 0 && status < REPLICA_STATUS_MAX) {
    str = replica_display_status_strs[status];
  } else {
    LOG_WARN_RET(OB_INVALID_ERROR, "invalid replica status", K(status));
  }
  return str;
}

int get_replica_status(const ObString &status_str, ObReplicaStatus &status)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(status_str.empty())) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), K(status_str));
  } else {
    status = REPLICA_STATUS_MAX;
    for (int64_t i = 0; i < ARRAYSIZEOF(replica_display_status_strs); ++i) {
      if (0 == status_str.case_compare(replica_display_status_strs[i])) {
        status = static_cast<ObReplicaStatus>(i);
        break;
      }
    }
    if (REPLICA_STATUS_MAX == status) {
      ret = OB_ENTRY_NOT_EXIST;
      LOG_WARN("display status str not found", KR(ret), K(status_str));
    }
  }
  return ret;
}

int get_replica_status(const char* str, ObReplicaStatus &status)
{
  int ret = OB_SUCCESS;
  if (NULL == str) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), KP(str));
  } else {
    status = REPLICA_STATUS_MAX;
    for (int64_t i = 0; i < ARRAYSIZEOF(replica_display_status_strs); ++i) {
      if (STRCASECMP(replica_display_status_strs[i], str) == 0) {
        status = static_cast<ObReplicaStatus>(i);
        break;
      }
    }
    if (REPLICA_STATUS_MAX == status) {
      ret = OB_ENTRY_NOT_EXIST;
      LOG_WARN("display status str not found", KR(ret), K(str));
    }
  }
  return ret;
}

int SimpleMember::init(int64_t timestamp, char *member_text)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(server_.parse_from_cstring(member_text))) {
    LOG_ERROR("server parse_from_cstring failed", K(member_text), KR(ret));
  } else {
    timestamp_ = timestamp;
  }
  return ret;
}

OB_SERIALIZE_MEMBER(SimpleMember, server_, timestamp_);

ObLSReplica::ObLSReplica()
  : create_time_us_(0),
    modify_time_us_(0),
    tenant_id_(OB_INVALID_TENANT_ID),
    ls_id_(),
    server_(),
    sql_port_(OB_INVALID_INDEX),
    role_(FOLLOWER),
    member_list_(),
    replica_type_(REPLICA_TYPE_FULL),
    proposal_id_(0),
    replica_status_(REPLICA_STATUS_NORMAL),
    restore_status_(),
    property_(),
    unit_id_(OB_INVALID_ID),
    zone_(),
    paxos_replica_number_(OB_INVALID_COUNT),
    data_size_(0),
    required_size_(0),
    in_member_list_(false),
    member_time_us_(0),
    learner_list_(),
    in_learner_list_(false)
{
}

ObLSReplica::~ObLSReplica()
{
}

void ObLSReplica::reset()
{
  create_time_us_ = 0;
  modify_time_us_ = 0;
  //location related
  tenant_id_ = OB_INVALID_TENANT_ID;
  ls_id_.reset();
  server_.reset();
  sql_port_ = OB_INVALID_INDEX;
  role_ = FOLLOWER;
  member_list_.reset();
  replica_type_ = REPLICA_TYPE_FULL;
  proposal_id_ = 0;
  replica_status_ = REPLICA_STATUS_NORMAL;
  restore_status_ = ObLSRestoreStatus::Status::RESTORE_NONE;
  property_.reset();
  //meta related
  unit_id_ = OB_INVALID_ID;
  zone_.reset();
  paxos_replica_number_ = OB_INVALID_COUNT;
  data_size_ = 0;
  required_size_ = 0;
  in_member_list_ = false;
  member_time_us_ = 0;
  learner_list_.reset();
  in_learner_list_ = false;
}

int ObLSReplica::init(
    const int64_t create_time_us,
    const int64_t modify_time_us,
    const uint64_t tenant_id,
    const ObLSID &ls_id,
    const common::ObAddr &server,
    const int64_t sql_port,
    const common::ObRole &role,
    const common::ObReplicaType &replica_type,
    const int64_t proposal_id,
    const ObReplicaStatus &replica_status,
    const ObLSRestoreStatus &restore_status,
    const int64_t memstore_percent,
    const uint64_t unit_id,
    const ObString &zone,
    const int64_t paxos_replica_number,
    const int64_t data_size,
    const int64_t required_size,
    const MemberList &member_list,
    const GlobalLearnerList &learner_list)
{
  int ret = OB_SUCCESS;
  reset();
  if (!ls_id.is_valid_with_tenant(tenant_id)
      || OB_ISNULL(zone)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("init a ObLSReplica with invalid argument", KR(ret), K(tenant_id), K(ls_id), K(zone));
  } else if (OB_FAIL(property_.set_memstore_percent(memstore_percent))) {
    LOG_WARN("fail to assign memstore_percent", KR(ret), K(memstore_percent));
  } else if (OB_FAIL(zone_.assign(zone))) {
    LOG_WARN("fail to assign zone", KR(ret), K(zone));
  } else if (OB_FAIL(member_list_.assign(member_list))) {
    LOG_WARN("failed to assign member list", KR(ret), K(member_list));
  } else if (OB_FAIL(learner_list_.deep_copy(learner_list))) {
    LOG_WARN("failed to deep copy learner list", KR(ret), K(learner_list));
  } else {
    create_time_us_ = create_time_us;
    modify_time_us_ = modify_time_us;
    tenant_id_ = tenant_id;
    ls_id_ = ls_id;
    server_ = server;
    sql_port_ = sql_port;
    role_ = role;
    replica_type_ = replica_type;
    proposal_id_ = proposal_id;
    replica_status_ = replica_status;
    restore_status_ = restore_status;
    unit_id_ = unit_id;
    paxos_replica_number_ = paxos_replica_number;
    data_size_ = data_size;
    required_size_ = required_size;
  }
  return ret;
}

int ObLSReplica::assign(const ObLSReplica &other)
{
  int ret = OB_SUCCESS;
  if (this != &other) {
    reset();
    if (OB_FAIL(copy_assign(member_list_, other.member_list_))) {
      LOG_WARN("failed to assign member_list_", KR(ret));
    } else if (OB_FAIL(copy_assign(learner_list_, other.learner_list_))) {
      LOG_WARN("failed to assign learner_list_", KR(ret));
    } else if (OB_FAIL(copy_assign(property_, other.property_))) {
      LOG_WARN("fail to assign property", KR(ret));
    } else if (OB_FAIL(zone_.assign(other.zone_))) {
      LOG_WARN("fail to assign zone", KR(ret));
    } else {
      create_time_us_ = other.create_time_us_;
      modify_time_us_ = other.modify_time_us_;
      // location-related
      tenant_id_ = other.tenant_id_;
      ls_id_ = other.ls_id_;
      server_ = other.server_;
      sql_port_ = other.sql_port_;
      role_ = other.role_;
      replica_type_ = other.replica_type_;
      proposal_id_ = other.proposal_id_;
      replica_status_ = other.replica_status_;
      restore_status_ = other.restore_status_;
      // meta-related
      unit_id_ = other.unit_id_;
      paxos_replica_number_ = other.paxos_replica_number_;
      data_size_ = other.data_size_;
      required_size_ = other.required_size_;
      in_member_list_ = other.in_member_list_;
      member_time_us_ = other.member_time_us_;
      in_learner_list_ = other.in_learner_list_;
    }
  }
  return ret;
}

bool ObLSReplica::is_equal_for_report(const ObLSReplica &other) const
{
  bool is_equal = false;
  if (this == &other) {
    is_equal = true;
  } else if (tenant_id_ == other.tenant_id_
      && ls_id_ == other.ls_id_
      && server_ == other.server_
      && sql_port_ == other.sql_port_
      && role_ == other.role_
      && member_list_is_equal(member_list_, other.member_list_)
      && replica_status_ == other.replica_status_
      && restore_status_ == other.restore_status_
      && property_ == other.property_
      && unit_id_ == other.unit_id_
      && zone_ == other.zone_
      && paxos_replica_number_ == other.paxos_replica_number_) {
    is_equal = true;
  }

  // only proposal_id of leader is meaningful
  // proposal_id of follower will be set to 0 in reporting process
  if (is_equal && ObRole::LEADER == role_) {
    is_equal = (proposal_id_ == other.proposal_id_);
  }

  // check replica_type and learner_list if necessary
  bool is_compatible_with_readonly_replica = false;
  int ret = OB_SUCCESS;
  if (is_equal && OB_FAIL(ObShareUtil::check_compat_version_for_readonly_replica(
                                           tenant_id_,
                                           is_compatible_with_readonly_replica))) {
    LOG_WARN("failed to check compat version for readonly replica", KR(ret), K_(tenant_id));
  } else if (is_equal && is_compatible_with_readonly_replica) {
    is_equal = learner_list_is_equal(learner_list_, other.learner_list_)
               && replica_type_ == other.replica_type_;
  }

  return is_equal;
}

bool ObLSReplica::learner_list_is_equal(const common::GlobalLearnerList &a, const common::GlobalLearnerList &b) const
{
  bool is_equal = true;
  if (a.get_member_number() != b.get_member_number()) {
    is_equal = false;
  } else {
    for (int i = 0; is_equal && i < a.get_member_number(); ++i) {
      ObAddr learner;
      int ret = OB_SUCCESS;
      if (OB_FAIL(a.get_server_by_index(i, learner))) {
        is_equal = false;
        LOG_WARN("failed to get server by index", KR(ret), K(i), K(a), K(b));
      } else {
        is_equal = b.contains(learner);
      }
    }
  }
  return is_equal;
}

bool ObLSReplica::member_list_is_equal(const MemberList &a, const MemberList &b) const
{
  bool is_equal = true;
  if (a.count() != b.count()) {
    is_equal = false;
  } else {
    for (int i = 0; is_equal && i < a.count(); ++i) {
      is_equal = false;
      for (int j = 0; j < b.count(); ++j) {
        if (a[i] == b[j]) {
          is_equal = true;
          break;
        }
      }
    }
  }
  return is_equal;
}

int64_t ObLSReplica::to_string(char *buf, const int64_t buf_len) const
{
  int64_t pos = 0;
  J_OBJ_START();
  J_KV(
      K_(modify_time_us),
      K_(create_time_us),
      // location related
      K_(tenant_id),
      K_(ls_id),
      K_(server),
      K_(sql_port),
      K_(role),
      K_(member_list),
      K_(replica_type),
      K_(proposal_id),
      "replica_status", ob_replica_status_str(replica_status_),
      K_(restore_status),
      K_(property),
      // meta related
      K_(unit_id),
      K_(zone),
      K_(paxos_replica_number),
      K_(data_size),
      K_(required_size),
      K_(in_member_list),
      K_(member_time_us),
      K_(learner_list),
      K_(in_learner_list));
  J_OBJ_END();
  return pos;
}

OB_SERIALIZE_MEMBER(ObLSReplica,
                    create_time_us_,
                    modify_time_us_,
                    // location related
                    tenant_id_,
                    ls_id_,
                    server_,
                    sql_port_,
                    role_,
                    member_list_,
                    replica_type_,
                    proposal_id_,
                    replica_status_,
                    restore_status_,
                    property_,
                    // meta related
                    unit_id_,
                    zone_,
                    paxos_replica_number_,
                    data_size_,
                    required_size_,
                    in_member_list_,
                    member_time_us_,
                    learner_list_,
                    in_learner_list_);

int ObLSReplica::member_list2text(
    const MemberList &member_list,
    ObSqlString &text)
{
  int ret = OB_SUCCESS;
  text.reset();
  if (0 > member_list.count()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "member count", member_list.count());
  } else if (0 == member_list.count()) {
    text.reset();
  } else {
    bool need_comma = false;
    char ip_port[MAX_IP_PORT_LENGTH];
    FOREACH_CNT_X(m, member_list, OB_SUCC(ret)) {
      if (OB_FAIL(m->get_server().ip_port_to_string(ip_port, sizeof(ip_port)))) {
        LOG_WARN("convert server to string failed", KR(ret), "member", *m);
      } else if (need_comma && OB_FAIL(text.append(","))) {
        LOG_WARN("failed to append comma to string", KR(ret));
      } else if (OB_FAIL(text.append_fmt("%.*s:%ld", static_cast<int>(sizeof(ip_port)), ip_port, m->get_timestamp()))) {
        LOG_WARN("failed to append ip_port to string", KR(ret), "member", *m);
      } else {
        need_comma = true;
      }
    }
  }
  return ret;
}

int ObLSReplica::text2learner_list(const char *text, GlobalLearnerList &learner_list)
{
  int ret = OB_SUCCESS;
  char *learner_text = nullptr;
  char *save_ptr1 = nullptr;
  learner_list.reset();
  if (nullptr == text) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), KP(text));
  }
  while (OB_SUCC(ret)) {
    learner_text = strtok_r((nullptr == learner_text ? const_cast<char *>(text) : nullptr), ",", &save_ptr1);
    /*
     * ipv4 format: a.b.c.d:port:timestamp,...
     * ipv6 format: [a:b:c:d:e:f:g:h]:port:timestamp,...
     */
    if (nullptr != learner_text) {
      char *timestamp_str = nullptr;
      char *end_ptr = nullptr;
      ObAddr learner_addr;
      if (OB_NOT_NULL(timestamp_str = strrchr(learner_text, ':'))) {
        *timestamp_str++ = '\0';
        int64_t timestamp_val = strtoll(timestamp_str, &end_ptr, 10);
        if (end_ptr == timestamp_str || *end_ptr != '\0') {
          ret = OB_ERR_UNEXPECTED;
          LOG_WARN("strtoll failed", KR(ret));
        } else if (OB_FAIL(learner_addr.parse_from_cstring(learner_text))) {
          LOG_ERROR("parse from cstring failed", KR(ret), K(learner_text));
        } else if (OB_FAIL(learner_list.add_learner(ObMember(learner_addr, timestamp_val)))) {
          LOG_WARN("push back failed", KR(ret), K(learner_addr), K(timestamp_val));
        }
      } else {
        ret = OB_ERR_UNEXPECTED;
        LOG_ERROR("parse learner text failed", KR(ret), K(learner_text));
      }
    } else {
      break;
    }
  } // while
  return ret;
}

int ObLSReplica::text2member_list(const char *text, MemberList &member_list)
{
  int ret = OB_SUCCESS;
  char *member_text = nullptr;
  char *save_ptr1 = nullptr;
  member_list.reset();
  if (nullptr == text) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), KP(text));
  }
  while (OB_SUCC(ret)) {
    SimpleMember member;
    member_text = strtok_r((nullptr == member_text ? const_cast<char *>(text) : nullptr), ",", &save_ptr1);
    /*
     * ipv4 format: a.b.c.d:port:timestamp,...
     * ipv6 format: [a:b:c:d:e:f:g:h]:port:timestamp,...
     */
    if (nullptr != member_text) {
      char *timestamp_str = nullptr;
      char *end_ptr = nullptr;
      if (OB_NOT_NULL(timestamp_str = strrchr(member_text, ':'))) {
        *timestamp_str++ = '\0';
        int64_t timestamp_val = strtoll(timestamp_str, &end_ptr, 10);
        if (end_ptr == timestamp_str || *end_ptr != '\0') {
          ret = OB_ERR_UNEXPECTED;
          LOG_WARN("strtoll failed", KR(ret));
        } else if (OB_FAIL(member.init(timestamp_val, member_text))) {
          LOG_ERROR("server parse_from_cstring failed", K(member_text), KR(ret));
        } else if (OB_FAIL(member_list.push_back(member))) {
          LOG_WARN("push back failed", KR(ret), K(member));
        }
      } else {
        ret = OB_ERR_UNEXPECTED;
        LOG_ERROR("parse member text failed", K(member_text), KR(ret));
      }
    } else {
      break;
    }
  } // while
  return ret;
}

int ObLSReplica::transform_ob_member_list(
    const common::ObMemberList &ob_member_list,
    MemberList &member_list)
{
  int ret = OB_SUCCESS;
  member_list.reset();
  common::ObMember member;
  for (int64_t i = 0; OB_SUCC(ret) && i < ob_member_list.get_member_number(); ++i) {
    member.reset();
    if (OB_FAIL(ob_member_list.get_member_by_index(i, member))) {
      SERVER_LOG(WARN, "get_member_by_index failed", KR(ret), K(i));
    } else if (OB_UNLIKELY(!member.is_valid())) {
      ret = OB_INVALID_ARGUMENT;
      SERVER_LOG(WARN, "member is invalid", KR(ret), K(member));
    } else if (OB_FAIL(member_list.push_back(share::SimpleMember(
        member.get_server(),
        member.get_timestamp())))) {
      SERVER_LOG(WARN, "push_back failed", KR(ret), K(member));
    }
  }
  return ret;
}

ObLSInfo::ObLSInfo()
  : tenant_id_(OB_INVALID_TENANT_ID),
    ls_id_(),
    replicas_()
{
}

ObLSInfo::ObLSInfo(
    const uint64_t tenant_id,
    const ObLSID &ls_id)
    : tenant_id_(tenant_id),
      ls_id_(ls_id),
      replicas_()
{
}

ObLSInfo::~ObLSInfo()
{
}

void ObLSInfo::reset()
{
  tenant_id_ = OB_INVALID_TENANT_ID;
  ls_id_.reset();
  replicas_.reset();
}

OB_SERIALIZE_MEMBER(ObLSInfo, tenant_id_, ls_id_, replicas_);

// find a certain replica according to server
int ObLSInfo::find(const common::ObAddr &server, const ObLSReplica *&replica) const
{
  replica = NULL;
  int ret = OB_SUCCESS;
  int64_t idx = OB_INVALID_INDEX;
  if (!is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "log stream", *this, K(server));
  } else if (OB_FAIL(find_idx_(server, idx))) {
    if (OB_ENTRY_NOT_EXIST != ret) {
      LOG_WARN("find index failed", KR(ret), K(server));
    }
  } else if (idx < 0 || idx >= replicas_.count()) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("invalid index", KR(ret), K(idx), "replica_count", replicas_.count());
  } else {
    replica = &replicas_.at(idx);
  }
  return ret;
}

// check a certain replica is leader or not by replica's index
bool ObLSInfo::is_strong_leader(int64_t index) const
{
  bool is_leader = true;
  if (!replicas_.at(index).is_strong_leader()) {
    is_leader = false;
  } else {
    FOREACH_CNT(r, replicas_) {
      if (OB_ISNULL(r)) {
        LOG_WARN_RET(OB_ERR_UNEXPECTED, "get invalie replica", K_(replicas), K(r));
      } else if (r->get_proposal_id() > replicas_.at(index).get_proposal_id()) {
        is_leader = false;
        break;
      }
    }
  }
  return is_leader;
}

// Find the leader replica in ObLSInfo's replicas_
// The leader replica must:
//    (1) role_ setted to LEADER
//    (2) has max proposal_id among replicas recorded in this ObLSInfo
int ObLSInfo::find_leader(const ObLSReplica *&replica) const
{
  int ret = OB_SUCCESS;
  replica = NULL;
  if (!is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "partition", *this);
  } else {
    int64_t laster_proposal_id = 0;
    int64_t index = 0;
    int64_t find_index = -1;
    FOREACH_CNT_X(r, replicas_, OB_SUCC(ret)) {
      if (r->get_proposal_id() > laster_proposal_id) {
        find_index = index;
        laster_proposal_id = r->get_proposal_id();
      }
      index ++;
    }
    if (find_index != -1) {
      if (!replicas_.at(find_index).is_strong_leader()) {
        ret = OB_ENTRY_NOT_EXIST;
        LOG_WARN("fail to get leader replica", KR(ret), K(find_index), "role", replicas_.at(find_index).get_role(),
                 "proposal_id_", replicas_.at(find_index).get_proposal_id(), K(*this));
      } else {
        replica = &replicas_.at(find_index);
      }
    } else {
      ret = OB_ENTRY_NOT_EXIST;
      LOG_WARN("fail to get leader replica", KR(ret), K(*this), "replica count", replicas_.count());
    }
  }
  return ret;
}

// Find index of a certain replica in ObLSInfo's replicas_ by their address.
int ObLSInfo::find_idx_(const ObLSReplica &replica, int64_t &idx) const
{
  int ret = OB_SUCCESS;
  idx = OB_INVALID_INDEX;
  if (!is_valid() || !replica.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "log stream", *this, K(replica));
  } else if (OB_FAIL(find_idx_(replica.get_server(), idx))) {
    if (OB_ENTRY_NOT_EXIST != ret) {
      LOG_WARN("find index failed", KR(ret), K(replica));
    }
  }
  return ret;
}

// Find index of a certain server in ObLSInfo's replicas_ where their server address equals.
int ObLSInfo::find_idx_(const ObAddr &server, int64_t &idx) const
{
  idx = OB_INVALID_INDEX;
  int ret = OB_ENTRY_NOT_EXIST;
  if (!is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "log stream", *this, K(server));
  } else {
    for (int64_t i = 0; OB_ENTRY_NOT_EXIST == ret && i < replicas_.count(); ++i) {
      if (server == replicas_.at(i).get_server()) {
        idx = i;
        ret = OB_SUCCESS;
      }
    }
  }
  return ret;
}

// Add this replica to ObLSInfo's replicas_:
// If this replica's server is not in replicas_ then push_back this new replica into replicas_.
// If this replica's server exists in replicas_ then assign this replica to the older one.
int ObLSInfo::add_replica(const ObLSReplica &replica)
{
  int ret = OB_SUCCESS;
  if (!is_valid() || !replica.is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "log stream", *this, K(replica));
  } else if (tenant_id_ != replica.get_tenant_id() || ls_id_ != replica.get_ls_id()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("replica not belong to this log stream",
        KR(ret), KT_(tenant_id), K_(ls_id), K(replica));
  } else {
    int64_t idx = OB_INVALID_INDEX;
    ret = find_idx_(replica, idx);
    if (OB_SUCCESS != ret && OB_ENTRY_NOT_EXIST != ret) {
      LOG_WARN("find index failed", KR(ret), K(replica));
    } else if (OB_ENTRY_NOT_EXIST == ret) {
      if (OB_FAIL(replicas_.push_back(replica))) {
        LOG_WARN("insert replica failed", KR(ret));
      }
    } else { // OB_SUCCESS == ret
      if (idx < 0 || idx >= replicas_.count()) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("invalid index", KR(ret), K(idx), "replica_count", replicas_.count());
      } else if (OB_FAIL(replicas_.at(idx).assign(replica))) {
        LOG_WARN("failed to assign replicas_.at(idx)", KR(ret), K(idx));
      }
    }
  }
  return ret;
}

int ObLSInfo::assign(const ObLSInfo &other)
{
  int ret = OB_SUCCESS;
  if (this != &other) {
    reset();
    tenant_id_ = other.get_tenant_id();
    ls_id_ = other.get_ls_id();
    if (OB_FAIL(copy_assign(replicas_, other.replicas_))) {
      LOG_WARN("failed to copy replicas_", KR(ret));
    }
  }
  return ret;
}

int ObLSInfo::composite_with(const ObLSInfo &other)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(
      tenant_id_ != other.get_tenant_id()
      || ls_id_ != other.get_ls_id())) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("tenant_id or ls_id not matched", KR(ret), KPC(this), K(other));
  } else {
    ObLSReplica tmp_replica;
    int64_t idx = OB_INVALID_INDEX; // not used
    int tmp_ret = OB_SUCCESS;
    for (int64_t i = 0; OB_SUCC(ret) && i < other.get_replicas().count(); i++) {
      const ObLSReplica &ls_replica = other.get_replicas().at(i);
      tmp_ret = find_idx_(ls_replica, idx);
      if (OB_ENTRY_NOT_EXIST != tmp_ret) {
        // ls replica exist or warn, do nothing
        ret = tmp_ret;
      } else {
        tmp_replica.reset();
        if (OB_FAIL(tmp_replica.assign(ls_replica))) {
          LOG_WARN("fail to assign replica", KR(ret), K(ls_replica));
        } else if (FALSE_IT(tmp_replica.update_to_follower_role())) {
        } else if (OB_FAIL(add_replica(tmp_replica))) {
          LOG_WARN("fail to add replica", KR(ret), K(tmp_replica));
        }
      }
    } // end for

    if (FAILEDx(update_replica_status())) {
      LOG_WARN("fail to update replica status", KR(ret), KPC(this));
    }
  }
  return ret;
}

// TODO: make sure the actions of this function
int ObLSInfo::update_replica_status()
{
  int ret = OB_SUCCESS;
  if (!is_valid()) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "ls", *this);
  } else {
    const ObLSReplica::MemberList *member_list = NULL;
    const common::GlobalLearnerList *learner_list = NULL;
    FOREACH_CNT_X(r, replicas_, OB_ISNULL(member_list) && OB_SUCCESS == ret) {
      if (r->is_strong_leader()) {
        member_list = &r->get_member_list();
        learner_list = &r->get_learner_list();
      }
    }

    FOREACH_CNT_X(r, replicas_, OB_SUCCESS == ret) {
      bool in_leader_member_list = (OB_ISNULL(member_list)
        && ObReplicaTypeCheck::is_paxos_replica_V2(r->get_replica_type()));
      int64_t in_member_time_us = 0;
      bool in_leader_learner_list = false;
      ObMember learner;
      // rectify replica_type_
      if (OB_NOT_NULL(learner_list) && learner_list->contains(r->get_server())) {
        r->set_replica_type(REPLICA_TYPE_READONLY);
        in_leader_learner_list = true;
        if (OB_FAIL(learner_list->get_learner_by_addr(r->get_server(), learner))) {
          LOG_WARN("fail to get learner by addr", KR(ret));
        } else if (in_leader_learner_list) {
          in_member_time_us = learner.get_timestamp();
        }
      } else {
        r->set_replica_type(REPLICA_TYPE_FULL);
      }
      // rectify in_member_list_ and in_member_list_time_
      if (OB_NOT_NULL(member_list)) {
        ARRAY_FOREACH_X(*member_list, idx, cnt, !in_leader_member_list) {
          if (r->get_server() == member_list->at(idx)) {
            in_leader_member_list = true;
            in_member_time_us = member_list->at(idx).get_timestamp();
          }
        }
      }
      r->update_in_member_list_status(in_leader_member_list, in_member_time_us);
      r->update_in_learner_list_status(in_leader_learner_list, in_member_time_us);
      // rectify replica_status_
      // follow these rules below:
      // 1 paxos replicas (FULL),NORMAL when in leader's member_list otherwise offline.
      // 2 non_paxos replicas (READONLY),NORMAL when in leader's learner_list otherwise offline
      // 3 if non_paxos replicas are deleted by partition service, status in meta table is set to REPLICA_STATUS_OFFLINE,
      //    then set replica_status to REPLICA_STATUS_OFFLINE
      if (REPLICA_STATUS_OFFLINE == r->get_replica_status()) {
        // do nothing
      } else if (in_leader_member_list || in_leader_learner_list) {
        r->set_replica_status(REPLICA_STATUS_NORMAL);
      } else {
        r->set_replica_status(REPLICA_STATUS_OFFLINE);
      }
    }
  }
  return ret;
}

bool ObLSInfo::is_self_replica(const ObLSReplica &replica) const
{
  return replica.get_tenant_id() == tenant_id_
      && replica.get_ls_id() == ls_id_;
}

int ObLSInfo::init(const uint64_t tenant_id, const ObLSID &ls_id)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(!ls_id.is_valid_with_tenant(tenant_id))) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("init with invalid arguments", KR(ret), K(tenant_id), K(ls_id));
  } else {
    tenant_id_ = tenant_id;
    ls_id_ = ls_id;
    replicas_.reset();
  }
  return ret;
}

int ObLSInfo::init(
    const uint64_t tenant_id,
    const ObLSID &ls_id,
    const common::ObIArray<ObLSReplica> &replicas)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(!ls_id.is_valid_with_tenant(tenant_id))) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("init with invalid arguments", KR(ret), K(tenant_id), K(ls_id), K(replicas));
  } else if (OB_FAIL(replicas_.assign(replicas))) {
    LOG_WARN("fail to assign replicas", KR(ret), K(replicas));
  } else {
    tenant_id_ = tenant_id;
    ls_id_ = ls_id;
  }
  return ret;
}

int ObLSInfo::init_by_replica(const ObLSReplica &replica)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(!replica.is_valid())) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid replica", KR(ret), K(replica));
  } else if (OB_FAIL(init(
      replica.get_tenant_id(),
      replica.get_ls_id()))) {
    LOG_WARN("fail to init ls_info", KR(ret), K(replica));
  } else if (OB_FAIL(add_replica(replica))) {
    LOG_WARN("fail to add replica", KR(ret), K(replica));
  }
  return ret;
}

int ObLSInfo::remove(const common::ObAddr &server)
{
  int ret = OB_SUCCESS;
  int64_t idx = OB_INVALID_INDEX;
  if (OB_UNLIKELY(!is_valid() || !server.is_valid())) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "ls_info", *this, K(server));
  } else if (OB_FAIL(find_idx_(server, idx))) {
    if (OB_ENTRY_NOT_EXIST == ret) {
      ret = OB_SUCCESS;
    } else {
      LOG_WARN("fail to find_idx_", KR(ret), K(server), K_(replicas));
    }
  } else if (OB_UNLIKELY(idx < 0 || idx >= replicas_.count())) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("invalid index", KR(ret), K(idx), "replica_count", replicas_.count());
  } else if (OB_FAIL(replicas_.remove(idx))) {
    LOG_WARN("remove replica failed", KR(ret), K(idx), K(replica_count()));
  }
  return ret;
}

int ObLSInfo::get_paxos_member_addrs(common::ObIArray<ObAddr> &addrs)
{
  int ret = OB_SUCCESS;
  const ObLSReplica *leader_replica = NULL;
  addrs.reset();
  if (OB_UNLIKELY(!is_valid() || replica_count() < 1)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("ls_info is invalid", KR(ret), KPC(this));
  } else if (OB_SUCC(find_leader(leader_replica))) {
    if (OB_ISNULL(leader_replica)) {
      ret = OB_LEADER_NOT_EXIST;
      LOG_WARN("no leader in ls_info", KR(ret), KPC(this));
    } else {
      ObLSReplica::MemberList member_list;
      member_list = leader_replica->get_member_list();
      ARRAY_FOREACH_N(member_list, idx, cnt) {
        const ObAddr &server = member_list.at(idx).get_server();
        if (OB_UNLIKELY(!server.is_valid())) {
          ret = OB_INVALID_ARGUMENT;
          LOG_WARN("invalid server", KR(ret), K(server), K(member_list));
        } else if (OB_FAIL(addrs.push_back(server))) {
          LOG_WARN("fail to push back server", KR(ret), K(server), K(addrs));
        }
      }
    }
  } else { // can't find leader
    LOG_WARN("can't find ls leader in ls_info", KR(ret), KPC(this));
    ret = OB_SUCCESS;
    ARRAY_FOREACH_N(replicas_, idx, cnt) {
      const ObLSReplica &replica = replicas_.at(idx);
      if (OB_UNLIKELY(!replica.is_valid())) {
        ret = OB_INVALID_ARGUMENT;
        LOG_WARN("invalid replica", KR(ret), K(replica));
      } else if (ObReplicaTypeCheck::is_paxos_replica_V2(replica.get_replica_type())) {
        if (OB_FAIL(addrs.push_back(replica.get_server()))) {
          LOG_WARN("fail to push back", KR(ret), K(replica), K(addrs));
        }
      }
    }
  }
  return ret;
}

int ObLSInfo::filter(const ObLSReplicaFilter &filter)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(!is_valid())) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid argument", KR(ret), "ls", *this);
  }
  for (int64_t i = replicas_.count() - 1; OB_SUCC(ret) && i >= 0; --i) {
    bool pass = true;
    if (OB_FAIL(filter.check(replicas_.at(i), pass))) {
      LOG_WARN("filter replica failed", KR(ret), "replica", replicas_.at(i));
    } else {
      if (!pass) {
        if (OB_FAIL(replicas_.remove(i))) {
          LOG_WARN("remove replica failed", KR(ret), "idx", i, K(replica_count()));
        }
      }
    }
  }
  return ret;
}

} // end namespace share
} // end namespace oceanbase
