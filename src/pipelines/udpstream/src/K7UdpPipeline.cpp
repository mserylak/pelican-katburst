#include "WeightedSpectrumDataSet.h"
#include "K7DataAdapter.h"
#include "K7UdpPipeline.h"
#include "SigprocStokesWriter.h"
#include <iostream>

namespace pelican {
namespace ampp {

// The constructor. It is good practice to initialise any pointer members to zero.
K7UdpPipeline::K7UdpPipeline(const QString& streamIdentifier) : AbstractPipeline(), _streamIdentifier(streamIdentifier)
{
    _iteration = 0;
}

// The destructor must clean up and created modules and any local dataBlob's created.
K7UdpPipeline::~K7UdpPipeline()
{
}

// Initialises the pipeline, creating required modules and data blobs, and requesting remote data.
void K7UdpPipeline::init()
{
    ConfigNode c = config(QString("K7UdpPipeline"));
    _totalIterations = c.getOption("totalIterations", "value", "0").toInt();
    std::cout << "K7UdpPipeline::init(): " << _totalIterations << " iterations of the pipeline" << std::endl;

    // Create the pipeline modules and any local data blobs.
    _rfiClipper = (RFI_Clipper *) createModule("RFI_Clipper");
    _stokesIntegrator = (StokesIntegrator *) createModule("StokesIntegrator");
    _intStokes = (SpectrumDataSetStokes *) createBlob("SpectrumDataSetStokes");
    _weightedIntStokes = (WeightedSpectrumDataSet*) createBlob("WeightedSpectrumDataSet");
    // Request remote data.
    requestRemoteData("SpectrumDataSetStokes");
}

// Defines a single iteration of the pipeline.
void K7UdpPipeline::run(QHash<QString, DataBlob*>& remoteData)
{
    // Get pointers to the remote data blob(s) from the supplied hash.
    SpectrumDataSetStokes* stokes = (SpectrumDataSetStokes*) remoteData["SpectrumDataSetStokes"];
    if( !stokes )
    {
        throw(QString("No stokes!"));
    }

    _stokesIntegrator->run(stokes, _intStokes);
    _weightedIntStokes->reset(_intStokes);

    dataOutput(_intStokes, "SpectrumDataSetStokes");
    _rfiClipper->run(_weightedIntStokes);

    if (_iteration % 100 == 0)
    {
        std::cout << "Finished the beamforming pipeline, iteration " << _iteration << " out of " << _totalIterations << std::endl;
    }
    _iteration++;

    if (_iteration != 0)
    {
        if (_iteration == _totalIterations)
        {
            stop();
        }
    }
}

} // namespace ampp
} // namespace pelican

