cd /mnt/Github/LinuxFanControl/build/
[[ -z $(pidmof lfcd) ]] && ./lfcd &
./lfc-gui
