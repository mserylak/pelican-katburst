#!/usr/bin/env python

# Copyright (C) 2015 by Maciej Serylak
# Licensed under the Academic Free License version 3.0
# This program comes with ABSOLUTELY NO WARRANTY.
# You are free to modify and redistribute this code as long
# as you do not remove the above attribution and reasonably
# inform receipients that you have modified the original work.

import time
import os
import optparse as opt
import sys
import numpy as np
import matplotlib.pyplot as plt
from sigpyproc.Readers import FilReader
from pylab import *

# Main
if __name__=="__main__":
    #
    # Parsing the command line options
    #
    usage = "Usage: %prog --file <filterbank_file> --spectrum <number> or \n       %prog --file <filterbank_file> --block \"<number number>\" or\
\n       %prog --file <filterbank_file> --dynamic \"<number number>\" or\
\n       %prog --file <filterbank_file> --timeseries \"<number number number>\" or\
\n       %prog --file <filterbank_file> --bandpass"

    cmdline = opt.OptionParser(usage)
    cmdline.formatter.max_help_position = 50 # increase space reserved for option flags (default 24), trick to make the help more readable
    cmdline.formatter.width = 200 # increase help width from 120 to 200
    cmdline.add_option('--file', type = 'string', dest = 'file', default = '', metavar = '*.fil', help = 'Filterbank file.')
    cmdline.add_option('--spectrum', type = 'int', dest = 'spectrum', help = 'Select single spectrum to plot.')
    cmdline.add_option('--block', type = 'string', dest = 'block', help = 'Select multiple spectra to average and plot.')
    cmdline.add_option('--dynamic', type = 'string', dest = 'dynamic', help = 'Select multiple spectra to plot dynamic spectrum.')
    cmdline.add_option('--timeseries', type = 'string', dest = 'timeseries', help = 'Select single channel and range of spectra to plot the timeseries.')
    cmdline.add_option('--bandpass', dest = 'bandpass', action = "store_true", help = 'Plot total bandpass.')
    cmdline.add_option('--save', dest = 'savePlot', action = "store_true", help = 'Save plot to .png file instead of interacive plotting.')

    # reading cmd options
    (opts, args) = cmdline.parse_args()

    if not opts.file:
        cmdline.print_usage()
        sys.exit(0)

    # Define constans and variables.
    filterbankFilename = opts.file
    filterbankFile = FilReader(filterbankFilename) # read filterbank file
    #print "File %s opened." % (filterbankFilename)
    numberChannels = filterbankFile.header.nchans # get number of spectral channels
    numberSpectra = filterbankFile.header.nsamples # get number of spectra/samples

    # Only one option for plotting is allowed.
    if (opts.spectrum and opts.dynamic) or\
       (opts.spectrum and opts.block) or\
       (opts.spectrum and opts.bandpass) or\
       (opts.spectrum and opts.timeseries) or\
       (opts.dynamic and opts.block) or\
       (opts.dynamic and opts.bandpass) or\
       (opts.dynamic and opts.timeseries) or\
       (opts.block and opts.bandpass) or\
       (opts.block and opts.timeseries):
        print "Only one option allowed."
        sys.exit(0)

    print "File %s has %d channels and %d samples/spectra." % (filterbankFilename, numberChannels, numberSpectra)
    if opts.spectrum:
        spectrum = opts.spectrum
        if (spectrum >= numberSpectra):
            spectrum = numberSpectra-1
            print "Selected spectrum exceeds available number of spectra!"
        singleSpectrum = filterbankFile.readBlock(spectrum, 1) # read specific spectrum from the data
        plt.plot(singleSpectrum)
        plt.xlabel("Channel")
        plt.ylabel("Intensity (a.u.)")
        if not opts.savePlot:
          plt.show()
        else:
          baseFilterbankFilename, extFilterbankFilename = os.path.splitext(filterbankFilename)
          plt.savefig(baseFilterbankFilename + "_spectrum_" + str(spectrum) + ".png")
          sys.exit(0)
    elif opts.block:
        block = opts.block.split()
        block = np.arange(int(block[0]), int(block[1]))
        #print block[0], block[-1]
        if (block[-1] >= numberSpectra):
            block[-1] = numberSpectra-1
            print "Selected spectrum exceeds available number of spectra!"
        blockSpectrum = filterbankFile.readBlock(block[0], block[-1]) # read specific spectrum from the data
        blockBandpass = blockSpectrum.get_bandpass() # calculate bandpass from entire observation
        plt.plot(blockBandpass)
        plt.xlabel("Channel")
        plt.ylabel("Intensity (a.u.)")
        if not opts.savePlot:
          plt.show()
        else:
          baseFilterbankFilename, extFilterbankFilename = os.path.splitext(filterbankFilename)
          plt.savefig(baseFilterbankFilename + "_block_" + str(block[0]) + "-" + str(block[-1]) + ".png")
          sys.exit(0)
    elif opts.dynamic:
        dynamic = opts.dynamic.split()
        dynamic = np.arange(int(dynamic[0]), int(dynamic[1]))
        #print dynamic[0], dynamic[-1]
        if (dynamic[-1] >= numberSpectra):
            dynamic[-1] = numberSpectra-1
            print "Selected spectrum exceeds available number of spectra!"
        dynamicSpectrum = filterbankFile.readBlock(dynamic[0], dynamic[-1]) # read specific spectrum from the data
        plt.imshow(dynamicSpectrum, origin='lower', cmap = cm.hot, interpolation='nearest', aspect='auto')
        plt.xlabel("Spectrum")
        plt.ylabel("Channel")
        if not opts.savePlot:
          plt.show()
        else:
          baseFilterbankFilename, extFilterbankFilename = os.path.splitext(filterbankFilename)
          plt.savefig(baseFilterbankFilename + "_dynamic_" + str(dynamic[0]) + "-" + str(dynamic[-1]) + ".png")
          sys.exit(0)
    elif opts.timeseries:
        timeseries = opts.timeseries.split()
        channel = int(timeseries[0])
        begin = int(timeseries[1])
        end = int(timeseries[2])
        #print begin,end,channel
        if (end >= numberSpectra):
            end = numberSpectra-1
            print "Selected spectrum exceeds available number of spectra!"
        if (channel >= numberChannels):
            channel = numberChannels-1
            print "Selected channel exceeds available number of channels!"
        timeseriesChannel = filterbankFile.getChan(channel)
        timeseriesChannel.shape
        timeseriesSelected = timeseriesChannel[begin:end]
        plt.plot(timeseriesSelected)
        plt.xlabel("Spectrum")
        plt.ylabel("Intensity (a.u.)")
        if not opts.savePlot:
          plt.show()
        else:
          baseFilterbankFilename, extFilterbankFilename = os.path.splitext(filterbankFilename)
          plt.savefig(baseFilterbankFilename + "_timeseries_ch" + str(channel) + "_" + str(begin) + "-" + str(end) + ".png")
          sys.exit(0)
    elif opts.bandpass:
        totalBandpass = filterbankFile.bandpass() # calculate bandpass from entire observation
        plt.plot(totalBandpass)
        plt.xlabel("Channel")
        plt.ylabel("Intensity (a.u.)")
        if not opts.savePlot:
          plt.show()
        else:
          baseFilterbankFilename, extFilterbankFilename = os.path.splitext(filterbankFilename)
          plt.savefig(baseFilterbankFilename + "_bandpass.png")
          sys.exit(0)
    else:
        print "No options provided."
        sys.exit(0)
