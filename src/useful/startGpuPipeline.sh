#!/usr/bin/env bash
startTime=`date +"D%Y%m%dT%H%M%S"`
numactl -C 12-23 K7Pipeline --config=K7_Pipeline.xml | tee GpuPipeline_${startTime}.log
