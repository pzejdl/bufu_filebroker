#!/bin/sh

index="0"
lumi="1"
maxlumi="5"
run="run1000030355"
outdir="/fff/ramdisk"

mkdir "$outdir/$run"

# trap ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c() {
    echo "INFO: Ctrl-C detected..."
    mkfile "$outdir/$run/${run}_ls0000_EoR.jsn"  
    exit
}

#
# Example: mkfile "$run_ls%04d_index%06d.raw" $lumi $index
#
function mkfile()
{
    printf -v filename "$@"
    echo $filename
    touch $filename
}

while [ 1 ]; do
    while [ $index -lt $maxlumi ]; do
	#mkfile "$outdir/$run/${run}_ls%04d_index%06d.raw" $lumi $index
	#sleep 0.2
	mkfile "$outdir/$run/${run}_ls%04d_index%06d.jsn" $lumi $index
	sleep 1
	index=$((index+1))
    done
    mkfile "$outdir/$run/${run}_ls%04d_EoLS.jsn" $lumi 
    
    lumi=$((lumi+1))
    index="0"
done
