/*
   Copyright 2016 Massachusetts Institute of Technology

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "global.h"
#include "helper.h"
#include "ycsb.h"
#include "ycsb_query.h"
#include "wl.h"
#include "thread.h"
#include "table.h"
#include "row.h"
#include "index_hash.h"
#include "index_btree.h"
#include "catalog.h"
#include "manager.h"
#include "row_lock.h"
#include "row_ts.h"
#include "row_mvcc.h"
#include "mem_alloc.h"
#include "query.h"
#include "msg_queue.h"
#include "message.h"

void YCSBTxnManager::init(uint64_t thd_id, Workload * h_wl) {
	TxnManager::init(thd_id, h_wl);
	_wl = (YCSBWorkload *) h_wl;
  reset();
}

void YCSBTxnManager::reset() {
  state = YCSB_0;
  next_record_id = 0;
	TxnManager::reset();
}

RC YCSBTxnManager::acquire_locks() {
  uint64_t starttime = get_sys_clock();
  assert(CC_ALG == CALVIN);
  YCSBQuery* ycsb_query = (YCSBQuery*) query;
  locking_done = false;
  RC rc = RCOK;
  incr_lr();
  assert(ycsb_query->requests.size() == g_req_per_query);
  assert(phase == CALVIN_RW_ANALYSIS);
	for (uint32_t rid = 0; rid < ycsb_query->requests.size(); rid ++) {
		ycsb_request * req = ycsb_query->requests[rid];
		uint64_t part_id = _wl->key_to_part( req->key );
    DEBUG("LK Acquire (%ld,%ld) %d,%ld -> %ld\n", get_txn_id(), get_batch_id(), req->acctype,
<<<<<<< HEAD
          req->key, get_key_node_id(req->key));
    if (get_key_node_id(req->key) != g_node_id) continue;
=======
          req->key, GET_NODE_ID_MINI(req->key));
		if (GET_NODE_ID_MINI(req->key) != g_node_id) continue;
>>>>>>> 8ee691f8bc5012b01a09fa4ed4cd44586f4b7b9d
		INDEX * index = _wl->the_index;
		itemid_t * item;
		item = index_read(index, req->key, part_id);
		row_t * row = ((row_t *)item->location);
		RC rc2 = get_lock(row,req->acctype);
    if(rc2 != RCOK) {
      rc = rc2;
    }
	}
  if(decr_lr() == 0) {
    if (ATOM_CAS(lock_ready, false, true)) rc = RCOK;
  }
  txn_stats.wait_starttime = get_sys_clock();
  /*
  if(rc == WAIT && lock_ready_cnt == 0) {
    if(ATOM_CAS(lock_ready,false,true))
    //lock_ready = true;
      rc = RCOK;
  }
  */
  INC_STATS(get_thd_id(),calvin_sched_time,get_sys_clock() - starttime);
  locking_done = true;
  return rc;
}


RC YCSBTxnManager::run_txn() {
  RC rc = RCOK;
  assert(CC_ALG != CALVIN);

  if(IS_LOCAL(txn->txn_id) && state == YCSB_0 && next_record_id == 0) {
    DEBUG("Running txn %ld\n",txn->txn_id);
    //query->print();
    query->partitions_touched.add_unique(GET_PART_ID(0,g_node_id));
    //query->partitions_touched.add_unique(key_to_part(((YCSBQuery*)query)->requests[0]->key));
  }

  uint64_t starttime = get_sys_clock();

  while(rc == RCOK && !is_done()) {
    rc = run_txn_state();
  }

  uint64_t curr_time = get_sys_clock();
  txn_stats.process_time += curr_time - starttime;
  txn_stats.process_time_short += curr_time - starttime;
  txn_stats.wait_starttime = get_sys_clock();

  if(IS_LOCAL(get_txn_id())) {
    if(is_done() && rc == RCOK)
      rc = start_commit();
    else if(rc == Abort)
      rc = start_abort();
  } else if(rc == Abort){
    rc = abort();
  }
  /*
  if(is_done() && rc == RCOK)
    rc = start_commit();
  else if(rc == Abort)
    rc = start_abort();
  */
  return rc;

}

