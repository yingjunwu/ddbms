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

#include "global.h"
#include "manager.h"
#include "transport.h"
#include "nn.hpp"
#include "tpcc_query.h"
#include "query.h"
#include "wl.h"
#include "message.h"

#define MAX_IFADDR_LEN 20

/*
	 HEADER FORMAT:
	 32b destination ID
	 32b source ID
	 32b ??? [unused] ??? transaction ID
	 ===> 32b message type <=== First 4 bytes of data
	 N?	data

	 */
void Transport::shutdown() {
}

void Transport::read_ifconfig(const char * ifaddr_file) {
	uint64_t cnt = 0;
	ifstream fin(ifaddr_file);
	string line;
  while (getline(fin, line)) {
		//memcpy(ifaddr[cnt],&line[0],12);
    strcpy(ifaddr[cnt],&line[0]);
		cnt++;
	}
	for(uint64_t i=0;i<_node_cnt;i++) {
		printf("%ld: %s\n",i,ifaddr[i]);
	}
}

void Transport::init(uint64_t node_id,Workload * workload) {
	_wl = workload;
  _node_cnt = g_node_cnt + g_client_node_cnt + g_repl_cnt * g_node_cnt;
	_node_id = node_id;
  if(ISCLIENT)
    _sock_cnt = g_client_send_thread_cnt * g_servers_per_client + (_node_cnt)*2;
  else
    _sock_cnt = (_node_cnt)*2 + g_client_send_thread_cnt;
	_thd_id = 1;

  s = new Socket[_sock_cnt];
  rr = 0;
	printf("Tport Init %ld: %ld\n",node_id,_sock_cnt);

#if !TPORT_TYPE_IPC
	// Read ifconfig file
	ifaddr = new char *[_node_cnt];
	for(uint64_t i=0;i<_node_cnt;i++) {
		ifaddr[i] = new char[MAX_IFADDR_LEN];
	}
	string path;
#if SHMEM_ENV
  path = "/dev/shm/";
#else
	char * cpath;
  cpath = getenv("SCHEMA_PATH");
	if(cpath == NULL)
		path = "./";
	else
		path = string(cpath);
#endif
	path += "ifconfig.txt";
	cout << "reading ifconfig file: " << path << endl;
	read_ifconfig(path.c_str());
#endif

	int rc = 0;

	int timeo = 1000; // timeout in ms
	int stimeo = 1000; // timeout in ms
  int opt = 0;
	for(uint64_t i=0;i<_sock_cnt;i++) {
		s[i].sock.setsockopt(NN_SOL_SOCKET,NN_RCVTIMEO,&timeo,sizeof(timeo));
		s[i].sock.setsockopt(NN_SOL_SOCKET,NN_SNDTIMEO,&stimeo,sizeof(stimeo));
    // NN_TCP_NODELAY doesn't cause TCP_NODELAY to be set -- nanomsg issue #118
		s[i].sock.setsockopt(NN_SOL_SOCKET,NN_TCP_NODELAY,&opt,sizeof(opt));
	}

	printf("Node ID: %d/%lu\n",g_node_id,_node_cnt);
  fflush(stdout);

  uint64_t s_cnt = 0;
#if SET_AFFINITY
  uint64_t s_thd;
  if(ISCLIENT)
    s_thd = g_client_send_thread_cnt;
  else
    s_thd = g_send_thread_cnt;
#endif

  // Listening port
  char socket_name[MAX_TPORT_NAME];
  int port;

    for(uint64_t j = 0; j < _node_cnt; j++) {
      if(j == g_node_id) {
        s_cnt++;
        continue;
      }
#if TPORT_TYPE_IPC
      port = j;
      sprintf(socket_name,"%s://node_%d_%d%s",TPORT_TYPE,port,g_node_id,TPORT_PORT);
#elif !SET_AFFINITY
      port = TPORT_PORT + j * _node_cnt + g_node_id;
      sprintf(socket_name,"%s://eth0:%d",TPORT_TYPE,port);
#else
      if(ISCLIENT)
        port = TPORT_PORT + j + _node_cnt;
      else
        port = TPORT_PORT + j;
      sprintf(socket_name,"%s://eth0:%d",TPORT_TYPE,port);
#endif
      printf("Sock[%ld] Binding to %s %d\n",s_cnt,socket_name,g_node_id);
      rc = s[s_cnt++].sock.bind(socket_name);
      if(rc < 0) {
        printf("Bind Error: %d %s\n",errno,strerror(errno));
        assert(false);
      }
    }
#if SET_AFFINITY
  if(!ISCLIENT) {
    for(uint64_t j = 0; j < g_client_send_thread_cnt; j++) {
#if TPORT_TYPE_IPC
      port = j + _node_cnt;
      sprintf(socket_name,"%s://node_%d_%d%s",TPORT_TYPE,port,g_node_id,TPORT_PORT);
#else
      port = TPORT_PORT + j + _node_cnt*2;
      sprintf(socket_name,"%s://eth0:%d",TPORT_TYPE,port);
#endif
      printf("Sock[%ld] Binding to %s %d\n",s_cnt,socket_name,g_node_id);
      rc = s[s_cnt++].sock.bind(socket_name);
      if(rc < 0) {
        printf("Bind Error: %d %s\n",errno,strerror(errno));
        assert(false);
      }

    }
  }
#endif
  // Sending ports
    for(uint64_t j = 0; j < _node_cnt; j++) {
      if(j == g_node_id) {
        s_cnt++;
        continue;
      }
#if TPORT_TYPE_IPC
      port = g_node_id;
      sprintf(socket_name,"%s://node_%d_%ld%s",TPORT_TYPE,port,j,TPORT_PORT);
#elif !SET_AFFINITY
      port = TPORT_PORT + g_node_id * _node_cnt + j;
			sprintf(socket_name,"%s://eth0;%s:%d",TPORT_TYPE,ifaddr[j],port);
#else
      if(ISCLIENTN(j))
        port = TPORT_PORT + g_node_id + _node_cnt;
      else
        port = TPORT_PORT + g_node_id;
			sprintf(socket_name,"%s://eth0;%s:%d",TPORT_TYPE,ifaddr[j],port);
#endif
      printf("Sock[%ld] Connecting to %s %d -> %ld\n",s_cnt,socket_name,g_node_id,j);
      rc = s[s_cnt++].sock.connect(socket_name);
      if(rc < 0) {
        printf("Connect Error: %d %s\n",errno,strerror(errno));
        assert(false);
      }
    }

#if SET_AFFINITY
  if(ISCLIENT) {
    for(uint64_t i = 0; i < s_thd; i++) {
      for(uint64_t j = g_server_start_node; j < g_server_start_node + g_servers_per_client; j++) {
#if TPORT_TYPE_IPC
        port = _node_cnt + i;
        sprintf(socket_name,"%s://node_%d_%ld%s",TPORT_TYPE,port,j,TPORT_PORT);
#else
        port = TPORT_PORT + _node_cnt*2 + i;
        sprintf(socket_name,"%s://eth0;%s:%d",TPORT_TYPE,ifaddr[j],port);
#endif
        printf("Sock[%ld] Connecting to %s %d -> %ld\n",s_cnt,socket_name,g_node_id,j);
        rc = s[s_cnt++].sock.connect(socket_name);
        if(rc < 0) {
          printf("Connect Error: %d %s\n",errno,strerror(errno));
          assert(false);
        }
      }
    }
  } 
#endif

	fflush(stdout);
  //assert(s_cnt == _sock_cnt);

	if(rc < 0) {
		printf("Bind Error: %d %s\n",errno,strerror(errno));
	}
}

