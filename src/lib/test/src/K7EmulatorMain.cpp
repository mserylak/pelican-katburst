#include <iostream>
#include <QtCore/QCoreApplication>
#include "pelican/emulator/EmulatorDriver.h"
#include "K7Emulator.h"

using namespace pelican;
using namespace pelican::ampp;

int main(int argc, char* argv[])
{
    // Create a QCoreApplication
    QCoreApplication app(argc, argv);

    try
    {
        ConfigNode emulatorConfig("<K7Emulator>"
                                  "<packet samples=\"1024\" interval=\"656\"/>"
                                  "<connection host=\"127.0.0.1\" port=\"9999\"/>"
                                  "</K7Emulator>");
        EmulatorDriver emulator(new K7Emulator(emulatorConfig));
        return app.exec();
    }

    // Catch any error messages from Pelican
    catch (const QString& err)
    {
        std::cerr << "Error: " << err.toStdString() << std::endl;
    }

    return 0;
}

