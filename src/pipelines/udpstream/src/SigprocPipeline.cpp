#include "DedispersionModule.h"
#include "DedispersionDataAnalysis.h"
#include "SigprocPipeline.h"
#include "SigprocAdapter.h"
#include "SpectrumDataSet.h"
#include "WeightedSpectrumDataSet.h"
#include <boost/bind.hpp>

using namespace pelican;
using namespace ampp;

// The constructor. It is good practice to initialise any pointer
// members to zero.
SigprocPipeline::SigprocPipeline()
    : AbstractPipeline()
{
    _rfiClipper = 0;
    _dedispersionModule = 0;
    _dedispersionAnalyser = 0;
}

// The destructor must clean up and created modules and
// any local DataBlob's created.
SigprocPipeline::~SigprocPipeline()
{
    delete _dedispersionAnalyser;
    delete _dedispersionModule;
    delete _rfiClipper;
}

// Initialises the pipeline, creating required modules and data blobs,
// and requesting remote data.
void SigprocPipeline::init()
{
    ConfigNode c = config(QString("SigprocPipeline"));
    // history indicates the number of datablobs to keep (iterations of run())
    // it should be Dedidpersion Buffer size (in Blobs)*number of Dedispersion Buffers
    unsigned int history = c.getOption("history", "value", "10").toUInt();
    _minEventsFound = c.getOption("events", "min", "5").toUInt();
    _maxEventsFound = c.getOption("events", "max", "5").toUInt();

    // Create the pipeline modules and any local data blobs.
    _rfiClipper = (RFI_Clipper *) createModule("RFI_Clipper");
    _dedispersionModule = (DedispersionModule*) createModule("DedispersionModule");
    _dedispersionAnalyser = (DedispersionAnalyser*) createModule("DedispersionAnalyser");
    _dedispersionModule->connect( boost::bind( &SigprocPipeline::dedispersionAnalysis, this, _1 ) );
    _dedispersionModule->unlockCallback( boost::bind( &SigprocPipeline::updateBufferLock, this, _1 ) );
    _stokesData = createBlobs<SpectrumDataSetStokes>("SpectrumDataSetStokes", history);
    _stokesBuffer = new LockingPtrContainer<SpectrumDataSetStokes>(&_stokesData);
    _weightedIntStokes = (WeightedSpectrumDataSet*) createBlob("WeightedSpectrumDataSet");

    // Request remote data.
    requestRemoteData("SpectrumDataSetStokes");
}

// Defines a single iteration of the pipeline.
void SigprocPipeline::run(QHash<QString, DataBlob*>& remoteData)
{
    SpectrumDataSetStokes* stokes = (SpectrumDataSetStokes*) remoteData["SpectrumDataSetStokes"];
    if( !stokes ) throw(QString("No stokes!"));

    /* to make sure the dedispersion module reads data from a lockable ring
       buffer, copy data to one */
        SpectrumDataSetStokes* stokesBuf = _stokesBuffer->next();
    *stokesBuf = *stokes;

        _weightedIntStokes->reset(stokesBuf);
    //_weightedIntStokes->reset(stokes);
    _rfiClipper->run(_weightedIntStokes);
    _dedispersionModule->dedisperse(_weightedIntStokes);
    dataOutput(stokesBuf, "SpectrumDataSetStokes");
}

void SigprocPipeline::dedispersionAnalysis( DataBlob* blob ) {
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

void SigprocPipeline::updateBufferLock( const QList<DataBlob*>& freeData ) {
     // find WeightedDataBlobs that can be unlocked
     foreach( DataBlob* blob, freeData ) {
        Q_ASSERT( blob->type() == "SpectrumDataSetStokes" );
        // unlock the pointers to the raw buffer
        _stokesBuffer->unlock( static_cast<SpectrumDataSetStokes*>(blob) );
     }
}

