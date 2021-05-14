#include "HDFS3_MetadataBase.hh"

HDFS3_MetadataBase::HDFS3_MetadataBase(Config* conf, RSUtil* rsu)
    : MetadataBase(conf, rsu) {
    _metadataThread = thread([=] { GetMetadata(); });
    _metadataThread.join();
}

void HDFS3_MetadataBase::GetMetadata() {
    // get the metadatabase by using hdfs fsck

    if (METADATA_BASE_DEBUG) cout << __func__ << " starts:\n";

    string cmdResult;
    string cmdFsck("hdfs fsck / -files -blocks -locations");
    FILE* pipe = popen(cmdFsck.c_str(), "r");

    if (!pipe) cerr << "ERROR when using hdfs fsck" << endl;
    char cmdBuffer[256];
    while (!feof(pipe)) {
        if (fgets(cmdBuffer, 256, pipe) != NULL) {
            cmdResult += cmdBuffer; 
        }
    }
    pclose(pipe);
    if (METADATA_BASE_DEBUG) cerr << "Get the Metadata successfully" << endl;

    int stripeNum = 0;
    _blk2Ip.clear();
    _ip2BlkNames.clear();
    _blk2StripeId.clear();
    _blkIdInStripe.clear();
    _allBlksInStripe.clear();
    _stripeId2StripeName.clear();
    

    set<string> allBlks;
    /* parse the metadata info from hdfs fsck */
    int stripeIdInt;
    string blkName, stripeId, ipAdd;
    size_t currentPos, endPos, ipLength;
    size_t startPos = cmdResult.find("Live_repl");

    for(int i=0; i<_conf->_helpersIPs.size(); ++i) {
        _ip2BlkNames[_conf->_helpersIPs[i]] = vector<string>();
    }

    while(true) {
        if(startPos == string::npos) break;
        currentPos = cmdResult.find_last_of(":", startPos);
        stripeId = cmdResult.substr(currentPos+1, 29);
        if(METADATA_BASE_DEBUG)
            cout << "find the stripe, stripeId = " << stripeId << endl;
        stripeIdInt = stripeNum++;
        _stripeId2StripeName[stripeIdInt] = stripeId.substr(0, 24);
        cout << "blkName in HDFS3 : " << _stripeId2StripeName[stripeIdInt] << endl;
        _allBlksInStripe.push_back(vector<string>());

        for(int i=0; i<_conf->_ecN; ++i) {
            currentPos = cmdResult.find("blk_", startPos);
            blkName = cmdResult.substr(currentPos, 24);
            if(METADATA_BASE_DEBUG) cout << "blkName: " << blkName << endl;
            currentPos = cmdResult.find("[", currentPos);
            endPos = cmdResult.find(":", currentPos);
            ipLength = endPos - currentPos - 1;
            ipAdd = cmdResult.substr(currentPos + 1, ipLength);
            startPos = endPos;

            allBlks.insert(blkName);
            _allBlksInStripe[stripeIdInt].push_back(blkName);   
            _blkIdInStripe[blkName] = i;
            _blk2StripeId[blkName] = stripeIdInt;
            _blk2Ip[blkName] = inet_addr(ipAdd.c_str());
            _ip2BlkNames[inet_addr(ipAdd.c_str())].push_back(blkName);
            if(METADATA_BASE_DEBUG) cout << "ip: " << ipAdd << endl;
        }

        startPos = cmdResult.find("Live_repl", startPos + 9);

    }
    if(METADATA_BASE_DEBUG) {
        cout << "the amounf of stripes: " << _allBlksInStripe.size() << endl;
    }
    
    // -- receive blockReport from slaves
    struct timeval timeout = {1, 500000};  // 1.5 seconds
    redisContext* rContext =
        redisConnectWithTimeout("127.0.0.1", 6379, timeout);
    if (rContext == NULL || rContext->err) {
        if (rContext) {
            cerr << "Connection error: " << rContext->errstr << endl;
            redisFree(rContext);
        } else {
            cerr << "Connection error: can't allocate redis context" << endl;
        }
        return;
    }

    redisReply* rReply;
    unsigned int holderIP;
    for(int i=0; i<_conf->_helpersIPs.size(); ++i) {
        _ip2BlkNames[_conf->_helpersIPs[i]] = vector<string>();
    }
    while (true) { 
        rReply = (redisReply*)redisCommand(rContext, "BLPOP blk_init 100");
        if (rReply->type == REDIS_REPLY_NIL) {
            cerr << "HDFS_MetadataBase::HDFSinit() empty queue " << endl;
            freeReplyObject(rReply);
            continue;
        } else if (rReply->type == REDIS_REPLY_ERROR) {
            cerr << "HDFS_MetadataBase::HDFSinit() ERROR happens " << endl;
            freeReplyObject(rReply);
            continue;
        } else {
            /**
             * cmd format:
             * |<----IP (4 bytes)---->|<-----file name (?Byte)----->|
             */
            memcpy((char*)&holderIP, rReply->element[1]->str, 4);
            blkName = string(rReply->element[1]->str + 4);
            
            freeReplyObject(rReply);
            if (allBlks.find(blkName) == allBlks.end()) continue;
            _blk2Ip[blkName] = holderIP;
            _ip2BlkNames[holderIP].push_back(blkName);
            _blkStatus[blkName] = 1;
            if (METADATA_BASE_DEBUG) {
                cout << "HDFS_MetadataBase::HDFSinit() getting blk: " << blkName << endl;
                cout << "_blk2Ip " << blkName << " " << holderIP << endl;
            }
            if (_blk2StripeId.count(blkName)) {
                int stripeId = _blk2StripeId[blkName];
                if (METADATA_BASE_DEBUG) {
                    printf("%s: stripeId = %d, IP = .%d, blkName = %s\n", __func__, stripeId, holderIP >> 24, blkName.c_str());
                }
            }
            
            allBlks.erase(blkName);
            if (allBlks.empty()) break;
        }
    }
    if (METADATA_BASE_DEBUG) cout << __func__ << " ends." << endl;
}


