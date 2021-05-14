#ifndef _BLOCK_REPORTER_HH_
#define _BLOCK_REPORTER_HH_

#include <dirent.h>
#include <string.h>

#include <iostream>
#include <string>
#include <vector>

#include "Util/hiredis.h"

#define MAX_INFO_LEN 256

using namespace std;

/**
 * Sends block info to Redis of ECCoordinator
 */
class BlockReporter {
   public:
    static void report(unsigned int, const char* blkDir, unsigned int localIP);
};

#endif
