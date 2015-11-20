#include "pelican/server/PelicanServer.h"
#include "pelican/comms/PelicanProtocol.h"
#include "pelican/utility/Config.h"
#include "K7Chunker.h"

#include <QtCore/QCoreApplication>
#include <boost/program_options.hpp>
#include <iostream>
#include <cstdlib>

// Prototype for function to create a pelican configuration XML object.
pelican::Config createConfig(int argc, char** argv);

int main(int argc, char ** argv)
{
    // Create a QCoreApplication.
    QCoreApplication app(argc, argv);

    try {

        // Parse command line arguments to create the configuration object.
        pelican::Config config = createConfig(argc, argv);
        // Create a Pelican server.
        pelican::PelicanServer server(&config);

        // Attach the chunker to server.
        server.addStreamChunker("K7Chunker");

        // Create a communication protocol object and attach it to the server on port 2000.
        pelican::AbstractProtocol* protocol =  new pelican::PelicanProtocol;
        server.addProtocol(protocol, 2000);

        // Start the server.
        server.start();

        // When the server is ready enter the QCoreApplication event loop.
        while (!server.isReady()) {}
        return app.exec();
    }
    // Catch any error messages from Pelican.
    catch (const QString& err)
    {
        std::cerr << "Error: " << err.toStdString() << std::endl;
    }
}

/**
 * @details
 * Create a Pelican Configuration XML document for the lofar data viewer.
 */
pelican::Config createConfig(int argc, char** argv)
{
    // Check that argc and argv are nonzero
    if (argc == 0 || argv == NULL) throw QString("No command line.");

    // Declare the supported options.
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce help message.")
        ("config,c", boost::program_options::value<std::string>(), "Set configuration file.");

    // Configuration option without a selection flag in the first argument
    // position is assumed to be a config file
    boost::program_options::positional_options_description p;
    p.add("config", -1);

    // Parse the command line arguments.
    boost::program_options::variables_map varMap;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p).run(), varMap);
    boost::program_options::notify(varMap);

    // Check for help message.
    if (varMap.count("help"))
    {
        std::cout << desc << std::endl;
        exit(0);
    }

    // Get the configuration file name.
    std::string configFilename = "";
    if (varMap.count("config"))
        configFilename = varMap["config"].as<std::string>();

    pelican::Config config;
    if (!configFilename.empty())
        config = pelican::Config(QString(configFilename.c_str()));

    return config;
}
