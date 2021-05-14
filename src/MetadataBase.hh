#ifndef _METADATA_BASE_HH_
#define _METADATA_BASE_HH_

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <streambuf>
#include <vector>
#include <unordered_map>

#include <dirent.h>

#include "Config.hh"
#include "RSUtil.hh"
#include "LRCUtil.hh"
#include "BUTTERFLY64Util.hh"
#include "Util/hiredis.h"

#define METADATA_BASE_DEBUG 0

using namespace std;

class MetadataBase {
  protected: 
    Config* _conf;
    RSUtil* _rsUtil;
    LRCUtil* _lrcUtil;
    BUTTERFLYUtil* _butterflyUtil;

  public:
    MetadataBase(Config* conf, RSUtil* rsu) : _conf(conf), _rsUtil(rsu) {};
    MetadataBase(Config* conf, RSUtil* rsu, LRCUtil* lrcu) : _conf(conf), _rsUtil(rsu), _lrcUtil(lrcu) {};
    MetadataBase(Config* conf, RSUtil* rsu, LRCUtil* lrcu, BUTTERFLYUtil* bu) : _conf(conf), _rsUtil(rsu), _lrcUtil(lrcu), _butterflyUtil(bu) {};
    ~MetadataBase(){};


    virtual map<string, int> getCoefficient(const string& blkName, map<string, int>* status) = 0;
    virtual vector<pair<string, unsigned int> > getBlksPlacement(vector<string> lostBlks) = 0;
    virtual vector<string> getBlksInIP(unsigned int ip) = 0;
    virtual map<string, int> getBlkRelatedStripeStatus(const string& blkName) = 0;
    virtual unsigned int getBlockIp(const string& blkName) = 0;
    virtual int getBlockStripeId(const string& blkName) = 0;
    virtual string getBlockStripeName(const string& blkName) = 0;
    virtual int getBlkIdInStripe(const string& blkName) = 0;
    virtual int* getLostBlkIdxInStripe(vector<string> lostBlks) = 0; // for LRC code, return the idx in the stripe 

};

#endif