uint64_t Transport::get_node_id() {
	return _node_id;
}

void Transport::send_msg(uint64_t sid, uint64_t dest_id, void * sbuf,int size) {
  uint64_t starttime = get_sys_clock();
  uint64_t id;
#if !SET_AFFINITY
  id = dest_id;
#else
  if(ISCLIENT) {
    if(dest_id >= g_server_start_node && dest_id < g_server_start_node + g_servers_per_client) {
      id = _node_cnt * 2 + (sid-g_client_thread_cnt) * g_servers_per_client + (dest_id - g_server_start_node);
    } else {
        id = _node_cnt + dest_id;
    }
  }
  else {
      id = g_client_send_thread_cnt + _node_cnt + dest_id;
  }
#endif
  uint64_t idx = id; 
  assert(idx < _sock_cnt);

	void * buf = nn_allocmsg(size,0);
	memcpy(buf,sbuf,size);
  int rc = -1;
  DEBUG("Sending batch of %d bytes to node %ld on socket %ld\n",size,dest_id,idx);
  //int attempts = 0;

  while(rc < 0 && !simulation->is_done()) {
    rc= s[idx].sock.send(&buf,NN_MSG,NN_DONTWAIT);
    // Check for a send error
    /*
    if(rc < 0 || rc != size) {
      if(attempts % 100 == 0)
        printf("send %d bytes -> %ld on sock %ld Error: %d %s; Time: %f\n",size,dest_id,idx,errno,strerror(errno),simulation->seconds_from_start(get_sys_clock()));
      sleep(10);
    }
    attempts++;
    */
  }
  //assert(rc == size);
	//int rc= s[idx].sock.send(&sbuf,NN_MSG,0);
  //nn_freemsg(sbuf);
  DEBUG("Batch of %d bytes sent to node %ld on socket %ld\n",size,dest_id,idx);

  INC_STATS(_thd_id,msg_send_time,get_sys_clock() - starttime);
  INC_STATS(_thd_id,msg_send_cnt,1);
}

