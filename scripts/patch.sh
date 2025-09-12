mainDir=/mnt/Github/LinuxFanControl
[[ $1 == "d" ]] && patchFile=lfcd.patch || patchFile=gui.patch

cd $(mainDir)/src
git apply --check -p0 $(mainDir)/$(patchFile) && git apply -p0 $(mainDir)/$(patchFile)

# git apply --reject --whitespace=fix -p0 $(mainDir)/$(patchFile)
