#sudo apt-get install -y libsensors-dev cmake g++

cd /mnt/Github/LinuxFanControl/build
cmake -DCMAKE_BUILD_TYPE=Release ../src
cmake --build . -j
