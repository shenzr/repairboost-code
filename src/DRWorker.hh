#ifndef _DR_WORDER_HH_
#define _DR_WORDER_HH_

#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <fstream>

#include <dirent.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "Computation.hh"
#include "Config.hh"
#include "RSUtil.hh"
#include "Util/hiredis.h"
#include "LinesDefine.hh"


#define DR_WORKER_DEBUG 0 

using namespace std;

/**
 * Work pattern of DRWorker
 *   1. wait for cmds
 *   2. start read thread to read from disk
 *   3. read data from redis (should be sent from preceder) computed with data from disk, send to redis in successor
 */

struct variablesForTran {
    bool isNeedPull;
    vector<mutex> _diskMtx;
    condition_variable _diskCv;
    bool* _diskFlag;
    char** _diskPkts;

    int _waitingToSend;
    mutex _mainSenderMtx;
    condition_variable _mainSenderCondVar;

    int* _pktsWaiting;
    mutex _pullWorkerMtx;
    vector<mutex> _diskCalMtx;
    condition_variable _diskCalCv;

    vector<pair<unsigned int, redisContext*>> _slavesCtx;
};

class DRWorker {
  protected:
    int _packetCnt;
    size_t _packetSize;

    unsigned int _localIP;

    Config* _conf;
    variablesForTran _send, _receive;
    
    redisContext* _selfCtx;


    redisContext* initCtx(unsigned int);
    void init(unsigned int, unsigned int, vector<unsigned int>&);

    // helper function
    redisContext* findCtx(vector<pair<unsigned int, redisContext*>>& slavesCtx, unsigned int ip);
    void readPacketFromDisk(int fd, char* content, int packetSize, int base);

    // reset the protected variables after using
    void cleanup(variablesForTran& var); 


  public:
    DRWorker(Config* conf);
    virtual void doProcess() {;};

    void readFile(string blkDir, string fileName, string& blkPath, bool& isFind);
};

#endif //_DR_WORDER_HH_




