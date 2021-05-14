#ifndef _CONFIG_HH_
#define _CONFIG_HH_

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "LinesDefine.hh"
#include "Util/tinyxml2.h"

#define COMMAND_MAX_LENGTH 1024 

using namespace tinyxml2;

class Config {
  public:
    size_t _ecK;
    size_t _ecN;
    size_t _lrcL;
    size_t _packetSize;
    size_t _packetSkipSize = 0;
    size_t _packetCnt;
    int _chunkRepairMethod = 0; 

    std::string _ecType;
    std::string _ecConfigFile;
    std::string _linkWeightFile;
    
    int* _group_id;

    std::vector<unsigned int> _helpersIPs;
    unsigned int _coordinatorIP;
    unsigned int _localIP;


    std::string _fileSysType;
    /**
     * HDFS related variables
     */
    std::string _stripeStore;
    std::string _hdfsHome;
    std::string _blkDir;

    /**
     * Thread nums
     */
    size_t _coCmdDistThreadNum = 1; // command distributor thread number in coordinator
    size_t _coCmdReqHandlerThreadNum = 1; // request handler thread number in coordinator

    Config(std::string confFile);
    void display();
};

#endif

