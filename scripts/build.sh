
#sudo apt-get install -y libsensors-dev cmake g++

cd ../src/daemon
cmake -B build -S .
cmake --build build -j

./build/lfcd     # startet Socket: /tmp/lfcd.sock