RC YCSBTxnManager::run_txn_post_wait() {
  uint64_t starttime = get_sys_clock();
  get_row_post_wait(row);
  uint64_t curr_time = get_sys_clock();
  txn_stats.process_time += curr_time - starttime;
  txn_stats.process_time_short += curr_time - starttime;
  next_ycsb_state();
  INC_STATS(get_thd_id(),trans_benchmark_compute_time,get_sys_clock() - curr_time);
  return RCOK;
}

bool YCSBTxnManager::is_done() { return next_record_id >= ((YCSBQuery*)query)->requests.size(); }

void YCSBTxnManager::next_ycsb_state() {
  switch(state) {
    case YCSB_0:
      state = YCSB_1;
      break;
    case YCSB_1:
      next_record_id++;
      if(send_RQRY_RSP || !IS_LOCAL(txn->txn_id) || !is_done()) {
        state = YCSB_0;
      } else {
        state = YCSB_FIN;
      }
      break;
    case YCSB_FIN:
      break;
    default:
      assert(false);
  }
}

bool YCSBTxnManager::is_local_request(uint64_t idx) {
<<<<<<< HEAD
  return get_key_node_id(((YCSBQuery*)query)->requests[idx]->key) == g_node_id;
=======
  return GET_NODE_ID_MINI(((YCSBQuery*)query)->requests[idx]->key) == g_node_id;
>>>>>>> 8ee691f8bc5012b01a09fa4ed4cd44586f4b7b9d
}

RC YCSBTxnManager::send_remote_request() {
  YCSBQuery* ycsb_query = (YCSBQuery*) query;
<<<<<<< HEAD
  uint64_t dest_node_id = get_key_node_id(ycsb_query->requests[next_record_id]->key);
=======
  uint64_t dest_node_id = GET_NODE_ID_MINI(ycsb_query->requests[next_record_id]->key);
>>>>>>> 8ee691f8bc5012b01a09fa4ed4cd44586f4b7b9d
  ycsb_query->partitions_touched.add_unique(GET_PART_ID(0,dest_node_id));
  //ycsb_query->partitions_touched.add_unique(key_to_part(((YCSBQuery*)query)->requests[next_record_id]->key));
  DEBUG("ycsb send remote request %ld, %ld\n",txn->txn_id,txn->batch_id);
  msg_queue.enqueue(get_thd_id(),Message::create_message(this,RQRY),dest_node_id);
  txn_stats.trans_process_network_start_time = get_sys_clock();
  return WAIT_REM;
}

