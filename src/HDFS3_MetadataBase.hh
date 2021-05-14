#ifndef _HDFS3_METADATA_BASE_HH_
#define _HDFS3_METADATA_BASE_HH_

#include <thread>
#include "MetadataBase.hh"

using namespace std;

class HDFS3_MetadataBase : public MetadataBase {
    private:
        thread _metadataThread;
        void GetMetadata();

        vector<vector<string> > _allBlksInStripe;
        map<string, unsigned int> _blkIdInStripe;

        map<string, int> _blkStatus;
        map<string, int> _blk2StripeId;
        map<int, string> _stripeId2StripeName;
        map<string, unsigned int> _blk2Ip;
        map<unsigned int, vector<string> > _ip2BlkNames;

    public:
        HDFS3_MetadataBase(Config* conf, RSUtil* rsu);

        map<string, int> getCoefficient(const string& blkName, map<string, int>* status);
        vector<pair<string, unsigned int> > getBlksPlacement(vector<string> lostBlks);
        vector<string> getBlksInIP(unsigned int ip);
        map<string, int> getBlkRelatedStripeStatus(const string& blkName);
        unsigned int getBlockIp(const string& blkName);
        int getBlockStripeId(const string& blkName);
        string getBlockStripeName(const string& blkName);
        int getBlkIdInStripe(const string& blkName);
        int* getLostBlkIdxInStripe(vector<string> lostBlks);
};


#endif // ! _HDFS3_METADATA_BASE_HH_