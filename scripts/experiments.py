import itertools
import math
# Experiments to run and analyze
# Go to end of file to fill in experiments 

# Format: [#Nodes,#Txns,Workload,CC_ALG,MPR]
fmt_tpcc = [["NODE_CNT","MAX_TXN_PER_PART","WORKLOAD","CC_ALG","MPR","THREAD_CNT","NUM_WH","MAX_TXN_IN_FLIGHT"]]
fmt_nd = [["NODE_CNT","MAX_TXN_PER_PART","WORKLOAD","CC_ALG","MPR","THREAD_CNT","NUM_WH","MAX_TXN_IN_FLIGHT","NETWORK_DELAY"]]
fmt_ycsb = [["CLIENT_NODE_CNT","NODE_CNT","MAX_TXN_PER_PART","WORKLOAD","CC_ALG","MPR","CLIENT_THREAD_CNT","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]]
fmt_nt = [["NODE_CNT","CLIENT_NODE_CNT","NETWORK_TEST"]]


#nnodes=[1,2,4,8,16,32]
nnodes=[2,4,8]
nmpr=[0,0.001,0.01,0.1]
#nmpr= range(0,6,1)
#nmpr=[1] + range(0,11,5)
nalgos=['MVCC','OCC']
#nalgos=['NO_WAIT','WAIT_DIE','TIMESTAMP','OCC','MVCC','HSTORE','HSTORE_SPEC']
nthreads=[1]
#nthreads=[1,2]
nwfs=[64]
ntifs=[8]
nzipf=[0.6]
nwr_perc=[0.5]
#ntifs=[1,4,8,16,32]
ntxn=1000000
nnet_delay=['100000UL']
#nnet_delay=['0UL','50000UL','100000UL','500000UL']
#nnet_delay=['0UL','50000UL','100000UL','500000UL','1000000UL','5000000UL']

simple = [

[1,2,10000,'YCSB','WAIT_DIE',1.0,1,1,10,0.6,0,0],


#[2,10000,'TPCC','NO_WAIT',30,2,8,16]
]

experiments_nd = [
    [n,ntxn,'TPCC',cc,m,t,wf,tif,nd] for n,m,cc,t,wf,tif,nd in itertools.product(nnodes,nmpr,nalgos,nthreads,nwfs,ntifs,nnet_delay)
]

experiments = [
    [n,ntxn,'TPCC',cc,m,t,wf,tif] for n,m,cc,t,wf,tif in itertools.product(nnodes,nmpr,nalgos,nthreads,nwfs,ntifs)
]

experiments_ycsb = [
    [n,ntxn,'YCSB',cc,m,t,tif,z,1.0-wp,wp] for n,m,cc,t,tif,z,wp in itertools.product(nnodes,nmpr,nalgos,nthreads,ntifs,nzipf,nwr_perc)
]

def test():
    fmt = fmt_ycsb
    exp = [
    [1,2,50000,'YCSB','WAIT_DIE',50,1,1,10,0.6,0.5,0.5],
    ]
    return fmt[0],exp

# Performance: throughput vs. node count
# Vary: Node count, % writes
def experiment_1():
    fmt = fmt_ycsb
    nnodes = [1,2,4,8]
    nmpr=[0,0.01,0.1,1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','HSTORE','HSTORE_SPEC']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[3,1]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.5,0.0]
    ntxn=2000000
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp] for ct,t,tif,z,wp,m,cc,n in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes)]
    return fmt[0],exp

