# Compile CAB Program at CAB_SDN folder
mkdir build
cd build
cmake ../
cd ../bin

# Run CABDaemon and controller
./CABDaemon rulefile 
ryu-manager
