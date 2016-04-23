#!/usr/bin/env python

# Copyright (C) 2016 by Maciej Serylak
# Licensed under the Academic Free License version 3.0
# This program comes with ABSOLUTELY NO WARRANTY.
# You are free to modify and redistribute this code as long
# as you do not remove the above attribution and reasonably
# inform receipients that you have modified the original work.

import time
import os
import sys
import optparse as opt
import numpy as np
import subprocess
import multiprocessing

if __name__=="__main__":
    # Parsing the command line options.
    usage = "Usage: %prog {options}"
    cmdline = opt.OptionParser(usage)
    cmdline.formatter.max_help_position = 100 # increase space reserved for option flags (default 24), trick to make the help more readable
    cmdline.formatter.width = 250 # increase help width from 120 to 200
    cmdline.add_option("--iteration", "-i", type = "string", dest = "packetsPerIteration", metavar = "<packetsPerIteration>", default = "128" , help = "Select number of packets per iteration of the chunker. Default: 128")
    cmdline.add_option("--frequency", "-f", type = "string", dest = "frequency", metavar = "\"<startChannel> <numberChannels>\"", default = "0 1024", help = "Select start and number of channels. Default: \"0 1024\"")
    cmdline.add_option("--address", "-a", type = "string", dest = "ipAddressPort", metavar = "\"<ip_address> <port_number>\"", default = "192.168.202.13 8622", help = "Select IP and port number K7Server will attach to. Default: \"192.168.202.13 8622\"")
    cmdline.add_option("--cpu", "-c", type = "int", dest = "cpuID", metavar = "<cpuID>", default = "2", help = "Select CPU affinity for the K7Server. Default: 2")
    cmdline.add_option("--output", "-o", type = "string", dest = "fileName", metavar = "<fileName>", help = "Give name for the K7Server configuration XML file. Default: K7_Server_<channel>_<date>.xml")
    cmdline.add_option('--norun', dest = 'doNotRun', action = "store_true", help = 'Generate XML config file and exit without running the K7Server.')
    # Reading command options.
    (opts, args) = cmdline.parse_args()
    # Define constans and variables.
    packetSize = 8208
    maxBufferChunks = 10240
    channelsPerPacket = 1024
    startTime = time.strftime("D%Y%m%dT%H%M%S", time.gmtime())
    packetsPerIteration = opts.packetsPerIteration
    # Calculate maximal chunk size (by default it is 128 packets * 8208 bytes per packet).
    maxChunkSize = int(packetsPerIteration) * packetSize
    # Calculate maximal buffer size (by default it is 10240 chunks)
    maxBufferSize = maxChunkSize * maxBufferChunks
    address = opts.ipAddressPort.split()
    ipNumber = address[0]
    portNumber = address[1]
    frequency = opts.frequency.split()
    frequency = [int(i) for i in frequency]
    startChannel = frequency[0]
    numberChannels = frequency[1]
    endChannel = startChannel + numberChannels - 1
    if ( startChannel < 0 or startChannel >= endChannel or endChannel <= startChannel or endChannel < 0 or endChannel >= 1024 ):
        print "Selected spectral range exceeds available range!"
        sys.exit(0)
    if not opts.fileName:
        fileName = "K7_Server_" + str(numberChannels).zfill(4) + "_" + startTime + ".xml"
    else:
        fileName = opts.fileName
    cpuID = opts.cpuID
    cpuMaxNumber = multiprocessing.cpu_count()
    if ( cpuID >= cpuMaxNumber or cpuID < 0 ):
        print "Selected cpuID greater than available! Resetting to cpuID = 2"
        cpuID = 2
    # Writing K7Server configuration to XML file.
    output = open(fileName, "w")
    output.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
    output.write("<!DOCTYPE pelican>\n")
    output.write("<configuration version=\"1.0\">\n")
    output.write("\n")
    output.write("  <server>\n")
    output.write("\n")
    output.write("    <chunkers>\n")
    output.write("      <K7Chunker>\n")
    output.write("        <connection host=\"" + ipNumber + "\" port=\"" + portNumber + "\"/>\n")
    output.write("        <channelsPerPacket value=\"" + str(channelsPerPacket) + "\"/>\n")
    output.write("        <udpPacketsPerIteration value=\"" + packetsPerIteration + "\"/>\n")
    output.write("        <stream channelStart=\"" + str(startChannel) + "\" channelEnd=\"" + str(endChannel) + "\"/>\n")
    output.write("        <data type=\"SpectrumDataSetStokes\"/>\n")
    output.write("      </K7Chunker>\n")
    output.write("    </chunkers>\n")
    output.write("\n")
    output.write("    <buffers>\n")
    output.write("      <SpectrumDataSetStokes>\n")
    output.write("         <buffer maxSize=\"" + str(maxBufferSize) + "\" maxChunkSize=\"" + str(maxChunkSize) + "\"/>\n")
    output.write("       </SpectrumDataSetStokes>\n")
    output.write("     </buffers>\n")
    output.write("\n")
    output.write("   </server>\n")
    output.write("\n")
    output.write("</configuration>\n")
    output.close()
    # Preparing full command and running it (or not) on the system.
    if opts.doNotRun:
        print ""
        print "Generating configuration XML file " + fileName + " without runnig K7Server."
        print ""
    else:
        schedtoolCommand = "schedtool -a " + str(cpuID) + " -e K7Server --config=" + fileName + " 2>&1 | tee -a " + "K7_Server_" + str(numberChannels).zfill(4) + "_" + startTime + ".log"
        output = open("K7_Server_" + str(numberChannels).zfill(4) + "_" + startTime + ".log", "w")
        output.write("\n")
        output.write("Using following setup:\n")
        output.write("connection host: " + ipNumber + "\n")
        output.write("connection port: " + portNumber + "\n")
        output.write("channelsPerPacket: " + str(channelsPerPacket) + "\n")
        output.write("udpPacketsPerIteration: " + packetsPerIteration + "\n")
        output.write("stream channelStart: " + str(startChannel) + "\n")
        output.write("stream channelEnd: " + str(endChannel) + "\n")
        output.write("buffer maxSize: " + str(maxBufferSize) + "\n")
        output.write("buffer maxChunkSize: " + str(maxChunkSize) + "\n")
        output.write("\n")
        output.close()
        print ""
        print "Using following setup:"
        print "connection host:", ipNumber
        print "connection port:", portNumber
        print "channelsPerPacket:", str(channelsPerPacket)
        print "udpPacketsPerIteration:", packetsPerIteration
        print "stream channelStart:", str(startChannel)
        print "stream channelEnd:", str(endChannel)
        print "buffer maxSize:", str(maxBufferSize)
        print "buffer maxChunkSize:", str(maxChunkSize)
        print ""
        os.system(schedtoolCommand)