def experiment_1_plot(summary,summary_client):
    from plot_helper import tput,abort_rate,lat
    fmt = fmt_ycsb
    nnodes = [1,2,4,8]
    nmpr=[0,0.01,0.1,1]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','HSTORE','HSTORE_SPEC']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[1,3]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.0,0.5]
    ntxn=2000000
    for wr,tif,cc,t in itertools.product(nwr_perc,ntifs,nalgos,nthreads):
        _cfg_fmt = ["CC_ALG","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[cc,ntxn,'YCSB',t,tif,nzipf[0],1.0-wr,wr]
        _title="{} {} {}% Writes {} TIF {} threads".format(cc,'YCSB',wr*100,tif,t)
        tput(nnodes,nmpr,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="MPR",title=_title)

    # x-axis: nodes, y-axis: latency
    for wr,tif,cc,t in itertools.product(nwr_perc,ntifs,nalgos,nthreads):
        _cfg_fmt = ["CC_ALG","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[cc,ntxn,'YCSB',t,tif,nzipf[0],1.0-wr,wr]
        _title="{} {} {}% Writes {} TIF {} threads".format(cc,'YCSB',wr*100,tif,t)
        lat(nnodes,nmpr,summary_client,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="MPR",title=_title)
    return


    # x-axis: nodes; one plot for each wr %
    for wr,mpr,tif in itertools.product(nwr_perc,nmpr,ntifs):
        _cfg_fmt = ["MPR","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[mpr,ntxn,'YCSB',nthreads[0],tif,nzipf[0],1.0-wr,wr]
        _title="{} {}% Writes {} MPR {} TIF".format('YCSB',wr*100,mpr,tif)
        tput(nnodes,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="CC_ALG",title=_title)
        abort_rate(nnodes,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="CC_ALG",title=_title)

# Performance: throughput vs. node count
# Vary: Node count, # server worker threads
def experiment_2():
    fmt = fmt_ycsb
    nnodes = [1]
    nmpr=[0]
    nalgos=['WAIT_DIE']
    #nalgos=['WAIT_DIE','HSTORE','HSTORE_SPEC']
    #nalgos=['WAIT_DIE','NO_WAIT','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL','TIMESTAMP']
    nthreads=[2]
    ncthreads=[4]
    ntifs=[1000]
    nzipf=[0.6]
    nwr_perc=[0.0]
    ntxn=2000000
    exp = [[int(math.ceil(n/2)) if n > 1 else 1,n,ntxn,'YCSB',cc,m,ct,t,tif,z,1.0-wp,wp] for ct,t,tif,z,wp,m,cc,n in itertools.product(ncthreads,nthreads,ntifs,nzipf,nwr_perc,nmpr,nalgos,nnodes)]
    return fmt[0],exp

def experiment_2_plot(summary):
    from plot_helper import tput,abort_rate
    fmt = fmt_ycsb
    nnodes = [1,2,4,8,16,32]
    nmpr=[0.01]
    nalgos=['NO_WAIT','WAIT_DIE','TIMESTAMP','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL']
    nthreads=[1]
    ntifs=[8]
    nzipf=[0.0,0.2,0.4,0.6,0.8]
    nwr_perc=[0.5]
    ntxn=1000000
    # x-axis: nodes; one plot for each wr %
    for z in nzipf:
        _cfg_fmt = ["MPR","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[nmpr[0],ntxn,'YCSB',nthreads[0],ntifs[0],z,1.0-nwr_perc[0],nwr_perc[0]]
        _title="{} {}% Writes {} MPR {} zipf".format('YCSB',nwr_perc[0]*100,nmpr[0],z)
        tput(nnodes,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="CC_ALG",title=_title)
        abort_rate(nnodes,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="CC_ALG",title=_title)


# Performance: throughput vs. node count
# Vary: Node count, % writes, mpr
def experiment_3():
    fmt = fmt_ycsb
    nnodes = [1,2,4,8]
    nmpr=[0,0.1,1]
    nalgos=['NO_WAIT','WAIT_DIE','TIMESTAMP','OCC','MVCC','HSTORE','HSTORE_SPEC','VLL']
    nthreads=[1]
    ntifs=[8]
    nzipf=[0.6]
    nwr_perc=[0.0,0.25,0.5]
    ntxn=1000000
    exp = [[n,ntxn,'YCSB',cc,m,t,tif,z,1.0-wp,wp] for n,m,cc,t,tif,z,wp in itertools.product(nnodes,nmpr,nalgos,nthreads,ntifs,nzipf,nwr_perc)]
    return fmt[0],exp

# Performance: throughput vs. node count
# Vary: Node count, txn in flight
def experiment_4():
    fmt = fmt_ycsb
    nnodes = [4]
    nmpr=[0.0]
    nalgos=['WAIT_DIE']
    nthreads=[4]
    ntifs=[50,100,500,1000]
    nzipf=[0.6]
    nwr_perc=[0.5]
    ntxn=1000000
    exp = [[n,ntxn,'YCSB',cc,m,t,tif,z,1.0-wp,wp] for n,m,cc,t,tif,z,wp in itertools.product(nnodes,nmpr,nalgos,nthreads,ntifs,nzipf,nwr_perc)]
    return fmt[0],exp

def experiment_4_plot(summary):
    from plot_helper import tput,abort_rate,time_breakdown
    fmt = fmt_ycsb
    nnodes = [2]
#nnodes = [1,2,4,8]
    nmpr=[0.1]
    nalgos=['NO_WAIT','WAIT_DIE','TIMESTAMP','OCC','MVCC','HSTORE','HSTORE_SPEC']
    nthreads=[1]
    ntifs=[50,100,500,1000]
    nzipf=[0.6]
    nwr_perc=[0.5]
    ntxn=1000000    
    # x-axis: nodes; one plot for each wr %
    for tif in ntifs:
        _cfg_fmt = ["MPR","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[nmpr[0],ntxn,'YCSB',nthreads[0],tif,nzipf[0],1.0-nwr_perc[0],nwr_perc[0]]
        _title="{} {}% Writes {} MPR {} zipf {} TIF".format('YCSB',nwr_perc[0]*100,nmpr[0],nzipf[0],tif)
        tput(nnodes,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="CC_ALG",title=_title)
        abort_rate(nnodes,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="NODE_CNT",vname="CC_ALG",title=_title)

    for n in nnodes:
        _cfg_fmt = ["MPR","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","NODE_CNT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[nmpr[0],ntxn,'YCSB',nthreads[0],n,nzipf[0],1.0-nwr_perc[0],nwr_perc[0]]
        _title="{} {}% Writes {} MPR {} zipf {} Nodes".format('YCSB',nwr_perc[0]*100,nmpr[0],nzipf[0],n)
        tput(ntifs,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="MAX_TXN_IN_FLIGHT",vname="CC_ALG",title=_title)
        abort_rate(ntifs,nalgos,summary,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="MAX_TXN_IN_FLIGHT",vname="CC_ALG",title=_title)

    for tif,n in itertools.product(ntifs,nnodes):
        _cfg_fmt = ["MPR","MAX_TXN_PER_PART","WORKLOAD","THREAD_CNT","NODE_CNT","MAX_TXN_IN_FLIGHT","ZIPF_THETA","READ_PERC","WRITE_PERC"]
        _cfg=[nmpr[0],ntxn,'YCSB',nthreads[0],n,tif,nzipf[0],1.0-nwr_perc[0],nwr_perc[0]]
        _title="{} {}% Writes {} MPR {} zipf {} Nodes {} TIF".format('YCSB',nwr_perc[0]*100,nmpr[0],nzipf[0],n,tif)
        time_breakdown(nalgos,summary,normalized=True,cfg_fmt=_cfg_fmt,cfg=_cfg,xname="CC_ALG",title=_title)


def network_experiment():
    fmt = fmt_nt
    nnodes = [1]
    cnodes = [1]
    ntest = ["true"]
    exp = [nnodes,cnodes,ntest]
    exp = [[n,c,t] for n,c,t in itertools.product(nnodes,cnodes,ntest)]
    return fmt[0],exp

def network_experiment_plot(all_exps,all_nodes,timestamps):
    from plot_helper import lat_node_tbls,lat_tbl
    fmt = fmt_nt
    nnodes = [1]
    cnodes = [1]
    ntest = ["true"]
    rexp = [nnodes,cnodes,ntest]
    rexp = [[n,c,t] for n,c,t in itertools.product(nnodes,cnodes,ntest)]
    for i,exp in enumerate(all_exps):
        lat_node_tbls(exp[:-1],all_nodes[i],exp[0].keys(),timestamps[i])
        lat_tbl(exp[-1],exp[-1].keys(),timestamps[i])


experiment_map = {
    'test': test,
    'experiment_1': experiment_1,
    'experiment_2': experiment_2,
    'experiment_3': experiment_3,
    'experiment_4': experiment_4,
    'network_experiment' : network_experiment,
    'experiment_1_plot': experiment_1_plot,
    'experiment_4_plot': experiment_4_plot,
    'network_experiment_plot' : network_experiment_plot,
}


# Default values for variable configurations
configs = {
    "NODE_CNT" : 2,
    "CLIENT_NODE_CNT" : 1,
    "CLIENT_THREAD_CNT" : 2,
    "CLIENT_REM_THREAD_CNT" : 1,
    "MAX_TXN_PER_PART" : 100,
    "WORKLOAD" : "YCSB",
    "CC_ALG" : "NO_WAIT",
    "MPR" : 0.0,
    "TPORT_TYPE":"\"ipc\"",
    "TPORT_TYPE_IPC":"true",
    "TPORT_PORT":"\"_.ipc\"",
    "REM_THREAD_CNT": 1,
    "THREAD_CNT": 1,
    "PART_CNT": 2,
    "NUM_WH": 2,
    "MAX_TXN_IN_FLIGHT": 1,
    "NETWORK_DELAY": '0UL',
    "NETWORK_TEST" : "false",
#YCSB
    "READ_PERC":0.5,
    "WRITE_PERC":0.5,
    "ZIPF_THETA":0.6,
}

##################
# FIXME
#################
experiments = fmt_ycsb + experiments_ycsb
config_names = fmt_ycsb[0]
