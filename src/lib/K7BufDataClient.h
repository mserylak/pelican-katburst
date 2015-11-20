#ifndef K7BUFDATACLIENT_H
#define K7BUFDATACLIENT_H

#include "pelican/core/PelicanServerClient.h"
#include "BufferingDataClient.h"

namespace pelican {
namespace ampp {

typedef BufferingDataClient<PelicanServerClient> K7BufDataClient;

PELICAN_DECLARE_CLIENT(K7BufDataClient)

} // namespace ampp
} // namespace pelican

#endif // K7BUFDATACLIENT_H

