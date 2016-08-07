/*
   Copyright 2015 Rachael Harding

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

#include "pps.h"
#include "pps_query.h"
#include "query.h"
#include "wl.h"
#include "thread.h"
#include "table.h"
#include "row.h"
#include "index_hash.h"
#include "index_btree.h"
#include "pps_const.h"
#include "transport.h"
#include "msg_queue.h"
#include "message.h"

void PPSTxnManager::init(uint64_t thd_id, Workload * h_wl) {
	TxnManager::init(thd_id, h_wl);
	_wl = (PPSWorkload *) h_wl;
  reset();
	TxnManager::reset();
}

void PPSTxnManager::reset() {
  PPSQuery* pps_query = (PPSQuery*) query;
  state = PPS_PAYMENT0;
  if(pps_query->txn_type == PPS_PAYMENT) {
    state = PPS_PAYMENT0;
  } else if (pps_query->txn_type == PPS_NEW_ORDER) {
    state = PPS_NEWORDER0;
  }
	TxnManager::reset();
}

RC PPSTxnManager::run_txn_post_wait() {
    get_row_post_wait(row);
    next_pps_state();
    return RCOK;
}


RC PPSTxnManager::run_txn() {
#if MODE == SETUP_MODE
  return RCOK;
#endif
  RC rc = RCOK;

#if CC_ALG == CALVIN
  rc = run_calvin_txn();
  return rc;
#endif

  if(IS_LOCAL(txn->txn_id) && 
      (state == PPS_GETPART0 || 
       state == PPS_GETPRODUCT0 ||
       state == PPS_GETSUPPLIER0 ||
       state == PPS_GETPARTBYSUPPLIER0 ||
       state == PPS_GETPARTBYPRODUCT0 )) {
    DEBUG("Running txn %ld\n",txn->txn_id);
#if DISTR_DEBUG
    query->print();
#endif
    query->partitions_touched.add_unique(GET_PART_ID(0,g_node_id));
  }


  while(rc == RCOK && !is_done()) {
    rc = run_txn_state();
  }

  if(IS_LOCAL(get_txn_id())) {
    if(is_done() && rc == RCOK) 
      rc = start_commit();
    else if(rc == Abort)
      rc = start_abort();
  }

  return rc;

}

bool PPSTxnManager::is_done() {
  bool done = false;
  done = state == PPS_FIN;
  return done;
}

RC PPSTxnManager::acquire_locks() {
  uint64_t starttime = get_sys_clock();
  assert(CC_ALG == CALVIN);
  locking_done = false;
  RC rc = RCOK;
  RC rc2 = RCOK;
  INDEX * index;
  itemid_t * item;
  row_t* row;
  uint64_t key;
  incr_lr();
  PPSQuery* pps_query = (PPSQuery*) query;
  uint64_t partition_id_part = parts_to_part(pps_query->part_key);
  uint64_t partition_id_product = product_to_part(pps_query->product_key);
  uint64_t partition_id_supplier = supplier_to_part(pps_query->supplier_key);

  switch(pps_query->txn_type) {
    case PPS_GETPART:
      if(GET_NODE_ID(partition_id_part) == g_node_id) {
        index = _wl->i_parts;
        item = index_read(index, part_key, partition_id_part);
        row_t * row = ((row_t *)item->location);
        rc2 = get_lock(row,RD);
        if(rc2 != RCOK)
          rc = rc2;
      }
      break;
    case PPS_GETPRODUCT:
      if(GET_NODE_ID(partition_id_product) == g_node_id) {
        index = _wl->i_products;
        item = index_read(index, product_key, partition_id_product);
        row_t * row = ((row_t *)item->location);
        rc2 = get_lock(row,RD);
        if(rc2 != RCOK)
          rc = rc2;
      }
      break;
    case PPS_GETSUPPLIER:
      if(GET_NODE_ID(partition_id_supplier) == g_node_id) {
        index = _wl->i_suppliers;
        item = index_read(index, supplier_key, partition_id_supplier);
        row_t * row = ((row_t *)item->location);
        rc2 = get_lock(row,RD);
        if(rc2 != RCOK)
          rc = rc2;
      }
      break;
    case PPS_GETPARTBYSUPPLIER:
      if(GET_NODE_ID(partition_id_supplier) == g_node_id) {
        index = _wl->i_suppliers;
        item = index_read(index, supplier_key, partition_id_supplier);
        row_t * row = ((row_t *)item->location);
        rc2 = get_lock(row,RD);
        if(rc2 != RCOK)
          rc = rc2;
      }
      if(GET_NODE_ID(partition_id_part) == g_node_id) {
        index = _wl->i_parts;
        item = index_read(index, part_key, partition_id_part);
        row_t * row = ((row_t *)item->location);
        rc2 = get_lock(row,RD);
        if(rc2 != RCOK)
          rc = rc2;
      }
      break;
    case PPS_GETPARTBYPRODUCT:
      if(GET_NODE_ID(partition_id_product) == g_node_id) {
        index = _wl->i_products;
        item = index_read(index, product_key, partition_id_product);
        row_t * row = ((row_t *)item->location);
        rc2 = get_lock(row,RD);
        if(rc2 != RCOK)
          rc = rc2;
      }
      if(GET_NODE_ID(partition_id_part) == g_node_id) {
        index = _wl->i_parts;
        item = index_read(index, part_key, partition_id_part);
        row_t * row = ((row_t *)item->location);
        rc2 = get_lock(row,RD);
        if(rc2 != RCOK)
          rc = rc2;
      }
      break;
    default: assert(false);
  }
  if(decr_lr() == 0) {
    if(ATOM_CAS(lock_ready,false,true))
      rc = RCOK;
  }
  txn_stats.wait_starttime = get_sys_clock();
  locking_done = true;
  INC_STATS(get_thd_id(),calvin_sched_time,get_sys_clock() - starttime);
  return rc;
}


void PPSTxnManager::next_pps_state() {
  switch(state) {
    case PPS_GETPART_S:
      state = PPS_GETPART0;
      break;
    case PPS_GETPART0:
      state = PPS_GETPART1;
      break;
    case PPS_GETPART1:
      state = PPS_FIN;
      break;
    case PPS_GETPRODUCT_S:
      state = PPS_GETPRODUCT0;
      break;
    case PPS_GETPRODUCT0:
      state = PPS_GETPRODUCT1;
      break;
    case PPS_GETPRODUCT1:
      state = PPS_FIN;
      break;
    case PPS_GETSUPPLIER_S:
      state = PPS_GETSUPPLIER0;
      break;
    case PPS_GETSUPPLIER0:
      state = PPS_GETSUPPLIER1;
      break;
    case PPS_GETSUPPLIER1:
      state = PPS_FIN;
      break;
    case PPS_GETPARTBYSUPPLIER_S:
      state = PPS_GETPARTBYSUPPLIER0;
      break;
    case PPS_GETPARTBYSUPPLIER0:
      state = PPS_GETPARTBYSUPPLIER1;
      break;
    case PPS_GETPARTBYSUPPLIER1:
      state = PPS_GETPARTBYSUPPLIER2;
      break;
    case PPS_GETPARTBYSUPPLIER2:
      state = PPS_GETPARTBYSUPPLIER3;
      break;
    case PPS_GETPARTBYSUPPLIER3:
      state = PPS_GETPARTBYSUPPLIER4;
      break;
    case PPS_GETPARTBYSUPPLIER4:
      state = PPS_GETPARTBYSUPPLIER5;
      break;
    case PPS_GETPARTBYSUPPLIER5:
      state = PPS_FIN;
      break;
    case PPS_GETPARTBYPRODUCT_S:
      state = PPS_GETPARTBYPRODUCT0;
      break;
    case PPS_GETPARTBYPRODUCT0:
      state = PPS_GETPARTBYPRODUCT1;
      break;
    case PPS_GETPARTBYPRODUCT1:
      state = PPS_GETPARTBYPRODUCT2;
      break;
    case PPS_GETPARTBYPRODUCT2:
      state = PPS_GETPARTBYPRODUCT3;
      break;
    case PPS_GETPARTBYPRODUCT3:
      state = PPS_GETPARTBYPRODUCT4;
      break;
    case PPS_GETPARTBYPRODUCT4:
      state = PPS_GETPARTBYPRODUCT5;
      break;
    case PPS_GETPARTBYPRODUCT5:
      state = PPS_FIN;
      break;
    case PPS_FIN:
      break;
    default:
      assert(false);
  }

}

// remote request should always be on a parts key, given our workload
RC PPSTxnManager::send_remote_request() {
  assert(IS_LOCAL(get_txn_id()));
  PPSQuery* pps_query = (PPSQuery*) query;
  PPSRemTxnType next_state = PPS_FIN;
	uint64_t part_key = pps_query->part_key;
  uint64_t dest_node_id = UINT64_MAX;
  dest_node_id = GET_NODE_ID(parts_to_part(part_key));
  PPSQueryMessage * msg = (PPSQueryMessage*)Message::create_message(this,RQRY);
  msg->state = state;
  query->partitions_touched.add_unique(GET_PART_ID(0,dest_node_id));
  msg_queue.enqueue(get_thd_id(),msg,dest_node_id);
  state = next_state;
  return WAIT_REM;
}

RC PPSTxnManager::run_txn_state() {
  PPSQuery* pps_query = (PPSQuery*) query;
	RC rc = RCOK;

	switch (state) {
		case PPS_PAYMENT0 :
      if(w_loc)
			  rc = run_payment_0(w_id, d_id, d_w_id, h_amount, row);
      else {
        rc = send_remote_request();
      }
			break;
		case PPS_PAYMENT1 :
			rc = run_payment_1(w_id, d_id, d_w_id, h_amount, row);
      break;
    case PPS_FIN :
      state = PPS_FIN;
      break;
		default:
			assert(false);
	}
  switch(state) {
    case PPS_GETPART0:
      if (part_loc)
        rc = run_getpart_0(part_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETPART1:
        rc = run_getpart_1(part_key, row);
      break;
    case PPS_GETPRODUCT0:
      if (product_loc)
        rc = run_getproduct_0(product_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETPRODUCT1:
        rc = run_getproduct_1(product_key, row);
      break;
    case PPS_GETSUPPLIER0:
      if (supplier_loc)
        rc = run_getsupplier_0(supplier_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETSUPPLIER1:
        rc = run_getsupplier_1(supplier_key, row);
      break;
    case PPS_GETPARTBYSUPPLIER0:
      if (supplier_loc)
        rc = run_getsupplier_0(supplier_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETPARTBYSUPPLIER1:
        rc = run_getsupplier_1(supplier_key, row);
      break;
    case PPS_GETPARTBYSUPPLIER2:
      if (supplier_loc)
        rc = run_getsupplier_2(supplier_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETPARTBYSUPPLIER3:
        rc = run_getsupplier_3(supplier_key, row);
      break;
    case PPS_GETPARTBYSUPPLIER4:
      if (supplier_loc)
        rc = run_getsupplier_4(supplier_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETPARTBYSUPPLIER5:
        rc = run_getsupplier_5(supplier_key, row);
      break;
    case PPS_GETPARTBYPRODUCT0:
      if (product_loc)
        rc = run_getproduct_0(product_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETPARTBYPRODUCT1:
        rc = run_getproduct_1(product_key, row);
      break;
    case PPS_GETPARTBYPRODUCT2:
      if (product_loc)
        rc = run_getproduct_2(product_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETPARTBYPRODUCT3:
        rc = run_getproduct_3(product_key, row);
      break;
    case PPS_GETPARTBYPRODUCT4:
      if (part_loc)
        rc = run_getproduct_4(part_key, row);
      else
        rc = send_remote_request();
      break;
    case PPS_GETPARTBYPRODUCT5:
        rc = run_getproduct_5(row);
      break;
    case PPS_FIN:
      state = PPS_FIN;
      break;
    default:
      assert(false);
  }
  if(rc == RCOK)
    next_pps_state();
  return rc;
}

inline RC PPSTxnManager::run_getpart_0(uint64_t part_key, row_t *& r_local) {
  /*
    SELECT * FROM PARTS WHERE PART_KEY = :part_key; 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_parts;
		item = index_read(index, part_key, parts_to_part(part_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
}
inline RC PPSTxnManager::run_getpart_1(row_t *& r_local) {
  /*
    SELECT * FROM PARTS WHERE PART_KEY = :part_key; 
   */
  assert(r_local);
  getAllFields(r_local);
  return RCOK;
}


