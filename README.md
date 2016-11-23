# Compile CAB Program at CAB_SDN folder
mkdir build
cd build
cmake ../
cd ../bin

# Run CABDaemon
./CABDaemon rulefile 

# Run ryu-controller
cd ../controller
ryu-manager cab_switch_cab ../config/ryu_config.ini
