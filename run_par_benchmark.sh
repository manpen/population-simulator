#!/bin/bash

nthreads=$1
echo "Threads: $nthreads"

outdir="benchmark_threads$nthreads"
mkdir -p $outdir

time=`date +%s`
host=`hostname`
output="$outdir/$host-$time-"

defcolor="\e[39m"
green="\e[32m"

tbudget=2 # seconds

./sim_benchmark --header-only > $output
ln -sf $output benchmark-latest.csv

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

for reps in `seq 9`; do
  for states in 8 32 128; do
    for prot in clock random2; do
      for sim in batch pop pop8 distr-linear distr-tree distr-alias; do
        params="-a $sim -p $prot -d $states -t $tbudget -s $reps -r 2"
        echo -e "$green $params$defcolor"
        for thread in `seq $nthreads`; do
          bash -c "./sim_benchmark $params | tee -a $output-$thread.csv" &
        done
        wait
      done
    done
  done
done
