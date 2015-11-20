#ifndef K7PIPELINE_H
#define K7PIPELINE_H

#include "pelican/core/AbstractPipeline.h"
#include "RFI_Clipper.h"
#include "DedispersionModule.h"
#include "DedispersionAnalyser.h"
#include "WeightedSpectrumDataSet.h"
#include "StokesIntegrator.h"
#ifdef TIMING_ENABLED
#include "timer.h"
#endif

namespace pelican {
namespace ampp {

class K7Pipeline : public AbstractPipeline
{
    public:
        // Constructor.
        K7Pipeline(const QString& streamIdentifier);

        // Destructor
        ~K7Pipeline();

        // Initialises the pipeline.
        void init();

        // Defines one iteration of the pipeline.
        void run(QHash<QString, DataBlob*>& remoteData);

        /// called internally to free up DataBlobs after they are finished with
        void updateBufferLock( const QList<DataBlob*>& );

    protected:
        void dedispersionAnalysis( DataBlob* data );

    private:
        QString _streamIdentifier;

        // Module pointers.
        RFI_Clipper* _rfiClipper;
        DedispersionModule* _dedispersionModule;
        DedispersionAnalyser* _dedispersionAnalyser;
        StokesIntegrator* _stokesIntegrator;

        // Local data blob pointers.
        QList<SpectrumDataSetStokes*> _stokesData;
        LockingPtrContainer<SpectrumDataSetStokes>* _stokesBuffer;
        WeightedSpectrumDataSet* _weightedIntStokes;

        SpectrumDataSetStokes *_stokes;
        SpectrumDataSetStokes *_intStokes;

        unsigned long int _counter;
        unsigned int _iteration;
        unsigned int _minEventsFound;
        unsigned int _maxEventsFound;

#ifdef TIMING_ENABLED
        TimerData _rfiClipperTime;
        TimerData _dedispersionTime;
        TimerData _totalTime;
#endif
};

} // namespace ampp
} // namespace pelican

#endif // K7PIPELINE_H

