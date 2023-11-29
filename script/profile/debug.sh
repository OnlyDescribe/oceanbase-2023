#!/bin/bash
PID=$(pidof observer)
profile_dir=$(pwd)
# cluster_home_path="/data/obcluster"
cluster_home_path="/home/oldcb/Desktop/ob/data/obcluster"

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
# python3 deploy_bootstrap.py --cluster-home-path="/home/oldcb/Desktop/ob/data/obcluster"
# python3 deploy_bootstrap.py --cluster-home-path="/data/obcluster"

# rm /home/oldcb/Desktop/ob/data/obcluster/bin/observer
# cp /home/oldcb/Desktop/ob/oceanbase-2023/build_debug/src/observer/observer /home/oldcb/Desktop/ob/data/obcluster/bin/