#ifndef _RS_UTIL_HH_
#define _RS_UTIL_HH_

#include <iostream>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "Config.hh"

extern "C" {
    #include "Util/galois.h"
    #include <gf_complete.h>
    #include "Util/jerasure.h"
}

using namespace std;

class RSUtil {
    int _ecK;
    int _ecN;
    int _ecM;
    int* _encMat;
    int* _completeEncMat;
    gf_t _gf;

  public:
    RSUtil() {};
    RSUtil(Config* conf, int* encMat);
    
    static void multiply(char* buf, int mulby, int size);
    int* getCoefficient_specifiedBlks(int idx, int* status);

};

#endif //_RS_UTIL_HH_


