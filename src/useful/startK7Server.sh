#!/usr/bin/env bash
cpuMaxNum=`cat /proc/cpuinfo | grep processor | wc -l`
if [ -z "$1" ] ; then
  cpuID=2
else
  cpuID=$1
fi
if [ ${cpuID} -ge ${cpuMaxNum} ] ; then
  echo "Selected cpuID greater than available! Resetting to cpuID = 2"
  cpuID=2
fi
startTime=`date +"D%Y%m%dT%H%M%S"`
schedtool -a ${cpuID} -e K7Server --config=K7_Server.xml | tee server_${startTime}.log
