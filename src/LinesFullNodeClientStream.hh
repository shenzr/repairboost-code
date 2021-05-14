#ifndef _LINES_FULLNODE_CLIENT_STREAM_HH_
#define _LINES_FULLNODE_CLIENT_STREAM_HH_

#include <fstream>
#include <cassert>
#include <typeinfo>
#include <arpa/inet.h>
#include <hiredis/hiredis.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "Config.hh"
#include "LinesDefine.hh"

#define EIS_DEBUG 0

using namespace std;

class LinesFullNodeClientStream {
    private:
        Config* _conf;
        redisContext* _selfCtx;
        redisContext* _coordinatorCtx;

        vector<pair<unsigned int, redisContext*> > _slaveCtx;

        vector<pair<string, unsigned int> > _lostFiles;  // The second element is the requestor IP.
        vector<unsigned int> _fetchIPs;

        size_t _packetCnt;
        size_t _packetSize;

        unsigned int _locIP;

        char** _contents;
 
        redisContext* findCtx(unsigned int ip);
        void initRedis(redisContext*&, unsigned int ip, unsigned short port);

        void sendFullNodeRequest();
        void sendCmdToCoordinator();
        void pipeCollector(); 

    public:
        LinesFullNodeClientStream(Config*, size_t packetCnt, size_t packetSize, unsigned int coorIP, unsigned int locIP);
        ~LinesFullNodeClientStream();
};

#endif  //_LINES_FULLNODE_CLIENT_STREAM_HH_
