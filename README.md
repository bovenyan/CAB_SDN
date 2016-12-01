# Compile CAB Program at CAB_SDN folder
mkdir build
cd build
cmake ../
cd ../bin

# Run CABDaemon
./CABDaemon rulefile 

# Run ryu-controller
cd ../controller
ryu-manager cab_switch_cab_v6 ../config/ryu_config.ini

# Install client echoer module @ client machine
### this ensures that every packet sent to the client will be pump back.
cd ../trace_gen
make
lsmod | grep cecho
sudo rmmod cecho
sudo insmod ./client_echo.ko
lsmod | grep cecho

# Usecase test cast checklist
0. compile all the tools 
    mkdir build 
    cd build
    cmake ../
    make

1. get firewall rules and ipc rules
    availalble in metadata/ruleset
    verify that the ruleset has a default full cover rule

2. run trace generator to generate traces
    cd ../bin
    ./TracePrepare -c ../config/TracePrepare_config.ini -r ../metadata/ruleset/acl2k.rules

3. run CAB_DAEMON
    ./CABDaemon ../config/CABDaemon_config.ini ../metadata/ruleset/acl2k.rules
    make sure to use the same rule set

4. run dump_collect.sh at both hosts
    cd ../trace_gen
    ./dump_collect.sh p3p1         or p7p3

5. run flow generator (remember sudo)
    cd ../bin
    sudo ./FlowGen -s blah -i p3p1 -f Trace_Generate/trace-100k-0.01-20/GENtrace/ref_trace.gz --ipv4
