#include "K7DataClient.h"
#include "K7Chunker.h"

using namespace pelican;
using namespace pelican::ampp;

K7DataClient::K7DataClient(const ConfigNode& configNode, const DataTypes& types, const Config* config) : PelicanServerClient(configNode, types, config)
{
}

K7DataClient::~K7DataClient()
{
}

