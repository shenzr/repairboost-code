#ifndef _HDFS_METADATA_BASE_HH_
#define _HDFS_METADATA_BASE_HH_

#include <typeinfo>
#include <algorithm>

#include "MetadataBase.hh"

using namespace std;

class HDFS_MetadataBase : public MetadataBase{
  private: 
    vector<vector<string> > _allBlksInStripe;
    map<string, unsigned int> _blkIdInStripe;
    

    map<string, int> _blkStatus;
    map<string, int> _blk2StripeId;
    map<string, unsigned int> _blk2Ip;
    map<unsigned int, vector<string> > _ip2BlkNames;
    
    
  public:
    HDFS_MetadataBase(Config* conf, RSUtil* rsu, LRCUtil* lrcu, BUTTERFLYUtil* bu);
    
    vector<string> getBlksInIP(unsigned int ip);
    vector<pair<string, unsigned int> > getBlksPlacement(vector<string> lostBlks);
    unsigned int getBlockIp(const string& blkName);
    map<string, int> getBlkRelatedStripeStatus(const string& blkName);
    map<string, int> getCoefficient(const string& blkName, map<string, int>* status); // status 
    int getBlockStripeId(const string& blkName);
    string getBlockStripeName(const string& blkName);
    int getBlkIdInStripe(const string& blkName);
    int* getLostBlkIdxInStripe(vector<string> lostBlks);
    
    
};

#endif



