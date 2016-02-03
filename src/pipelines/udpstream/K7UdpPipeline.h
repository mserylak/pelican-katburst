#ifndef K7UDPPIPELINE_H
#define K7UDPPIPELINE_H

#include "pelican/core/AbstractPipeline.h"
#include "RFI_Clipper.h"
#include "WeightedSpectrumDataSet.h"
#include "StokesIntegrator.h"

namespace pelican {
namespace ampp {

class K7UdpPipeline : public AbstractPipeline
{
    public:
        // Constructor.
        K7UdpPipeline(const QString& streamIdentifier);

        // Destructor
        ~K7UdpPipeline();

        // Initialises the pipeline.
        void init();

        // Defines one iteration of the pipeline.
        void run(QHash<QString, DataBlob*>& remoteData);

    private:
        int _totalIterations;
        unsigned int _iteration;
        QString _streamIdentifier;

        // Module pointers.
        StokesIntegrator* _stokesIntegrator;
        RFI_Clipper* _rfiClipper;

        // Local data blob pointers.
        SpectrumDataSetStokes* _stokesData;
        SpectrumDataSetStokes *_stokes;
        SpectrumDataSetStokes *_intStokes;
        WeightedSpectrumDataSet* _weightedIntStokes;
};

} // namespace ampp
} // namespace pelican

#endif // K7UDPPIPELINE_H