inline RC PPSTxnManager::run_getproduct_0(uint64_t product_key, row_t *& r_local) {
  /*
    SELECT * FROM PRODUCTS WHERE PRODUCT_KEY = :product_key; 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_products;
		item = index_read(index, product_key, product_to_part(product_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
}
inline RC PPSTxnManager::run_getproduct_1(row_t *& r_local) {
  /*
    SELECT * FROM PRODUCTS WHERE PRODUCT_KEY = :product_key; 
   */
  assert(r_local);
  getAllFields(r_local);
  return RCOK;
}

inline void PPSTxnManager::getThreeFields(row_t *& r_local) {
  char * fields = new char[100];
  r_local->get_value(FIELD1,fields);
  r_local->get_value(FIELD2,fields);
  r_local->get_value(FIELD3,fields);
}

inline void PPSTxnManager::getAllFields(row_t *& r_local) {
  char * fields = new char[100];
  r_local->get_value(FIELD1,fields);
  r_local->get_value(FIELD2,fields);
  r_local->get_value(FIELD3,fields);
  r_local->get_value(FIELD4,fields);
  r_local->get_value(FIELD5,fields);
  r_local->get_value(FIELD6,fields);
  r_local->get_value(FIELD7,fields);
  r_local->get_value(FIELD8,fields);
  r_local->get_value(FIELD9,fields);
  r_local->get_value(FIELD10,fields);
}

