#ifndef _LRC_UTIL_HH_
#define _LRC_UTIL_HH_

#include <string.h>
#include "Config.hh"

extern "C" {
    #include "Util/galois.h"
    #include <gf_complete.h>
    #include "Util/jerasure.h"
}

using namespace std;

class LRCUtil {
    int _ecK;
    int _ecN;
    int _LRC_L;
    int _LRC_G;
    int* _group_id;

    int* _encMat;
    int* _completeEncMat;
    gf_t _gf;

  public:
    LRCUtil(Config* conf, int* encMat);
    static void multiply(char* buf, int mulby, int size);
    int* getCoefficient_specifiedBlks(int idx, int* status);
};

#endif //_RS_UTIL_HH_

