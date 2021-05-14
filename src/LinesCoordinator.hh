#ifndef _LINES_COORDINATOR_HH_
#define _LINES_COORDINATOR_HH_

#include <cstring>
#include <algorithm>

#include "Coordinator.hh"
#include "LinesDealScheduling.hh"

using namespace std;

class LinesCoordinator : public Coordinator {

    map<int, unsigned int> _helperId2Ip;
    map<unsigned int, int> _helperIp2Id;

    // override
    void requestHandler();

   public:
    LinesCoordinator(Config* c);
    void parseDRRequest(char*, int, unsigned int&, string&, vector<pair<string, unsigned int>>&);
    void parseFNRequest(char*, int, unsigned int&, vector<string>&); 

    void getSurviHelperIds(map<string, int>*, vector<pair<string, int> >&);
    void solveFullNodeRequest(char*, int, redisContext*);
};

#endif  //_LINES_COORDINATOR_HH_