void YCSBTxnManager::copy_remote_requests(YCSBQueryMessage * msg) {
  YCSBQuery* ycsb_query = (YCSBQuery*) query;
  //msg->requests.init(ycsb_query->requests.size());
<<<<<<< HEAD
  uint64_t dest_node_id = get_key_node_id(ycsb_query->requests[next_record_id]->key);
#if ONE_NODE_RECIEVE == 1 && defined(NO_REMOTE) && LESS_DIS_NUM == 10
  while (next_record_id < ycsb_query->requests.size() && get_key_node_id(ycsb_query->requests[next_record_id]->key) == dest_node_id) {
#else
  while (next_record_id < ycsb_query->requests.size() && !is_local_request(next_record_id) &&
         get_key_node_id(ycsb_query->requests[next_record_id]->key) == dest_node_id) {
#endif
=======
  uint64_t dest_node_id = GET_NODE_ID_MINI(ycsb_query->requests[next_record_id]->key);
  #if ONE_NODE_RECIEVE == 1 && defined(NO_REMOTE) && LESS_DIS_NUM == 10
    while (next_record_id < ycsb_query->requests.size() && GET_NODE_ID(key_to_part(ycsb_query->requests[next_record_id]->key)) == dest_node_id) {
  #else
    while (next_record_id < ycsb_query->requests.size() && !is_local_request(next_record_id) &&
         GET_NODE_ID_MINI(ycsb_query->requests[next_record_id]->key) == dest_node_id) {
  #endif
>>>>>>> 8ee691f8bc5012b01a09fa4ed4cd44586f4b7b9d
    YCSBQuery::copy_request_to_msg(ycsb_query,msg,next_record_id++);
  }
}

RC YCSBTxnManager::run_txn_state() {
  YCSBQuery* ycsb_query = (YCSBQuery*) query;
	ycsb_request * req = ycsb_query->requests[next_record_id];
	uint64_t part_id = _wl->key_to_part( req->key );
<<<<<<< HEAD
  assert(part_id < g_part_cnt);

  /*
    the loc variable is modified using minipart_map
  */
  //bool loc = GET_NODE_ID(part_id) == g_node_id;
  bool loc = get_key_node_id(req->key) == g_node_id;


=======
  assert(part_id >= 0);
  bool loc = GET_NODE_ID_MINI(req->key) == g_node_id;
  if (!loc) this->isdistributed = true;
  
  #if MIGRATION
  #if MIGRATION_ALG == REMUS
    //part_map修改了，只有那些本地生成的事务，访问的mini分区刚好迁移完了，继续保持本地执行
    if (g_node_id == MIGRATION_SRC_NODE && IS_LOCAL(txn->txn_id) && loc == false && part_id == MIGRATION_PART && get_part_status(key_to_part(req->key)) == 2){
      loc = true;
      //std::cout<<"keep local remus"<<' ';
    }
  
  #elif MIGRATION_ALG == DETEST
    //part_map修改了，只有那些本地生成的事务，访问的mini分区刚好迁移完了，继续保持本地执行
    if (g_node_id == MIGRATION_SRC_NODE && IS_LOCAL(txn->txn_id) && loc == false && part_id == MIGRATION_PART && get_minipart_status(get_minipart_id(req->key)) == 2){
      loc = true;
      //std::cout<<"keep local detest"<<' ';
    }
  #elif MIGRATION_ALG == SQUALL
    if (g_node_id == MIGRATION_DES_NODE && part_id == MIGRATION_PART && squall_status == 1 && get_squallpart_status(get_squallpart_id(req->key)) == 0){
    }
  #elif MIGRATION_ALG == DETEST_SPLIT
    if (this->get_txn_id() % g_node_cnt == 0 && loc == false && part_id == 0 && get_minipart_status(get_minipart_id(req->key)) != 0 && this->txn_stats.starttime < remus_finish_time){
      loc = true;
      std::cout<<"keep local detest"<<' ';
    }
  #endif
  #endif
  
>>>>>>> 8ee691f8bc5012b01a09fa4ed4cd44586f4b7b9d
  //std::cout<<"key is:"<<req->key<<" node is "<<GET_NODE_ID(part_id)<<" part is "<<part_id<<endl;

	RC rc = RCOK;

	switch (state) {
		case YCSB_0 :
      if(loc) {
        rc = run_ycsb_0(req,row);
      } else {
        rc = send_remote_request();

      }

      break;
		case YCSB_1 :
      rc = run_ycsb_1(req->acctype,row);
      break;
    case YCSB_FIN :
      state = YCSB_FIN;
      break;
    default:
			assert(false);
  }

  if (rc == RCOK) next_ycsb_state();

  return rc;
}

RC YCSBTxnManager::run_ycsb_0(ycsb_request * req,row_t *& row_local) {
  uint64_t starttime = get_sys_clock();
  RC rc = RCOK;
  int part_id = _wl->key_to_part( req->key );
  access_t type = req->acctype;
  itemid_t * m_item;
  INC_STATS(get_thd_id(),trans_benchmark_compute_time,get_sys_clock() - starttime);
  m_item = index_read(_wl->the_index, req->key, part_id);
  starttime = get_sys_clock();
  row_t * row = ((row_t *)m_item->location);

  INC_STATS(get_thd_id(),trans_benchmark_compute_time,get_sys_clock() - starttime);
  rc = get_row(row, type,row_local);
  return rc;

}

RC YCSBTxnManager::run_ycsb_1(access_t acctype, row_t * row_local) {
  uint64_t starttime = get_sys_clock();
  if (acctype == RD || acctype == SCAN) {
    int fid = 0;
		char * data = row_local->get_data();
		uint64_t fval __attribute__ ((unused));
    fval = *(uint64_t *)(&data[fid * 100]);
#if ISOLATION_LEVEL == READ_COMMITTED || ISOLATION_LEVEL == READ_UNCOMMITTED
    // Release lock after read
    release_last_row_lock();
#endif

  } else {
    assert(acctype == WR);
		int fid = 0;
	  char * data = row_local->get_data();
	  *(uint64_t *)(&data[fid * 100]) = 0;
#if YCSB_ABORT_MODE
    if (data[0] == 'a') return RCOK;
#endif

#if ISOLATION_LEVEL == READ_UNCOMMITTED
    // Release lock after write
    release_last_row_lock();
#endif
  }
  INC_STATS(get_thd_id(),trans_benchmark_compute_time,get_sys_clock() - starttime);
  return RCOK;
}
RC YCSBTxnManager::run_calvin_txn() {
  RC rc = RCOK;
  uint64_t starttime = get_sys_clock();
  YCSBQuery* ycsb_query = (YCSBQuery*) query;
  DEBUG("(%ld,%ld) Run calvin txn\n",txn->txn_id,txn->batch_id);
  while(!calvin_exec_phase_done() && rc == RCOK) {
    DEBUG("(%ld,%ld) phase %d\n",txn->txn_id,txn->batch_id,this->phase);
    switch(this->phase) {
      case CALVIN_RW_ANALYSIS:

        // Phase 1: Read/write set analysis
        calvin_expected_rsp_cnt = ycsb_query->get_participants(_wl);
#if YCSB_ABORT_MODE
        if(query->participant_nodes[g_node_id] == 1) {
          calvin_expected_rsp_cnt--;
        }
#else
        calvin_expected_rsp_cnt = 0;
#endif
        DEBUG("(%ld,%ld) expects %d responses;\n", txn->txn_id, txn->batch_id,
              calvin_expected_rsp_cnt);

        this->phase = CALVIN_LOC_RD;
        break;
      case CALVIN_LOC_RD:
        // Phase 2: Perform local reads
        DEBUG("(%ld,%ld) local reads\n",txn->txn_id,txn->batch_id);
        rc = run_ycsb();
        //release_read_locks(query);

        this->phase = CALVIN_SERVE_RD;
        break;
      case CALVIN_SERVE_RD:
        // Phase 3: Serve remote reads
        // If there is any abort logic, relevant reads need to be sent to all active nodes...
        if(query->participant_nodes[g_node_id] == 1) {
          rc = send_remote_reads();
        }
        if(query->active_nodes[g_node_id] == 1) {
          this->phase = CALVIN_COLLECT_RD;
          if(calvin_collect_phase_done()) {
            rc = RCOK;
          } else {
            DEBUG("(%ld,%ld) wait in collect phase; %d / %d rfwds received\n", txn->txn_id,
                  txn->batch_id, rsp_cnt, calvin_expected_rsp_cnt);
            rc = WAIT;
          }
        } else { // Done
          rc = RCOK;
          this->phase = CALVIN_DONE;
        }

        break;
      case CALVIN_COLLECT_RD:
        // Phase 4: Collect remote reads
        this->phase = CALVIN_EXEC_WR;
        break;
      case CALVIN_EXEC_WR:
        // Phase 5: Execute transaction / perform local writes
        DEBUG("(%ld,%ld) execute writes\n",txn->txn_id,txn->batch_id);
        rc = run_ycsb();
        this->phase = CALVIN_DONE;
        break;
      default:
        assert(false);
    }
  }
  uint64_t curr_time = get_sys_clock();
  txn_stats.process_time += curr_time - starttime;
  txn_stats.process_time_short += curr_time - starttime;
  txn_stats.wait_starttime = get_sys_clock();
  return rc;
}

RC YCSBTxnManager::run_ycsb() {
  RC rc = RCOK;
  assert(CC_ALG == CALVIN);
  YCSBQuery* ycsb_query = (YCSBQuery*) query;

  for (uint64_t i = 0; i < ycsb_query->requests.size(); i++) {
	  ycsb_request * req = ycsb_query->requests[i];
    if (this->phase == CALVIN_LOC_RD && req->acctype == WR) continue;
    if (this->phase == CALVIN_EXEC_WR && req->acctype == RD) continue;

<<<<<<< HEAD
		uint64_t part_id = _wl->key_to_part( req->key );
    assert(part_id < g_part_cnt);
    
    bool loc = get_key_node_id(req->key) == g_node_id;
=======
		//uint64_t part_id = _wl->key_to_part( req->key );
    bool loc = GET_NODE_ID_MINI(req->key) == g_node_id;
>>>>>>> 8ee691f8bc5012b01a09fa4ed4cd44586f4b7b9d

    if (!loc) continue;

    rc = run_ycsb_0(req,row);
    assert(rc == RCOK);

    rc = run_ycsb_1(req->acctype,row);
    assert(rc == RCOK);
  }
  return rc;

}

