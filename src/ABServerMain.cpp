#include "server/PelicanServer.h"
#include "comms/PelicanProtocol.h"
#include "utility/Config.h"

#include "ABChunker.h"

#include <QtCore/QCoreApplication>
#include <boost/program_options.hpp>
#include <iostream>

namespace opts = boost::program_options;
using namespace pelican;

// Prototype for function to create a pelican configuration XML object.
Config createConfig(int argc, char** argv);

int main(int argc, char ** argv)
{
    // Create a QCoreApplication.
    QCoreApplication app(argc, argv);

    // Create a Pelican configuration object (this assumes that a Pelican
    // configuration XML file is supplied as the first command line argument)
    if (argc != 2) {
        std::cerr << "Please supply an XML config file." << std::endl;
        return 0;
    }
    //QString configFile(argv[1]);
    //Config config(configFile);
    Config config = createConfig(argc, argv);

    try {
        // Create a Pelican server.
        PelicanServer server(&config);

        // Attach the chunker to server.
        server.addStreamChunker("ABChunker");

        // Create a communication protocol object and attach it to the server
        // on port 15000.
        AbstractProtocol *protocol =  new PelicanProtocol;

        // Get the transmission port address
        Config::TreeAddress address;
        address << Config::NodeId("server", "");
        ConfigNode configNode = config.get(address);
        quint16 txPort = (quint16) configNode.getOption("tx", "port").toUInt();
        std::cout << "Using port " << txPort << "." << std::endl;
        server.addProtocol(protocol, txPort);

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
        return 1;
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
    opts::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce help message.")
        ("config,c", opts::value<std::string>(), "Set configuration file.");


    // Configuration option without a selection flag in the first argument
    // position is assumed to be a config file
    opts::positional_options_description p;
    p.add("config", -1);

    // Parse the command line arguments.
    opts::variables_map varMap;
    opts::store(opts::command_line_parser(argc, argv).options(desc)
            .positional(p).run(), varMap);
    opts::notify(varMap);

    // Check for help message.
    if (varMap.count("help")) {
        std::cout << desc << std::endl;;
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