// Listens to socket for messages from other nodes
std::vector<Message*> Transport::recv_msg() {
	int bytes = 0;
	void * buf;
  uint64_t starttime = get_sys_clock();
  std::vector<Message*> msgs;
	
	for(uint64_t i=0;i<_sock_cnt;i++) {
		bytes = s[rr++ % _sock_cnt].sock.recv(&buf, NN_MSG, NN_DONTWAIT);

    if(rr == UINT64_MAX)
      rr = 0;

		if(bytes <= 0 && errno != 11) {
		  printf("Recv Error %d %s\n",errno,strerror(errno));
			nn::freemsg(buf);	
		}

		if(bytes>0)
			break;
	}

    /*
	if(bytes < 0 && errno != 11) {
		printf("Recv Error %d %s\n",errno,strerror(errno));
	}
  */
	// Discard any messages not intended for this node
	if(bytes <= 0 ) {
    return msgs;
	}

	// Calculate time of message delay
  /*
	ts_t time;
	memcpy(&time,&((char*)buf)[bytes-sizeof(ts_t)],sizeof(ts_t));
  */

	ts_t time2 = get_sys_clock();

  INC_STATS(_thd_id,msg_recv_time, time2 - starttime);
	INC_STATS(_thd_id,msg_recv_cnt,1);

	starttime = get_sys_clock();

  //rem_qry_man.unmarshall(buf,bytes);
  msgs = Message::create_messages((char*)buf);
  DEBUG("Batch of %d bytes recv from node %ld; Time: %f\n",bytes,msgs[0]->return_node_id,simulation->seconds_from_start(get_sys_clock()));

	//DEBUG("Msg delay: %d->%d, %d bytes, %f s\n",return_id,
  //          dest_id,bytes,((float)(time2-time))/BILLION);
	nn::freemsg(buf);	

	INC_STATS(_thd_id,msg_unpack_time,get_sys_clock()-starttime);
  return msgs;
}

void Transport::simple_send_msg(int size) {
	void * sbuf = nn_allocmsg(size,0);

	ts_t time = get_sys_clock();
	memcpy(&((char*)sbuf)[0],&time,sizeof(ts_t));

  int rc = -1;
  while(rc < 0 ) {
    //rc = s[(g_node_id + 1) % _node_cnt].sock.send(&sbuf,NN_MSG,0);
  if(g_node_id == 0)
    rc = s[3].sock.send(&sbuf,NN_MSG,0);
  else
    rc = s[2].sock.send(&sbuf,NN_MSG,0);
	}
}

uint64_t Transport::simple_recv_msg() {
	int bytes;
	void * buf;

  if(g_node_id == 0)
		bytes = s[1].sock.recv(&buf, NN_MSG, NN_DONTWAIT);
  else
		bytes = s[0].sock.recv(&buf, NN_MSG, NN_DONTWAIT);
    if(bytes <= 0 ) {
      if(errno != 11)
        nn::freemsg(buf);	
      return 0;
    }

	ts_t time;
	memcpy(&time,&((char*)buf)[0],sizeof(ts_t));
	//printf("%d bytes, %f s\n",bytes,((float)(get_sys_clock()-time)) / BILLION);

	nn::freemsg(buf);	
	return bytes;
}