inline RC PPSTxnManager::run_getsupplier_0(uint64_t supplier_key, row_t *& r_local) {
  /*
    SELECT * FROM SUPPLIERS WHERE SUPPLIER_KEY = :supplier_key; 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_suppliers;
		item = index_read(index, supplier_key, supplier_to_part(supplier_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
    return rc;

}

inline RC PPSTxnManager::run_getsupplier_1(row_t *& r_local) {
  /*
    SELECT * FROM SUPPLIERS WHERE SUPPLIER_KEY = :supplier_key; 
   */
  assert(r_local);
  getAllFields(r_local);
  return RCOK;

}

inline RC PPSTxnManager::run_getpartsbyproduct_0(uint64_t product_key, row_t *& r_wh_local) {
  /*
    SELECT FIELD1, FIELD2, FIELD3 FROM PRODUCTS WHERE PRODUCT_KEY = ? 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_products;
		item = index_read(index, product_key, product_to_part(product_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
}
inline RC PPSTxnManager::run_getpartsbyproduct_1(uint64_t product_key, row_t *& r_wh_local) {
  /*
    SELECT FIELD1, FIELD2, FIELD3 FROM PRODUCTS WHERE PRODUCT_KEY = ? 
   */
  assert(r_local);
  getThreeFields(r_local);
  return RCOK;
}

