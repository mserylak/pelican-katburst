#ifndef K7DATACLIENT_H
#define K7DATACLIENT_H

#include "pelican/core/PelicanServerClient.h"

namespace pelican {
namespace ampp {

class K7DataClient : public PelicanServerClient
{
    public:
        K7DataClient( const ConfigNode& configNode, const DataTypes& types, const Config* config );
        ~K7DataClient();
    private:
};

PELICAN_DECLARE_CLIENT(K7DataClient)

} // namespace ampp
} // namespace pelican

#endif // K7DATACLIENT_H

