#include "BufferingDataClient.h"
#include <QCoreApplication>
#include <boost/bind.hpp>

namespace pelican {
namespace ampp {

template<class DataClientType>
BufferingDataClient<DataClientType>::BufferingDataClient(const ConfigNode& configNode, const DataTypes& types, const Config* config)
    : DataClientType(configNode, types, config)
    , _agent(boost::bind(&DataClientType::getData, this, _1))
//    , _halt(false)
{
    // start the thread running that collects data
    std::cout << "starting agent..." << std::endl;
    _agent.start();
    std::cout << "done..." << std::endl;

    // dedicate a thread to running the event loop of the agent to ensure messages are delivered to the agent
    //QtConcurrent::run(boost::bind(&BufferingDataClient<DataClientType>::exec, this));   
}

template<class DataClientType>
BufferingDataClient<DataClientType>::~BufferingDataClient()
{
    // stop the thread running
    std::cout << "stopping agent..." << std::endl;
    _agent.stop();
    std::cout << "done..." << std::endl;
    //_halt = true;
}

#if 0
template<class DataClientType>
void BufferingDataClient<DataClientType>::exec() 
{
    while(!_halt) {
        QCoreApplication::processEvents();
    }
}
#endif

template<class DataClientType>
pelican::AbstractDataClient::DataBlobHash BufferingDataClient<DataClientType>::getData(pelican::AbstractDataClient::DataBlobHash& hash)
{
    std::cout << "getting data..." << std::endl;
    _agent.getData(hash);
    std::cout << "done..." << std::endl;
    return hash;
}

} // namespace ampp
} // namespace pelican