inline RC PPSTxnManager::run_getpartsbyproduct_2(uint64_t product_key, row_t *& r_wh_local) {
  /*
    SELECT PART_KEY FROM USES WHERE PRODUCT_KEY = ? 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_uses;
		item = index_read(index, product_key, product_to_part(product_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
    return rc;
}

inline RC PPSTxnManager::run_getpartsbyproduct_3(uint64_t &part_key, row_t *& r_wh_local) {
  /*
    SELECT PART_KEY FROM USES WHERE PRODUCT_KEY = ? 
   */
  char * fields = new char[100];
  r_local->get_value(FIELD1,fields);
  part_key = (uint64_t)fields;
  return RCOK;
}

inline RC PPSTxnManager::run_getpartsbyproduct_4(uint64_t part_key, row_t *& r_wh_local) {
  /*
   SELECT FIELD1, FIELD2, FIELD3 FROM PARTS WHERE PART_KEY = ? 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_parts;
		item = index_read(index, part_key, parts_to_part(part_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
    return rc;
}
inline RC PPSTxnManager::run_getpartsbyproduct_5(row_t *& r_local) {
  /*
   SELECT FIELD1, FIELD2, FIELD3 FROM PARTS WHERE PART_KEY = ? 
   */
  assert(r_local);
  getThreeFields(r_local);
  return RCOK;
}

inline RC PPSTxnManager::run_getpartsbysupplier_0(uint64_t supplier_key, row_t *& r_wh_local) {
  /*
    SELECT FIELD1, FIELD2, FIELD3 FROM suppliers WHERE supplier_KEY = ? 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_suppliers;
		item = index_read(index, supplier_key, suppliers_to_part(supplier_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
    return rc;
}
inline RC PPSTxnManager::run_getpartsbysupplier_1(row_t *& r_wh_local) {
  /*
    SELECT FIELD1, FIELD2, FIELD3 FROM suppliers WHERE supplier_KEY = ? 
   */
  assert(r_local);
  getThreeFields(r_local);
  return RCOK;
}

