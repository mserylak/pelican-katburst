#!/usr/bin/env python

import sys
import socket
import struct

ip = "192.168.202.13"
port = 8622
#ip = "127.0.0.1"
#port = 9999

packetSize = 8208
headerSize = 16
packetsPerSecond = 390625
drvAccumulationNumber = 0

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind((ip, port))

data, addr = sock.recvfrom(packetSize)
initUTCtimestamp = struct.unpack("<Q", data[0:8])[0]
initAccumulationNumber = struct.unpack("<L", data[8:12])[0]
initAccumulationRate = struct.unpack("<L", data[12:16])[0]
drvAccumulationNumber = initAccumulationNumber
print "first", initUTCtimestamp, initAccumulationNumber, initAccumulationRate
while True:
  data, addr = sock.recvfrom(packetSize)
  UTCtimestamp = struct.unpack("<Q", data[0:8])[0]
  accumulationNumber = struct.unpack("<L", data[8:12])[0]
  accumulationRate = struct.unpack("<L", data[12:16])[0]
  #print "drvAccumulationNumber", drvAccumulationNumber
  drvAccumulationNumber = drvAccumulationNumber + accumulationRate
  if (drvAccumulationNumber >= packetsPerSecond):
    drvAccumulationNumber = drvAccumulationNumber % packetsPerSecond
    initUTCtimestamp = initUTCtimestamp + 1

  if ((drvAccumulationNumber - accumulationNumber != 0) or (initUTCtimestamp - UTCtimestamp != 0)):
    print UTCtimestamp, accumulationNumber, accumulationRate, initUTCtimestamp, drvAccumulationNumber, accumulationRate
    sock.close()
    sys.exit("Packets out of sync!")

  print UTCtimestamp, accumulationNumber, accumulationRate, initUTCtimestamp, drvAccumulationNumber, accumulationRate
