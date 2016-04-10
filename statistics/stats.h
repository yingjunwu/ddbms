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

#ifndef _STATS_H_
#define _STATS_H_
#include "stats_array.h"

class StatValue {
public:
  StatValue() : value(0) {}
  void operator+=(double value) {
    this->value += value;
  }
  void operator=(double value) {
    this->value = value;
  }
  double get() {return value;}
private:
  double value;
};

class Stats_thd {
public:
	void init(uint64_t thd_id);
	void combine(Stats_thd * stats);
	void print(FILE * outf);
	void print_client(FILE * outf);
	void clear();

	char _pad2[CL_SIZE];
  
  uint64_t* part_cnt;
  uint64_t* part_acc;

  double total_runtime;

  // Execution
  uint64_t txn_cnt;
  uint64_t remote_txn_cnt;
  uint64_t local_txn_cnt;
  uint64_t txn_commit_cnt;
  uint64_t txn_abort_cnt;
  double txn_run_time;
  uint64_t multi_part_txn_cnt;
  double multi_part_txn_run_time;
  uint64_t single_part_txn_cnt;
  double single_part_txn_run_time;

  // Client
  uint64_t txn_sent_cnt;
  double cl_send_intv;

  // Breakdown
  double ts_alloc_time;
  double abort_time;
  double txn_manager_time;
  double txn_index_time;
  double txn_validate_time;
  double txn_cleanup_time;

  // Work queue
  double work_queue_wait_time;
  uint64_t work_queue_cnt;
  uint64_t work_queue_new_cnt;
  double work_queue_new_wait_time;
  uint64_t work_queue_old_cnt;
  double work_queue_old_wait_time;
  double work_queue_enqueue_time;
  double work_queue_dequeue_time;
  uint64_t work_queue_conflict_cnt;

  // Abort queue
  double abort_queue_enqueue_time;
  double abort_queue_dequeue_time;

  // Worker thread
  double worker_process_time;
  uint64_t worker_process_cnt;
  uint64_t * worker_process_cnt_by_type;
  double * worker_process_time_by_type;

  // IO
  double msg_queue_delay_time;
  uint64_t msg_queue_cnt;
  double msg_send_time;
  double msg_recv_time;
  uint64_t msg_batch_cnt;
  uint64_t msg_batch_size_msgs;
  uint64_t msg_batch_size_bytes;
  uint64_t msg_send_cnt;
  uint64_t msg_recv_cnt;
  double msg_unpack_time;
  double mbuf_send_intv_time;

  // Concurrency control, general
  uint64_t cc_conflict_cnt;
  uint64_t txn_wait_cnt;
  uint64_t txn_conflict_cnt;

  // 2PL
  uint64_t twopl_already_owned_cnt;
  uint64_t twopl_owned_cnt;
  uint64_t twopl_sh_owned_cnt;
  uint64_t twopl_ex_owned_cnt;
  double twopl_owned_time;
  double twopl_sh_owned_time;
  double twopl_ex_owned_time;
  double twopl_diff_time;

  // OCC
  double occ_validate_time;
  double occ_cs_wait_time;
  double occ_cs_time;
  double occ_hist_validate_time;
  double occ_act_validate_time;
  double occ_hist_validate_fail_time;
  double occ_act_validate_fail_time;
  uint64_t occ_check_cnt;
  uint64_t occ_abort_check_cnt;
  uint64_t occ_ts_abort_cnt;
  double occ_finish_time;

  // Logging
  uint64_t log_write_cnt;
  double log_write_time;
  uint64_t log_flush_cnt;
  double log_flush_time;
  double log_process_time;

  // Transaction Table
  uint64_t txn_table_new_cnt;
  uint64_t txn_table_get_cnt;
  uint64_t txn_table_release_cnt;
  uint64_t txn_table_cflt_cnt;
  uint64_t txn_table_cflt_size;
  double txn_table_get_time;
  double txn_table_release_time;

	char _pad[CL_SIZE];
};

class Stats {
public:
	// PER THREAD statistics
	Stats_thd ** _stats;
	Stats_thd * totals;
	
	void init();
	void init(uint64_t thread_id);
	void clear(uint64_t tid);
	//void add_lat(uint64_t thd_id, uint64_t latency);
	void commit(uint64_t thd_id);
	void abort(uint64_t thd_id);
	void print_client(bool prog); 
	void print(bool prog);
	void print_cnts(FILE * outf);
	void print_lat_distr();
  void print_lat_distr(uint64_t min, uint64_t max); 
	void print_abort_distr();
  uint64_t get_txn_cnts();
  void util_init();
  void print_util();
  int parseLine(char* line);
  void mem_util(FILE * outf);
  void cpu_util(FILE * outf);
  void print_prof(FILE * outf);

  clock_t lastCPU, lastSysCPU, lastUserCPU;
};

#endif