inline RC PPSTxnManager::run_getpartsbysupplier_2(uint64_t supplier_key, row_t *& r_wh_local) {
  /*
    SELECT PART_KEY FROM USES WHERE supplier_KEY = ? 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_uses;
		item = index_read(index, supplier_key, suppliers_to_part(supplier_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
    return rc;
}
inline RC PPSTxnManager::run_getpartsbysupplier_3(uint64_t &part_key, row_t *& r_wh_local) {
  /*
    SELECT PART_KEY FROM USES WHERE supplier_KEY = ? 
   */
  assert(r_local);
  char * fields = new char[100];
  r_local->get_value(FIELD1,fields);
  part_key = (uint64_t)fields;
  return RCOK;
}
inline RC PPSTxnManager::run_getpartsbysupplier_4(uint64_t part_key, row_t *& r_wh_local) {
  /*
   SELECT FIELD1, FIELD2, FIELD3 FROM PARTS WHERE PART_KEY = ? 
   */
    RC rc;
    itemid_t * item;
		INDEX * index = _wl->i_parts;
		item = index_read(index, part_key, parts_to_part(part_key));
		assert(item != NULL);
		r_loc = (row_t *) item->location;
    rc = get_row(r_loc, RD, r_local);
    return rc;
}
inline RC PPSTxnManager::run_getpartsbysupplier_5(row_t *& r_wh_local) {
  /*
   SELECT FIELD1, FIELD2, FIELD3 FROM PARTS WHERE PART_KEY = ? 
   */
  assert(r_local);
  getThreeFields(r_local);
  return RCOK;
}

RC PPSTxnManager::run_calvin_txn() {
  RC rc = RCOK;
  uint64_t starttime = get_sys_clock();
  PPSQuery* pps_query = (PPSQuery*) query;
  DEBUG("(%ld,%ld) Run calvin txn\n",txn->txn_id,txn->batch_id);
  while(!calvin_exec_phase_done() && rc == RCOK) {
    DEBUG("(%ld,%ld) phase %d\n",txn->txn_id,txn->batch_id,this->phase);
    switch(this->phase) {
      case CALVIN_RW_ANALYSIS:
        // Phase 1: Read/write set analysis
        calvin_expected_rsp_cnt = pps_query->get_participants(_wl);
        if(query->participant_nodes[g_node_id] == 1) {
          calvin_expected_rsp_cnt--;
        }

        DEBUG("(%ld,%ld) expects %d responses; %ld participants, %ld active\n",txn->txn_id,txn->batch_id,calvin_expected_rsp_cnt,query->participant_nodes.size(),query->active_nodes.size());

        this->phase = CALVIN_LOC_RD;
        break;
      case CALVIN_LOC_RD:
        // Phase 2: Perform local reads
        DEBUG("(%ld,%ld) local reads\n",txn->txn_id,txn->batch_id);
        rc = run_pps_phase2();
        //release_read_locks(pps_query);

        this->phase = CALVIN_SERVE_RD;
        break;
      case CALVIN_SERVE_RD:
        // Phase 3: Serve remote reads
        if(query->participant_nodes[g_node_id] == 1) {
          rc = send_remote_reads();
        }
        if(query->active_nodes[g_node_id] == 1) {
          this->phase = CALVIN_COLLECT_RD;
          if(calvin_collect_phase_done()) {
            rc = RCOK;
          } else {
            assert(calvin_expected_rsp_cnt > 0);
            DEBUG("(%ld,%ld) wait in collect phase; %d / %d rfwds received\n",txn->txn_id,txn->batch_id,rsp_cnt,calvin_expected_rsp_cnt);
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
        rc = run_pps_phase5();
        this->phase = CALVIN_DONE;
        break;
      default:
        assert(false);
    }
  }
  txn_stats.process_time += get_sys_clock() - starttime;
  txn_stats.wait_starttime = get_sys_clock();
  return rc;
  
}


RC PPSTxnManager::run_pps_phase2() {
  PPSQuery* pps_query = (PPSQuery*) query;
  RC rc = RCOK;
  assert(CC_ALG == CALVIN);

	switch (pps_query->txn_type) {
		case PPS_PAYMENT :
      break;
		case PPS_NEW_ORDER :
        break;
    default: assert(false);
  }
  return rc;
}

RC PPSTxnManager::run_pps_phase5() {
  PPSQuery* pps_query = (PPSQuery*) query;
  RC rc = RCOK;
  assert(CC_ALG == CALVIN);

	switch (pps_query->txn_type) {
		case PPS_PAYMENT :
      if(w_loc) {
      }
      if(c_w_loc) {
      }
      break;
		case PPS_NEW_ORDER :
      if(w_loc) {
      }
        break;
    default: assert(false);
  }
  return rc;

}

