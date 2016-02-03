#ifndef DEDISPERSION_PARAMETERS_H_
#define DEDISPERSION_PARAMETERS_H_

// Original ALFABURST
#if 0
#define NUMREG      14
#define DIVINT      16
#define DIVINDM     40
#endif
// After Wes' tuning
#define NUMREG      14
#define DIVINT      4
#define DIVINDM     40

#define FDIVINDM    80.0f

#define ARRAYSIZE DIVINT * DIVINDM

#endif // DEDISPERSION_PARAMETERS_H_

