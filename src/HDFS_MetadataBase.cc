#include "HDFS_MetadataBase.hh"

HDFS_MetadataBase::HDFS_MetadataBase(Config* conf, RSUtil* rsu, LRCUtil* lrcu, BUTTERFLYUtil* bu)
    : MetadataBase(conf, rsu, lrcu, bu) {
    DIR* dir;
    FILE* file;
    struct dirent* ent;
    bool newStripe;
    int start, pos, stripeId, stripeNum = 0;
    unsigned int idxInStripe;
    string fileName, blkName, bName, rawStripe, stripeStore = _conf->_stripeStore;
    set<string> blks;

    _blk2Ip.clear();
    _blkStatus.clear();
    _blkIdInStripe.clear();
    _allBlksInStripe.clear();
    _blk2StripeId.clear();
    _ip2BlkNames.clear();
   
    if ((dir = opendir(stripeStore.c_str())) != NULL) { // read meta from meta.stripe.dir 
        while ((ent = readdir(dir)) != NULL) {
            if (METADATA_BASE_DEBUG) cout << "filename: " << ent->d_name << endl;
            if (strcmp(ent->d_name, ".") == 0 ||
                strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            newStripe = false;

            fileName = string(ent->d_name);
            blkName = fileName.substr(fileName.find(':') + 1); 
            blkName = blkName.substr(0, blkName.find_last_of('_')); 
            // TODO: remove duplicates
            blks.insert(blkName);
            ifstream ifs(stripeStore + "/" + string(ent->d_name));
            rawStripe = string(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>()); 
            if (METADATA_BASE_DEBUG) cout << rawStripe << endl;
            ifs.close();

            stripeId = -1;
            if (!_blk2StripeId.count(blkName))  {
                stripeId = stripeNum++;
                newStripe = true;
                _allBlksInStripe.push_back(vector<string>());
            }

            start = 0;
            idxInStripe = 0;
            // remove \newline
            if (rawStripe.back() == 10) rawStripe.pop_back();

            int bidInStripe;
            while (true) {
                pos = rawStripe.find(':', start);
                if (pos == string::npos)
                    bName = rawStripe.substr(start);
                else
                    bName = rawStripe.substr(start, pos - start);
                bName = bName.substr(0, bName.find_last_of('_'));
                if (METADATA_BASE_DEBUG)
                    cout << "blkName: " << blkName << " bName: " << bName << " stripeId: " << stripeId << endl;

                if (newStripe) {
                    _blkStatus[bName] = 0;
                    _blk2StripeId[bName] = stripeId;
                    _allBlksInStripe[stripeId].push_back(bName);
                }
                
                if(bName == blkName) {
                    bidInStripe = _blkIdInStripe[blkName] = idxInStripe;
                }
                idxInStripe++;

                if (pos == string::npos) break;
                start = pos + 1;
            }

            if (METADATA_BASE_DEBUG && newStripe) {
                cout << "stripe "<< stripeId << ": " << endl;
                for(int i=0; i<_allBlksInStripe[stripeId].size(); ++i) {
                    cout << _allBlksInStripe[stripeId][i] << " ";
                }
                cout << endl;
            }

        }
        closedir(dir);
    } else {
        // TODO: error handling
        ;
        cout << "stripeStore empty!" << endl;
    }
    
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
            if (blks.find(blkName) == blks.end()) continue;
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
            
            blks.erase(blkName);
            if (blks.empty()) break;
        }
    }
    cout << __func__ << "finished!" << endl;
}

vector<string> HDFS_MetadataBase::getBlksInIP(unsigned int ip) {
    return _ip2BlkNames[ip]; 
}

int HDFS_MetadataBase::getBlockStripeId(const string& blkName) {
    return _blk2StripeId[blkName];
}

map<string, int> HDFS_MetadataBase::getCoefficient(const string& blkName, map<string, int>* status) {
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

    int* coef;
    if(_conf->_ecType == "RS")
        coef = _rsUtil->getCoefficient_specifiedBlks(idx, int_status);
    else if(_conf->_ecType == "LRC")
        coef = _lrcUtil->getCoefficient_specifiedBlks(idx, int_status);
    else if(_conf->_ecType == "BUTTERFLY") 
        coef = _butterflyUtil->getCoefficient(idx);

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

unsigned int HDFS_MetadataBase::getBlockIp(const string& blkName) {
    if (!_blk2Ip.count(blkName)) {
        printf("%s: no block %s !\n", __func__, blkName.c_str());
        exit(1);
    }
    return _blk2Ip[blkName];
}

map<string, int> HDFS_MetadataBase::getBlkRelatedStripeStatus(const string& blkName) {
    int stripeId = _blk2StripeId[blkName];

    map<string, int> status;
    for(auto blk : _allBlksInStripe[stripeId]) {
        status[blk] = _blkStatus[blk];
    }
    return status;
}

vector<pair<string, unsigned int> > HDFS_MetadataBase::getBlksPlacement(vector<string> lostBlks) {
    int ecN = _conf->_ecN;
    int sz = lostBlks.size();

    vector<pair<string, unsigned int> > placeIpInfo(ecN * sz);
    vector<string> stripe;
    int bid, stripeId;
    for(int i=0; i<sz; ++i) {
        stripeId = _blk2StripeId[lostBlks[i]];
        stripe = _allBlksInStripe[stripeId];
        for(int j=0; j<stripe.size(); ++j) {
            placeIpInfo[i * ecN + j] = {stripe[j], _blk2Ip[stripe[j]]};
        }
    }
    return placeIpInfo;
}

string HDFS_MetadataBase::getBlockStripeName(const string& blkName) {
    return "";
}

int HDFS_MetadataBase::getBlkIdInStripe(const string& blkName) {
    return _blkIdInStripe[blkName];
}

int* HDFS_MetadataBase::getLostBlkIdxInStripe(vector<string> lostBlks) {
    int sz = lostBlks.size();
    int* idxs = (int*)calloc(sz, sizeof(int));
    for(int i=0; i<sz; ++i) {
        idxs[i] = _blkIdInStripe[lostBlks[i]];
    }
    return idxs;
}
