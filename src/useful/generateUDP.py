#!/usr/bin/env python

import struct
import socket
import time
import numpy as np

HOST = "192.168.202.13"
PORT = 8622
#HOST = "127.0.0.1"
#PORT = 9999

samplingTime = 2.56e-6
packetsPerSecond = 390625
UTCtimestamp = int(time.time())
accumulationNumber = 0
accumulationRate = 256
sleepTime = accumulationRate * samplingTime
headerValues = (UTCtimestamp, accumulationNumber, accumulationRate)
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
while True:
    accumulationNumber += accumulationRate
    if (accumulationNumber >= packetsPerSecond):
        accumulationNumber = accumulationNumber % packetsPerSecond
        UTCtimestamp += 1
    headerValues = (UTCtimestamp, accumulationNumber, accumulationRate)
    packedHeaderValues = struct.pack("<QLL", *headerValues)
    mu, sigma = 20, 3
    dataValues = np.random.normal(mu, sigma, 4096)
    dataValues = np.rint(dataValues)
    packedDataValues = struct.pack("<4096h", *dataValues)
    PACKETDATA = ''.join([packedHeaderValues, packedDataValues])
    s.sendto(PACKETDATA, (HOST, PORT))
    time.sleep(sleepTime)
