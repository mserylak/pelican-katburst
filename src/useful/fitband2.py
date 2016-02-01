#!/usr/bin/python
import string, sys
import numpy as np
import matplotlib.pyplot as plt
import pickle
import os
plt.ioff()
frequency=[]
spectrumrms=[]
spectrummean=[]
averagespectrum=[]
# If no arguments were given, print a helpful message
if len(sys.argv)!=6:
    print 'Usage: fitband filein start_freq chbw polynomial_degree decimation_factor'
    sys.exit(0)

# Get filenames
filein=sys.argv[1]
fileoutname,extension=os.path.splitext(filein)
fileout = fileoutname+'.bp'
print fileoutname, fileout
start_freq = float(sys.argv[2])
bw = float(sys.argv[3])
poldeg=int(sys.argv[4])
decim=int(sys.argv[5])
#open file for read to obtain channelwise RMS
f = open(filein, 'r')
linenumber=1
line=f.readline()
data=line.split()
Nchans = len(data)
tf=np.array(map(float,data))
tf=tf/decim
while (linenumber<5000):
    line=f.readline()
    if line=="":
        break
    data=line.split()
    timefreq=np.array(map(float,data))
    timefreq=timefreq/decim
    linenumber+=1
    print tf.size, timefreq.size
    tf=np.vstack([tf,timefreq])
f.close()

fullrms=np.std(tf)#,axis=0)
fullmedian=np.median(tf)#,axis=1)
#print 'fullrms', fullrms, len(fullrms)
trustedrms=np.mean(fullrms)
trustedmedian=np.mean(fullmedian)
print trustedrms,trustedmedian

f = open(filein, 'r')
nspectra = 1
for line in f:
    thisline=line.split()
    thisspectrum = map (float, thisline)
# Compute the rms of the spectrum
    intensity = np.array(thisspectrum)
    intensity = intensity/decim
    if nspectra==1:
        print Nchans
        sum = intensity
        for i in range (0, Nchans):
            frequency.append(float(start_freq+i*bw))
#        spectrumrms.append(np.std(intensity))
#        spectrummean.append(np.mean(intensity))
        nspectra = nspectra +1
    else:
        if Nchans == len(thisspectrum):
            sum = sum + intensity
            if np.mod(nspectra,1000)==0: 
                print "read spectrum ",nspectra 
#            spectrumrms.append(np.std(intensity))
#            spectrummean.append(np.mean(intensity))
            nspectra = nspectra +1

#    print np.std(intensity), np.mean(intensity)
f.close()

# Find number of lofar subbands (30 or 31 for now)
Nsubbands = 1
if np.mod(Nchans-1,30)==0:
    Nsubbands=30
elif np.mod(Nchans-1,31)==0:
    Nsubbands=31    


print Nsubbands, nspectra
# Do the fit
x = np.array(frequency)-frequency[0]
y = sum / nspectra
z = np.polyfit(x,y,'1')
thefit=np.poly1d(z)
mymedian=np.median(y)
residual=np.empty( (Nchans) )
residual=y-thefit(x)
myrms=np.std(residual)
print 'Residual RMS is:', myrms
plt.plot(x, y, 'b.')
for iter in range (1,10):
    rejected = 0
    for i in range (0,Nchans):
        residual[i]=y[i]-thefit(x[i])
        if abs(residual[i])>=2*myrms:
            y[i]=thefit(x[i])
            rejected+=1
            print 'RMS ', myrms
            print 'Rejecting ', rejected
    z = np.polyfit(x,y,poldeg)
    thefit=np.poly1d(z)
    myrms=np.std(y)
residual=y-thefit(x)
myrms=np.std(residual)
print 'Rejected %4.2f percent of all channels' % (100.*float(rejected)/float(Nchans))
print z
print ''
print ''
outfile = open(fileout, 'w')
outfile.write("# Bandpass file from %s probably of %s subbands\n" % (filein, Nsubbands))
print Nchans
outfile.write("%s \n" % (str(Nchans)))
print frequency[0]
outfile.write("%s \n" % (str(frequency[0])))
print bw
print trustedrms
print np.median(thefit(x))
channelwidth=str(bw)
outfile.write("%s \n" % (channelwidth[0:10]))
outfile.write("%s \n" % (str(trustedrms)))
#outfile.write("%s \n" % (str(np.median(thefit(x)))))
# Write out the coefficients
for i in range (0,poldeg+1):
    print z[poldeg-i]
    outfile.write("%s \n" % (str(z[poldeg-i])))
plt.plot(x, thefit(x), 'k-', x, thefit(x)+2.*myrms, 'r-', x, residual,'g-')
#plt.ylim(-2*myrms,mymedian+10*myrms)
plt.xlabel('MHz')
plt.ylabel(myrms)
plt.show()

