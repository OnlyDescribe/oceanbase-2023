#!/bin/bash
PID=$(pidof observer)
profile_dir=$(pwd)
cluster_home_path="/home/oldcb/Desktop/ob/data/obcluster"
# cluster_home_path="/data/obcluster"

if [ -z "$PID" ]
then
    echo "observer is not running"
else
    sudo kill -9 $PID
    sleep 1
    cd $cluster_home_path && sudo rm -rf store etc* log audit run
fi
cd $profile_dir
python3 deploy_observer.py --cluster-home-path="$cluster_home_path"
perf record -F 999 -g -p $(pidof observer) -- sleep 20 &
python3 deploy_bootstrap.py --cluster-home-path="$cluster_home_path" &
wait
perf script -i perf.data &> perf.unfold
FlameGraph/stackcollapse-perf.pl perf.unfold &> perf.folded
FlameGraph/flamegraph.pl perf.folded > perf.svg