map<string, int> HDFS3_MetadataBase::getCoefficient(const string& blkName, map<string, int>* status) {
    int* int_status = (int*)calloc(_conf->_ecN, sizeof(int));
    map<int, string> id2BlkName;
    id2BlkName.clear();
    for (auto it : (*status)) {
        if (it.second) {
            int_status[_blkIdInStripe[it.first]] = 1;
            id2BlkName[_blkIdInStripe[it.first]] = it.first;
        }
    }
    int idx = _blkIdInStripe[blkName];

    int* coef = _rsUtil->getCoefficient_specifiedBlks(idx, int_status);

    map<string, int> coeff;
    int cnt = 0;
    for(int i=0; i<_conf->_ecN; ++i) {
        if(int_status[i]) {
            if (coef == nullptr) {
                printf("coef is empty\n");
                return {};
            }
            coeff[id2BlkName[i]] = coef[cnt++];
        }
    }

    return coeff;
}
vector<pair<string, unsigned int> > HDFS3_MetadataBase::getBlksPlacement(vector<string> lostBlks) {
    int sz = lostBlks.size();
  
    vector<pair<string, unsigned int> > placeIpInfo(_conf->_ecN * sz);
    vector<string> stripe;
    int bid, stripeId;
    for(int i=0; i<sz; ++i) {
        stripeId = _blk2StripeId[lostBlks[i]];
        stripe = _allBlksInStripe[stripeId];
        for(int j=0; j<stripe.size(); ++j) {
            placeIpInfo[i * _conf->_ecN + j] = {stripe[j], _blk2Ip[stripe[j]]};
        }
    }
    return placeIpInfo;
}

map<string, int> HDFS3_MetadataBase::getBlkRelatedStripeStatus(const string& blkName) {
    int stripeId = _blk2StripeId[blkName];

    map<string, int> status;
    for(auto blk : _allBlksInStripe[stripeId]) {
        status[blk] = _blkStatus[blk];
    }
    return status;
}

vector<string> HDFS3_MetadataBase::getBlksInIP(unsigned int ip) {
    return _ip2BlkNames[ip];
}

unsigned int HDFS3_MetadataBase::getBlockIp(const string& blkName) {
    if (!_blk2Ip.count(blkName)) {
        printf("%s: no block %s !\n", __func__, blkName.c_str());
        exit(1);
    }
    return _blk2Ip[blkName];
}

int HDFS3_MetadataBase::getBlockStripeId(const string& blkName) {
    return _blk2StripeId[blkName];
}

string HDFS3_MetadataBase::getBlockStripeName(const string& blkName) {
    return _stripeId2StripeName[getBlockStripeId(blkName)];
}

int HDFS3_MetadataBase::getBlkIdInStripe(const string& blkName) {
    return _blkIdInStripe[blkName];
}

int* HDFS3_MetadataBase::getLostBlkIdxInStripe(vector<string> lostBlks) {
    int sz = lostBlks.size();
    int* idxs = (int*)calloc(sz, sizeof(int));
    for(int i=0; i<sz; ++i) {
        idxs[i] = _blkIdInStripe[lostBlks[i]];
    }
    return idxs;
}