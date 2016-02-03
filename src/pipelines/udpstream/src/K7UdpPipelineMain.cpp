#include "pelican/core/PipelineApplication.h"
#include "K7DataClient.h"
#include "K7UdpPipeline.h"
#include "K7DataAdapter.h"
#include <QtCore/QCoreApplication>

using namespace pelican;
using namespace pelican::ampp;

int main(int argc, char* argv[], const QString& stream)
{
    std::cout << "Starting K7UdpPipelineMain..." << std::endl;

    // Create a QCoreApplication.
    QCoreApplication app(argc, argv);

    try
    {
        // Create a PipelineApplication.
        PipelineApplication pApp(argc, argv);

        // Register the pipelines that can run.
        pApp.registerPipeline(new K7UdpPipeline(stream));

        // Set the data client.
        pApp.setDataClient("K7DataClient");

        // Start the pipeline driver.
        pApp.start();
    }

    // Catch any error messages from Pelican.
    catch (const QString& err)
    {
        std::cerr << "K7UdpPipelineMain: " << err.toStdString() << std::endl;
    }

    return 0;
}

