#ifndef _COMPUTATION_HH_
#define _COMPUTATION_HH_

#include <iostream>

/**
 * This class is attampted to provide galois field operations
 */
class Computation {
   public:
    static void XORBuffers(char* dst, char* src, size_t length);
};

#endif
