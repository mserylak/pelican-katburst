#include <QDebug>
#include <QList>
#include "DedispersionModule.h"
#include "DedispersionParameters.h"
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include "GPU_MemoryMap.h"
#include "DedispersionBuffer.h"
#include "WeightedSpectrumDataSet.h"
#include "GPU_Job.h"
#include "GPU_Kernel.h"
#include "GPU_Param.h"
#include "GPU_NVidia.h"
#include "GPU_Manager.h"
#include <fstream>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/math/distributions/chi_squared.hpp>
#include <boost/random/variate_generator.hpp>

extern "C" void cacheDedisperseLoop ( float *outbuff, long outbufSize, float *buff, float mstartdm,
                                      float mdmstep, int tdms, const int numSamples,
                                      const float* dmShift, const int i_maxshift,
                                      const int i_nchans );

namespace pelican {
namespace ampp {

/*
 * @details DedispersionModule
 * <DedispersionModule>
 *   <timeBinsPerBufferPow2 value="12"/>
 *   <sampleTime seconds="0.00008192"/> Sample time of incoming data.
 *   <frequencyChannel1 MHz="1827.859375"/> The frequency of the first channel (lowest or highest depending on channelBandwidth +ve or -ve).
 *   <channelBandwidth MHz="-0.390625"/> The width of each frequency channel.
 *   <dedispersionSamples value="15"/> Number of dedispersion steps.
 *   <dedispersionStepSize value="1.0"/> Size of dedispersion step size.
 *   <dedispersionMinimum value="0.0"/> Minimal dedispersion value.
 *   <numberOfBuffers value="2"/>
 * </DedispersionModule>
 */

DedispersionModule::DedispersionModule( const ConfigNode& config ) : AsyncronousModule(config)
{
    // Get configuration options
    //unsigned int nChannels = config.getOption("outputChannelsPerSubband", "value", "512").toUInt();
    float timeSamplesPow2 = config.getOption("timeBinsPerBufferPow2", "value", "15").toFloat();
    _numSamplesBuffer = (int) pow(2.0, timeSamplesPow2);
    _tdms = config.getOption("dedispersionSamples", "value", "1984").toUInt();
    _dmStep = config.getOption("dedispersionStepSize", "value", "0.0").toFloat();
    _dmLow = config.getOption("dedispersionMinimum", "value", "0.0").toFloat();
    if ( _dmLow < 0.0 )
    {
        _dmLow = 0.0;
    }
    _foff = config.getOption("channelBandwidth", "MHz", "1.0").toDouble();
    _invert = ( _foff >= 0 ) ? 1 : 0;

    _fch1 = 0.0;
    _fch1 = config.getOption("frequencyChannel1", "MHz").toFloat();

    unsigned int maxBuffers = config.getOption("numberOfBuffers", "value", "2").toUInt();
    if ( maxBuffers < 1 )
    {
        throw(QString("DedispersionModule: Must have at least one buffer"));
    }

    // Setup the data buffers and objects required for each job.
    for ( unsigned int i=0; i < maxBuffers; ++i )
    {
        _buffersList.append( new DedispersionBuffer(_numSamplesBuffer, 1, _invert) );
        GPU_Job tmp;
        _jobs.append( tmp );
        DedispersionSpectra tmp2;
        _dedispersionData.append( tmp2 );
    }
    _jobBuffer.reset( &_jobs );
    _buffers.reset( &_buffersList );
    _dedispersionDataBuffer.reset( &_dedispersionData );
    _currentBuffer = _buffers.next();
}

DedispersionModule::~DedispersionModule()
{
    waitForJobCompletion();
    _cleanBuffers();
}

void DedispersionModule::waitForJobCompletion()
{
    while ( ! ( _kernels.allAvailable() && _jobBuffer.allAvailable() ) )
    {
        usleep(10);
    }
    AsyncronousModule::waitForJobCompletion();
}

void DedispersionModule::_cleanBuffers()
{
    // Clean up kernels.
    foreach ( DedispersionKernel* k, _kernelList )
    {
        delete k;
    }
    _kernelList.clear();
    // Clean up the data buffers memory.
    foreach ( DedispersionBuffer* b, _buffersList )
    {
        delete b;
    }
    _buffersList.clear();
}

void DedispersionModule::resize( const SpectrumDataSet<float>* streamData )
{
    unsigned int nChannels = streamData->nChannels();
    unsigned int nSubbands = streamData->nSubbands();
    unsigned int nPolarisations = streamData->nPolarisations();
    //unsigned sampleSize = nSubbands * nChannels * nPolarisations;
    unsigned sampleSize = nSubbands * nChannels;

    if ( sampleSize != _currentBuffer->sampleSize() )
    {
        unsigned maxBuffers = _buffersList.size();
        unsigned maxSamples = _currentBuffer->maxSamples();
        waitForJobCompletion();
        _cleanBuffers();
        // Set up the time/freq buffers.
        for( unsigned int i=0; i < maxBuffers; ++i )
        {
            _buffersList.append( new DedispersionBuffer(maxSamples, sampleSize, _invert) );
        }
        _buffers.reset( &_buffersList );
        _currentBuffer = _buffers.next();

        _nChannels = nChannels * nSubbands;
        // Generate the noise template to replace flagged data
        _noiseTemplate.resize( _numSamplesBuffer * _nChannels );
        typedef boost::mt19937                     ENG;    // Mersenne Twister.
        typedef boost::normal_distribution<float>  DIST;   // Normal Distribution.
        typedef boost::variate_generator<ENG,DIST> GEN;    // Variate Generator.

        ENG  eng;
        DIST dist(0,1);
        GEN  gen(eng,dist);

        for (int i = 0; i < _numSamplesBuffer * _nChannels; ++i)
        {
            // Chi-squared distribution with 4 degrees of freedom, like raw sampled total power
            // set the mean to zero and rms to 1 (mean=4 and variance=8 -> rms = 2sqrt(2)).
            do
            {
                float x1 = gen();
                float x2 = gen();
                float x3 = gen();
                float x4 = gen();
                _noiseTemplate[i] = (x1*x1 + x2*x2 + x3*x3 + x4*x4 - 4.0)/2.828427; 
            } while (_noiseTemplate[i] > 3.5);
            //_noiseTemplate[i] = (x1*x1 - 4.0)/2.828427; 
        }
        // Calculate dispersion measure shifts.
        _dmshifts.clear();
        for ( int c = 0; c < _nChannels; ++c )
        {
            float val= 4148.741601 * ((1.0 / (_fch1 + (_foff * c)) / (_fch1 + (_foff * c))) - (1.0 / _fch1 / _fch1));
            (_invert) ? _dmshifts.insert(_dmshifts.begin(), 1, val) : _dmshifts.push_back(val);
        }
        _tsamp = streamData->getBlockRate();
        _maxshift = (_invert)? -((_dmLow + _dmStep * (_tdms - 1)) * _dmshifts[0])/_tsamp:((_dmLow + _dmStep * (_tdms - 1)) * _dmshifts[_nChannels - 1])/_tsamp;
        // Calculate the remaining number of samples between the full buffer minus maxshift and what is being dedispersed:
        _remainingSamples = (_numSamplesBuffer - _maxshift) % (NUMREG * DIVINT);
        std::cout << "DedispersionModule::resize(): maxSamples = " << maxSamples << std::endl;
        std::cout << "DedispersionModule::resize(): dmLow = " << _dmLow << std::endl;
        //std::cout << "DedispersionModule::resize(): mshift = " << _dmLow + _dmStep * (_tdms - 1) * _dmshifts[_nChannels - 1] << std::endl;
        std::cout << "DedispersionModule::resize(): dmStep = " << _dmStep << std::endl;
        std::cout << "DedispersionModule::resize(): tdms = " << _tdms << std::endl;
        std::cout << "DedispersionModule::resize(): foff = " << _foff << std::endl;
        std::cout << "DedispersionModule::resize(): fch1 = " << _fch1 << std::endl;
        std::cout << "DedispersionModule::resize(): maxShift = " << _maxshift << std::endl;
        std::cout << "DedispersionModule::resize(): remainingSamples = " << _remainingSamples << std::endl;
        std::cout << "DedispersionModule::resize(): tsamp = " << _tsamp << std::endl;
        std::cout << "DedispersionModule::resize(): blob nChannels= " << nChannels << std::endl;
        std::cout << "DedispersionModule::resize(): nTimeBlocks= " << streamData->nTimeBlocks() << std::endl;
        if( (int)maxSamples <= _maxshift )
        {
            throw QString("DedispersionModule: maxshift requirements (%1) are bigger than the number of samples (%2)").arg(_maxshift).arg(maxSamples);
        }
        // Reset kernels.
        for ( unsigned int i = 0; i < maxBuffers; ++i )
        {
            DedispersionKernel* kernel = new DedispersionKernel( _dmLow, _dmStep, _tsamp, _tdms, _nChannels, _maxshift + _remainingSamples, _numSamplesBuffer );
            _kernelList.append( kernel ); 
            kernel->setDMShift( _dmshifts );
        }
        _kernels.reset( &_kernelList );
    }
}

void DedispersionModule::dedisperse( DataBlob* incoming )
{
    dedisperse( dynamic_cast<WeightedSpectrumDataSet*>(incoming) );
}

void DedispersionModule::dedisperse( WeightedSpectrumDataSet* weightedData )
{
    // transfer weighted data to host memory buffer
    //
    // --------- copy data statitistics -----------------
    // Index is the place to write the mean and rms given the current chunk. It 
    // takes into account the fact that for buffers after the first, the first 
    // values are just copied from the maxshift part of the previous buffers
    //int index = ( _counter == 0 ) ? _nsamp/nTimeBlocks 
    //    : _nsamp / nTimeBlocks + floor( (float)_maxshift/(float) nTimeBlocks);

    //_means[_counter % _stages][index] = blobMean * nChannels;
    //_rmss[_counter % _stages][index] = blobRMS * nChannels;

    // --------- copy spectrum data to buffer -----------------
  SpectrumDataSetStokes* streamData = 
    static_cast<SpectrumDataSetStokes*>(weightedData->dataSet());
  
  _blobs.push_back( streamData ); // keep a list of blobs to lock
  resize( streamData ); // ensure we have buffers scaled appropriately
  
  unsigned int sampleNumber = 0; // marker to indicate the number of samples succesfully 
  // transferred to the buffer from the Datablob
  unsigned int maxSamples = streamData->nTimeBlocks();
  QString tempString;
  do {
    unsigned ret = _currentBuffer->addSamples( weightedData, _noiseTemplate, &sampleNumber );
    if (0 == ret) {
      //timerStart(&_launchTimer);
      //timerStart(&_bufferTimer);
      DedispersionBuffer* next = _buffers.next();
      next->clear();
      //timerUpdate(&_bufferTimer);
      {   // lock mutex scope
        // lock here to ensure there is just a single hit on the 
        // lock mutex for each buffer
        QMutexLocker l( &lockerMutex );
        lockAllUnprotected( _blobs );
        //timerStart(&_copyTimer);
        //        std::cout << "maxshift to be copied" << std::endl;
	//        lockAllUnprotected( _currentBuffer->copy( next, _noiseTemplate, _maxshift ) );
        lockAllUnprotected( _currentBuffer->copy( next, _noiseTemplate, _maxshift + _remainingSamples, sampleNumber ) );
//        lockAllUnprotected( _currentBuffer->copy( next, _noiseTemplate, _maxshift, sampleNumber ) );
        //        std::cout << "maxshift copied" << std::endl;
        
        //timerUpdate( &_copyTimer );
        // ensure lock is maintianed for the next buffer
        // if not already marked by the maxshift copy
        if( sampleNumber != maxSamples && ! next->inputDataBlobs().contains(streamData) )
          lockUnprotected( streamData );
      }
      _blobs.clear();
      //timerStart( &_dedisperseTimer );
      QtConcurrent::run( this, &DedispersionModule::dedisperse, _currentBuffer, _dedispersionDataBuffer.next() );
      //timerUpdate( &_dedisperseTimer );
      _currentBuffer = next;
      //timerUpdate(&_launchTimer);
      //timerReport(&_launchTimer, "Launch Total");
      //timerReport(&_dedisperseTimer, "Dedispersing Time");
      //timerReport(&_bufferTimer,"bufferTimer");
      //timerReport(&_copyTimer,"copyTimer");
    }
  }
    while( sampleNumber != maxSamples );
}

void DedispersionModule::dedisperse( DedispersionBuffer* buffer, DedispersionSpectra* dataOut )
{
    // prepare the output data datablob
  /*
    float lostData = (float)buffer->numZeros()/(float)buffer->elements();
    std::cout << " lost data fraction: " << lostData << std::endl;
    if (lostData > 0.1) 
      dataOut->setLost(1);
    else
      dataOut->setLost(0);
  */
    unsigned int nsamp = buffer->numSamples() - _maxshift - _remainingSamples;
//    unsigned int nsamp = buffer->numSamples() - _maxshift;
    /*
    std::cout << nsamp << " " <<
      _tdms << " " <<
      _dmLow << " " <<
      _dmStep << " " <<
      dataOut << " " << 
      std::endl;
    */
    dataOut->resize( nsamp, _tdms, _dmLow, _dmStep );
    // Set up a job for the GPU processing kernel
    GPU_Job* job = _jobBuffer.next();
    DedispersionKernel* kernelPtr = _kernels.next();
    kernelPtr->setOutputBuffer( dataOut->data() );
    kernelPtr->setInputBuffer( buffer->getData(),
                   boost::bind( &DedispersionModule::gpuDataUploaded, this, buffer ) );
    job->addKernel( kernelPtr );
    job->addCallBack( boost::bind( &DedispersionModule::gpuJobFinished, this, job, kernelPtr, dataOut ) );
    dataOut->setInputDataBlobs( buffer->inputDataBlobs() );
    dataOut->setFirstSample( buffer->firstSampleNumber() );
    submit( job );
    //std::cout << "DedispersionModule::dedisperse(): current jobs = " << gpuManager()->jobsQueued() << std::endl;
}

void DedispersionModule::gpuJobFinished( GPU_Job* job, DedispersionKernel* kernel, DedispersionSpectra* dataOut )
{
    _kernels.unlock( kernel ); // Give up the kernel.
    if ( job->status() != GPU_Job::Failed )
    {
        job->reset();
        _jobBuffer.unlock(job); // Return the job to the pool, ready for the next.
        exportData( dataOut );  // Send out the finished data product to our customers.
    }
    else
    {
        std::cerr << "DedispersionModule::gpuJobFinished(): " << job->error() << std::endl;
        job->reset();
        _jobBuffer.unlock(job); // return the job to the pool, ready for the next
        exportCancel( dataOut );
    }
}

void DedispersionModule::gpuDataUploaded( DedispersionBuffer* buffer )
{
    _buffers.unlock(buffer);
}

void DedispersionModule::exportComplete( DataBlob* datablob )
{
    // Unlock Spectrum Data blobs.
    DedispersionSpectra* data = static_cast<DedispersionSpectra*>(datablob);
    foreach ( SpectrumDataSetStokes* d, data->inputDataBlobs() )
    {
        unlock( d );
    }
    // Unlock the dedispersion datablob.
    _dedispersionDataBuffer.unlock(data);
}

DedispersionModule::DedispersionKernel::DedispersionKernel( float start, float step, float tsamp, float tdms , unsigned nChans, unsigned maxshift, unsigned nsamples ) : _startdm( start ), _dmstep( step ), _tsamp(tsamp), _tdms(tdms), _nChans(nChans), _maxshift(maxshift), _nsamples(nsamples)
{
}

void DedispersionModule::DedispersionKernel::setDMShift( std::vector<float>& buffer )
{
    _dmShift = GPU_MemoryMap(buffer);
}

void DedispersionModule::DedispersionKernel::cleanUp()
{
    _inputBuffer.runCallBacks();
}

void DedispersionModule::DedispersionKernel::setOutputBuffer( std::vector<float>& buffer )
{
    _outputBuffer = GPU_MemoryMap(buffer);
}

void DedispersionModule::DedispersionKernel::setInputBuffer( std::vector<float>& buffer, GPU_MemoryMap::CallBackT callback )
{
    _inputBuffer = GPU_MemoryMap(buffer);
    _inputBuffer.addCallBack( callback );
}

void DedispersionModule::DedispersionKernel::run( GPU_NVidia& gpu )
{
//cache_dedisperse_loop( float *outbuff, float *buff, float mstartdm, float mdmstep )
//std::cout << " maxShift =" << _maxshift << std::endl;
//std::cout << " nchans =" << _nChans << std::endl;
//std::cout << " tsamp =" << _tsamp << std::endl;
//std::cout << " input buffer (" << gpu.devicePtr(_inputBuffer) << " ) size=" << _inputBuffer.size() << std::endl;
//std::cout << " output buffer (" << gpu.devicePtr(_outputBuffer) << ") size=" << _outputBuffer.size() << std::endl;
//std::cout << " dmShift size =" << _dmShift.size() << std::endl;
//std::cout << " nSamples =" << _nsamples << std::endl;
    cacheDedisperseLoop( (float*)gpu.devicePtr(_outputBuffer) , _outputBuffer.size(),
                         (float*)gpu.devicePtr(_inputBuffer), (_startdm/_tsamp),
                         (_dmstep/_tsamp), _tdms, _nsamples,
                         (const float*)gpu.devicePtr(_dmShift),
                         _maxshift,
                         _nChans
                       );
}

} // namespace ampp
} // namespace pelican
