cd /mnt/Github/LinuxFanControl/build/Release
[[ -z $(pidmof lfcd) ]] && ./lfcd &
./lfc-gui
