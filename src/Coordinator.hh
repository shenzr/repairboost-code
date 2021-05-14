#ifndef _COORDINATOR_HH_
#define _COORDINATOR_HH_

#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <unordered_map>

#include <arpa/inet.h>
#include <hiredis/hiredis.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "Config.hh"
#include "MetadataBase.hh"
#include "HDFS_MetadataBase.hh"
#include "HDFS3_MetadataBase.hh"
#include "RSUtil.hh"
#include "LRCUtil.hh"
#include "BUTTERFLY64Util.hh"
#include "LinesDefine.hh"


#define COORDINATOR_DEBUG 0 
#define FILENAME_MAX_LENGTH 256

using namespace std;

/**
 * ONE COORDINATOR to rule them ALL!!!
 */
class Coordinator {
  protected:
    Config* _conf;
    RSUtil* _rsUtil;
    LRCUtil* _lrcUtil;
    BUTTERFLYUtil* _butterflyUtil;
    size_t _handlerThreadNum;
    size_t _distributorThreadNum;
    vector<thread> _distThrds;
    vector<thread> _handlerThrds;

    int _slaveCnt;
    int _ecK;
    int _ecN;
    int _ecM;
    int _ec_LRC_L;
    int _ec_LRC_G;
    int _coefficient;
    int* _encMat;

    string _ecConfigFile;
    MetadataBase* _metadataBase;
    
    redisContext* _selfCtx;
    map<unsigned int, int> _ip2idx;
    vector<pair<unsigned int, pair<string, redisContext*>>> _ip2Ctx;
 
    void init();

    redisContext* initCtx(unsigned int);
    int searchCtx(vector<pair<unsigned int, pair<string, redisContext*>>>&, unsigned int, size_t, size_t);
    
    void cmdDistributor(int id, int total);
    virtual void requestHandler();

  public:
    // just init redis contexts
    Coordinator(Config*);
    virtual void doProcess();
};

#endif //_DR_COORDINATOR_HH_



