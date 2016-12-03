# run CAB and controller
./CABDaemon ../config/CABDaemon_config.ini ../metadata/ruleset/acl2k.rules &
pushd ../controller 
ryu-manager cab_switch_CAB &
popd

# run collector
./dump_collect.sh p3p1 &

# traffic generation
sudo ps aux | grep CABDaemon
sudo ps aux | grep ryu-manager
sudo ps aux | grep tcpdump 
read -n1 -r -p "Press Any key to Generate Traffic...(Plz validate rules)" key

sudo ./FlowGen -s blah.stats -i p3p1 -C 300000 -F 10 -f Trace_Generate/acl2k.trace --ipv4

