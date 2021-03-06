#ifndef CONSTANTS_H__
#define CONSTANTS_H__

/**
 * @file constants.h
 */

namespace pelican {
namespace ampp {

/**
 * @brief
 * Constants used by Pelican-lofar modules.
 */

namespace math
{
const double pi       = 3.141592653589793238462643383279502884197;
const double twoPi    = 6.283185307179586476925286766559005768394;
const double deg2rad  = 0.01745329251994329576923690768488612713443;
const double rad2deg  = 57.29577951308232087679815481410517033241;
const double rad2asec = 2.062648062470963551564733573307786131967e5;
const double asec2rad = 4.848136811095359935899141023579479759564e-6;
const double deg2asec = 3600.0;
const double asec2deg = 2.777777777777777777777777777777777777778e-4;
} // namespace math


/**
 * @brief
 * Constants used by Pelican-lofar modules.
 */

///
namespace phy
{
const double c = 299792458; // speed of light
} // namespace phy


} // namespace ampp
} // namespace pelican
#endif // CONSTANTS_H__
