#!/usr/bin/env python

import socket
import struct

ip = "127.0.0.1"
port = 9999

packet_size = 8208
header_size = 16

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind((ip, port))

while True:
  data, addr = sock.recvfrom(packet_size)
  UTCtimestamp = struct.unpack(">Q", data[0:8])[0]
  print "UTC timestamp", UTCtimestamp
  accumulationNumber = struct.unpack(">L", data[8:12])[0]
  print "Accumulation number", accumulationNumber
  accumulationRate = struct.unpack(">L", data[12:16])[0]
  print "Accumulation rate", accumulationRate
  print UTCtimestamp,accumulationNumber,accumulationRate
  for j in range(header_size, packet_size, 8):
    payload_XX_Re = struct.unpack(">h", data[j:j+2])[0]
    payload_YY_Re = struct.unpack(">h", data[j+2:j+4])[0]
    payload_XY_Re = struct.unpack(">h", data[j+4:j+6])[0]
    payload_XY_Im = struct.unpack(">h", data[j+6:j+8])[0]
    print payload_XX_Re, payload_YY_Re, payload_XY_Re, payload_XY_Im
  print ""

sock.close()

