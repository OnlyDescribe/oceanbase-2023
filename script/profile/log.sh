#!/bin/bash
cd /data/obcluster/log
trace=$(grep "ALTER SYSTEM BOOTSTRAP ZONE 'zone1' SERVER '127.0.0.1:2882'" observer.log | grep -o -E 'YB[0-9A-F-]+-0-0' | head -n 1)
grep --exclude='bootstrap.log' -E "$trace" * > bootstrap.log

# execute bootstrap
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'check_all_server_bootstrap_mode_match execute success'  * > analyze.log
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'STEP_2.6:wait_elect_ls execute success'  * >> analyze.log
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'STEP_2.7:prepare_bootstrap execute success'  * >> analyze.log
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'execute_bootstrap start to executor'  * >> analyze.log
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'create_all_schema execute success' * >> analyze.log
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'refresh_schema execute success' * >> analyze.log
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'wait_all_rs_in_service execute success' * >> analyze.log
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'start do_restart' * >> analyze.log
grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'finish do_restart' * >> analyze.log

# for 优化 6
# grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'start ob_service end' * >> analyze.log
# grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'start observer begin' * >> analyze.log
# grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'success to refresh server configure' * >> analyze.log
# grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'check if multi tenant synced' * >> analyze.log
# grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'check if timezone usable' * >> analyze.log
# grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'observer start service' * >> analyze.log

# 耗时久的过程
# grep --exclude='analyze.log' --exclude='bootstrap.log' -E 'cost=(1[0-9]{5,}|[2-9][0-9]{5,})' * >> analyze.log
