#!/usr/bin/env bash
startTime=`date +"D%Y%m%dT%H%M%S"`
numactl -C 12-23 K7UdpPipeline --config=K7_UdpPipeline.xml | tee udpPipeline_${startTime}.log
