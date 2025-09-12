cd /mnt/Github/LinuxFanControl/build/src
[[ -z $(pidof lfcd) ]] && ./daemon/lfcd &
./gui/lfc-gui
