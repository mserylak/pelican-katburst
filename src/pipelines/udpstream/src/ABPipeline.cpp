#include "DedispersionModule.h"
#include "DedispersionDataAnalysis.h"
#include "DedispersionDataAnalysisOutput.h"
#include "WeightedSpectrumDataSet.h"
#include "ABDataAdapter.h"
#include "ABPipeline.h"
#include "SigprocStokesWriter.h"
#include <boost/bind.hpp>
#include <iostream>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <stdio.h>

using namespace pelican;
using namespace ampp;

// The constructor. It is good practice to initialise any pointer
// members to zero.
ABPipeline::ABPipeline(const QString& streamIdentifier)
    : AbstractPipeline(), _streamIdentifier(streamIdentifier)
{
    _rfiClipper = 0;
    _dedispersionModule = 0;
    _dedispersionAnalyser = 0;
    _counter = 0;
}

// The destructor must clean up and created modules and
// any local DataBlob's created.
ABPipeline::~ABPipeline()
{
    delete _dedispersionAnalyser;
    delete _dedispersionModule;
    delete _rfiClipper;
}

// Initialises the pipeline, creating required modules and data blobs,
// and requesting remote data.
void ABPipeline::init()
{
    ConfigNode c = config(QString("ABPipeline"));
    // history indicates the number of datablobs to keep (iterations of run())
    // it should be Dedidpersion Buffer size (in Blobs)*number of Dedispersion Buffers
    unsigned int history = c.getOption("history", "value", "10").toUInt();
    _minEventsFound = c.getOption("events", "min", "5").toUInt();
    _maxEventsFound = c.getOption("events", "max", "5").toUInt();

    // Create the pipeline modules and any local data blobs.
    _rfiClipper = (RFI_Clipper *) createModule("RFI_Clipper");
    _stokesIntegrator = (StokesIntegrator *) createModule("StokesIntegrator");
    _dedispersionModule = (DedispersionModule*) createModule("DedispersionModule");
    _dedispersionAnalyser = (DedispersionAnalyser*) createModule("DedispersionAnalyser");
    _dedispersionModule->connect( boost::bind( &ABPipeline::dedispersionAnalysis, this, _1 ) );
    _dedispersionModule->unlockCallback( boost::bind( &ABPipeline::updateBufferLock, this, _1 ) );
    _stokesData = createBlobs<SpectrumDataSetStokes>("SpectrumDataSetStokes", history);
    _stokesBuffer = new LockingPtrContainer<SpectrumDataSetStokes>(&_stokesData);
    _intStokes = (SpectrumDataSetStokes *) createBlob("SpectrumDataSetStokes");
    _weightedIntStokes = (WeightedSpectrumDataSet*) createBlob("WeightedSpectrumDataSet");

    // Request remote data.
    requestRemoteData("SpectrumDataSetStokes");
}

// Defines a single iteration of the pipeline.
void ABPipeline::run(QHash<QString, DataBlob*>& remoteData)
{
#ifdef TIMING_ENABLED
    timerStart(&_totalTime);
#endif
    // Get pointers to the remote data blob(s) from the supplied hash.
    SpectrumDataSetStokes* stokes = (SpectrumDataSetStokes*) remoteData["SpectrumDataSetStokes"];
    if( !stokes ) throw(QString("No stokes!"));
    /* to make sure the dedispersion module reads data from a lockable ring
       buffer, copy data to one */
    SpectrumDataSetStokes* stokesBuf = _stokesBuffer->next();

    _stokesIntegrator->run(stokes, _intStokes);
    *stokesBuf = *_intStokes;
    _weightedIntStokes->reset(stokesBuf);
#ifdef TIMING_ENABLED
    timerStart(&_rfiClipperTime);
#endif
    _rfiClipper->run(_weightedIntStokes);
#ifdef TIMING_ENABLED
    timerUpdate(&_rfiClipperTime);
#endif
#ifdef TIMING_ENABLED
    timerStart(&_dedispersionTime);
#endif
    _dedispersionModule->dedisperse(_weightedIntStokes);
#ifdef TIMING_ENABLED
    timerUpdate(&_dedispersionTime);
#endif
    if (0 == _counter % 100)
    {
        std::cout << _counter << " chunks processed." << std::endl;
    }
#ifdef TIMING_ENABLED
    timerUpdate(&_totalTime);
    if (0 == _counter % 1000)
    {
        timerReport(&ABDataAdapter::_adapterTime, "Adapter Time");
        timerReport(&_rfiClipperTime, "RFI_Clipper");
        timerReport(&_dedispersionTime, "DedispersionModule");

        timerReport(&_totalTime, "Pipeline Time (excluding adapter)");
        std::cout << endl;
        std::cout << "Total (average) allowed time per iteration = " << stokesBuf->getBlockRate() * stokesBuf->nTimeBlocks() << " sec" << "\n";
        std::cout << "Total (average) actual time per iteration = "
                  << ABDataAdapter::_adapterTime.timeAverage +
                  _totalTime.timeAverage << " sec" << "\n";
        std::cout << std::endl;
    }
#endif

    _counter++;
}

void ABPipeline::dedispersionAnalysis( DataBlob* blob ) {
    DedispersionDataAnalysis result;
    DedispersionSpectra* data = static_cast<DedispersionSpectra*>(blob);
    if ( _dedispersionAnalyser->analyse(data, &result) )
      {
        std::cout << "Found " << result.eventsFound() << " events" << std::endl;
        std::cout << "Limits: " << _minEventsFound << " " << _maxEventsFound << " events" << std::endl;
        dataOutput( &result, "TriggerInput" );
        if (_minEventsFound >= _maxEventsFound){
            std::cout << "Writing out..." << std::endl;
            if (result.eventsFound() >= _minEventsFound){
              dataOutput( &result, "DedispersionDataAnalysis" );
              foreach( const SpectrumDataSetStokes* d, result.data()->inputDataBlobs()) {
                dataOutput( d, "SignalFoundSpectrum" );
              }
            }
        }
        else{
          if (result.eventsFound() >= _minEventsFound && result.eventsFound() <= _maxEventsFound){
            std::cout << "Writing out..." << std::endl;
            dataOutput( &result, "DedispersionDataAnalysis" );
            foreach( const SpectrumDataSetStokes* d, result.data()->inputDataBlobs()) {
              dataOutput( d, "SignalFoundSpectrum" );
            }
          }
        }
      }
}

void ABPipeline::updateBufferLock( const QList<DataBlob*>& freeData ) {
     // find WeightedDataBlobs that can be unlocked
     foreach( DataBlob* blob, freeData ) {
        Q_ASSERT( blob->type() == "SpectrumDataSetStokes" );
        // unlock the pointers to the raw buffer
        _stokesBuffer->unlock( static_cast<SpectrumDataSetStokes*>(blob) );
     }
}

