#include "pelican/emulator/EmulatorDriver.h"
#include "K7Emulator.h"
#include "pelican/utility/Config.h"
#include <QtCore/QCoreApplication>
#include <boost/program_options.hpp>
#include <iostream>

using namespace pelican;
using namespace pelican::ampp;

// Prototype for function to create a pelican configuration XML object.
pelican::Config createConfig(int argc, char** argv);

int main(int argc, char** argv)
{
    // Create a QCoreApplication
    QCoreApplication app(argc, argv);

    try
    {
        pelican::Config config = createConfig(argc, argv);
        Config::TreeAddress emulatorAddress;
        emulatorAddress << Config::NodeId("emulators","") << Config::NodeId("K7Emulator","");
        ConfigNode configNode = config.get(emulatorAddress);
        EmulatorDriver emulator(new K7Emulator(configNode));
        return app.exec();
    }

    // Catch any error messages from Pelican
    catch (const QString& err)
    {
        std::cerr << "Error: " << err.toStdString() << std::endl;
    }

    return 0;
}

// Create a Pelican Configuration XML document for the K7 emulator.
pelican::Config createConfig(int argc, char** argv)
{
    // Check that argc and argv are nonzero
    if (argc == 0 || argv == NULL)
    {
        throw QString("No command line.");
    }

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
    {
        config = pelican::Config(QString(configFilename.c_str()));
    }

    return config;
}
