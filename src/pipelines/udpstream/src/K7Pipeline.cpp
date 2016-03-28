#include "DedispersionModule.h"
#include "DedispersionDataAnalysis.h"
#include "DedispersionDataAnalysisOutput.h"
#include "WeightedSpectrumDataSet.h"
#include "K7DataAdapter.h"
#include "K7Pipeline.h"
#include "SigprocStokesWriter.h"
#include <boost/bind.hpp>
#include <iostream>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <stdio.h>

namespace pelican {
namespace ampp {

// The constructor. It is good practice to initialise any pointer members to zero.
K7Pipeline::K7Pipeline(const QString& streamIdentifier) : AbstractPipeline(), _streamIdentifier(streamIdentifier)
{
    _rfiClipper = 0;
    _stokesIntegrator = 0;
    _dedispersionModule = 0;
    _dedispersionAnalyser = 0;
    _iteration = 0;
}

// The destructor must clean up and created modules and any local dataBlob's created.
K7Pipeline::~K7Pipeline()
{
    delete _rfiClipper;
    delete _stokesIntegrator;
    delete _dedispersionModule;
    delete _dedispersionAnalyser;
    foreach ( SpectrumDataSetStokes* d, _stokesData )
    {
        delete d;
    }
}

// Initialises the pipeline, creating required modules and data blobs, and requesting remote data.
void K7Pipeline::init()
{
    ConfigNode c = config(QString("K7Pipeline"));
    // History indicates the number of datablobs to keep (iterations of run()).        <- HERE LIES THE PROBLEM!
    // It should be dedispersion buffer size (in blobs) * number of dedispersion buffers.
    unsigned int history = c.getOption("history", "value", "10").toUInt();
    _minEventsFound = c.getOption("events", "min", "5").toUInt();
    _maxEventsFound = c.getOption("events", "max", "5").toUInt();

    // Create the pipeline modules and any local data blobs.
    _rfiClipper = (RFI_Clipper *) createModule("RFI_Clipper");
    _stokesIntegrator = (StokesIntegrator *) createModule("StokesIntegrator");
    _dedispersionModule = (DedispersionModule*) createModule("DedispersionModule");
    _dedispersionAnalyser = (DedispersionAnalyser*) createModule("DedispersionAnalyser");
    _dedispersionModule->connect( boost::bind( &K7Pipeline::dedispersionAnalysis, this, _1 ) );
    _dedispersionModule->unlockCallback( boost::bind( &K7Pipeline::updateBufferLock, this, _1 ) );
    _stokesData = createBlobs<SpectrumDataSetStokes>("SpectrumDataSetStokes", history);
    _stokesBuffer = new LockingPtrContainer<SpectrumDataSetStokes>(&_stokesData);
    _intStokes = (SpectrumDataSetStokes *) createBlob("SpectrumDataSetStokes");
    _weightedIntStokes = (WeightedSpectrumDataSet*) createBlob("WeightedSpectrumDataSet");

    // Request remote data.
    requestRemoteData("SpectrumDataSetStokes");
}

// Defines a single iteration of the pipeline.
void K7Pipeline::run(QHash<QString, DataBlob*>& remoteData)
{
    // Get pointers to the remote data blob(s) from the supplied hash.
    SpectrumDataSetStokes* stokes = (SpectrumDataSetStokes*) remoteData["SpectrumDataSetStokes"];
    if ( !stokes )
    {
        throw (QString("K7Pipeline::run(): No stokes!"));
    }
    // To make sure the dedispersion module reads data from a lockable ring buffer, copy data to one.
    SpectrumDataSetStokes* stokesBuf = _stokesBuffer->next();

    _stokesIntegrator->run(stokes, _intStokes);
    *stokesBuf = *_intStokes;
    _weightedIntStokes->reset(stokesBuf);

    dataOutput(_intStokes, "SpectrumDataSetStokes");
    _rfiClipper->run(_weightedIntStokes);
    _dedispersionModule->dedisperse(_weightedIntStokes);
    if (0 == _iteration % 5)
    {
        std::cout << "K7Pipeline::run(): Finished the dedispersion pipeline, iteration " << _iteration << std::endl;
    }
    _iteration++;
}

void K7Pipeline::dedispersionAnalysis( DataBlob* blob )
{
    DedispersionDataAnalysis result;
    DedispersionSpectra* data = static_cast<DedispersionSpectra*>(blob);
    if ( _dedispersionAnalyser->analyse(data, &result) )
    {
        std::cout << "K7Pipeline::dedispersionAnalysis(): Found " << result.eventsFound() << " events" << std::endl;
        std::cout << "K7Pipeline::dedispersionAnalysis(): Limits: " << _minEventsFound << " " << _maxEventsFound << " events" << std::endl;
        dataOutput( &result, "TriggerInput" );
        if (_minEventsFound >= _maxEventsFound)
        {
            std::cout << "K7Pipeline::dedispersionAnalysis(): Writing out..." << std::endl;
            if (result.eventsFound() >= _minEventsFound)
            {
                dataOutput( &result, "DedispersionDataAnalysis" );
                foreach( const SpectrumDataSetStokes* d, result.data()->inputDataBlobs())
                {
                    dataOutput( d, "SignalFoundSpectrum" );
                }
            }
        }
        else
        {
            if (result.eventsFound() >= _minEventsFound && result.eventsFound() <= _maxEventsFound)
            {
                std::cout << "K7Pipeline::dedispersionAnalysis(): Writing out..." << std::endl;
                dataOutput( &result, "DedispersionDataAnalysis" );
                foreach( const SpectrumDataSetStokes* d, result.data()->inputDataBlobs())
                {
                    dataOutput( d, "SignalFoundSpectrum" );
                }
            }
        }
    }
}

void K7Pipeline::updateBufferLock( const QList<DataBlob*>& freeData )
{
    // Find WeightedDataBlobs that can be unlocked.
    foreach( DataBlob* blob, freeData )
    {
        Q_ASSERT( blob->type() == "SpectrumDataSetStokes" );
        // Unlock the pointers to the raw buffer.
        _stokesBuffer->unlock( static_cast<SpectrumDataSetStokes*>(blob) );
    }
}

} // namespace ampp
} // namespace pelican
