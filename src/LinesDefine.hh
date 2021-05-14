#ifndef _LINES_DEFINE_HH_
#define _LINES_DEFINE_HH_

#define COEF_SELECT_STATUS 1001

#define CLIENT_DEGRADED_READ 0
#define CLIENT_FULLNODE_RECOVERY 1

#define SCHEDUL_METHOD_BOOST 0
#define SCHEDUL_METHOD_RANDOM 1
#define SCHEDUL_METHOD_LRU 2

#define SINGLE_STRIPE_CR 0
#define SINGLE_STRIPE_PPR 1
#define SINGLE_STRIPE_PATH 2

#include <string>
using namespace std;

string ip2Str(unsigned int);


#endif

