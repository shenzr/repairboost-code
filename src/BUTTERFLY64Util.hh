#ifndef _BUTTERFLY_UTIL_HH_
#define _BUTTERFLY_UTIL_HH_

#include <assert.h>
#include <string.h>
#include "Config.hh"

extern "C" {
    #include "Util/galois.h"
    #include <gf_complete.h>
    #include "Util/jerasure.h"
}

using namespace std;

class BUTTERFLYUtil {
    int _ecK;
    int _ecN;
    int _ecM;
    int _alpha;
    int* _encMat;
    gf_t _gf;

  public:
    BUTTERFLYUtil(Config* conf);
    void generate_encoding_matrix();
    void generate_decoding_matrix(int decMat[6][160]);
    int* getCoefficient(int idx);
};

#endif //_BUTTERFLY_UTIL_HH_


