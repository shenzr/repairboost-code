#include "Coordinator.hh"

Coordinator::Coordinator(Config* conf) {
    _conf = conf;
    init();

    _handlerThrds = vector<thread>(_handlerThreadNum);
    _distThrds = vector<thread>(_distributorThreadNum);

}


void Coordinator::init() {
    _handlerThreadNum = _conf->_coCmdReqHandlerThreadNum;
    _distributorThreadNum = _conf->_coCmdDistThreadNum;
    _ecK = _conf->_ecK;
    _ecN = _conf->_ecN;
    _ecM = _ecN - _ecK;
    _ec_LRC_L = _conf->_lrcL;
    _ec_LRC_G = _ecM - _ec_LRC_L;
    _slaveCnt = _conf->_helpersIPs.size();

    if (COORDINATOR_DEBUG) cout << "# of slaves: " << _slaveCnt << endl; 


    vector<int> rightBounder;
    if (_slaveCnt % _distributorThreadNum == 0)
        rightBounder.push_back(_slaveCnt / _distributorThreadNum); //  # of slaves per thread
    else
        rightBounder.push_back(_slaveCnt / _distributorThreadNum + 1);

    int remaining = _slaveCnt % _distributorThreadNum, idx, rBounderId;
    for (int i = 1; i < _distributorThreadNum; i++) {
        if (i >= remaining)
            rightBounder.push_back(rightBounder.back() +
                                   _slaveCnt / _distributorThreadNum);
        else
            rightBounder.push_back(rightBounder.back() +
                                   _slaveCnt / _distributorThreadNum + 1);
    }

    puts("start to read the parameter.");

    idx = 0;
    rBounderId = 0;
    string prefix("disQ:");
    for (auto& it : _conf->_helpersIPs) { 
        if (++idx > rightBounder[rBounderId]) rBounderId++;
        _ip2Ctx.push_back({it, {prefix + to_string(rBounderId), initCtx(it)}});
    }
    _selfCtx = initCtx(_conf->_localIP);
    _ecConfigFile = _conf->_ecConfigFile;

    // add by Zuoru:
    printf("%s\n", _ecConfigFile.c_str());

    ifstream ifs(_ecConfigFile);
    _encMat = (int*)malloc(sizeof(int) * _ecM * _ecK);
    for (int i = 0; i < _ecM; i++) {
        for (int j = 0; j < _ecK; j++) {
            ifs >> _encMat[_ecK * i + j];
        }
    }
    ifs.close();
    if(_conf->_ecType == "RS") {
        puts("initializing rs utility");
        _rsUtil = new RSUtil(_conf, _encMat);
        puts("finish the parameter reading");
    } else if(_conf->_ecType == "LRC") {
        puts("initializing lrc utility");
        _lrcUtil = new LRCUtil(_conf, _encMat); 
        puts("finish the parameter reading for lrc");
    } else if(_conf->_ecType == "BUTTERFLY") {
        if(_ecN != _ecK + 2 || _conf->_packetCnt%(1<<(_ecK-1))!=0) {
            puts("parameters not satisfy BUTTERFLY CODE");
            exit(-1);
        }
        puts("initializing butterfly utility");
        _butterflyUtil = new BUTTERFLYUtil(_conf);
        puts("finish the parameter reading for butterfly");
    }
    

    puts("Start to create the MetadataBase!"); 
    // start metadatabase
    if (_conf->_fileSysType == "HDFS") {
        if (COORDINATOR_DEBUG)
            cout << "creating metadata base for HDFS " << endl;
        _metadataBase = new HDFS_MetadataBase(_conf, _rsUtil, _lrcUtil, _butterflyUtil);
    } else if (_conf->_fileSysType == "HDFS3") {
        if (COORDINATOR_DEBUG)
            cout << "creating metadata base for HDFS3 " << endl;
        _metadataBase = new HDFS3_MetadataBase(_conf, _rsUtil);
    }

    cout << typeid(this).name() << "::" << __func__ << "() ends\n";

}

redisContext* Coordinator::initCtx(unsigned int redisIP) {
    struct timeval timeout = {1, 500000};  // 1.5 seconds
    cout << "initCtx: connect to " << ip2Str(redisIP) << endl;
    redisContext* rContext =
        redisConnectWithTimeout(ip2Str(redisIP).c_str(), 6379, timeout);
    if (rContext == NULL || rContext->err) {
        if (rContext) {
            cerr << "Connection error: " << rContext->errstr << endl;
            redisFree(rContext);
        } else {
            cerr << "Connection error: can't allocate redis context at IP: "
                 << redisIP << endl;
        }
    }
    return rContext;
}

void Coordinator::doProcess() {
    // starts the threads
    for (int i = 0; i < _distributorThreadNum; i++) {
        _distThrds[i] =
            thread([=] { this->cmdDistributor(i, _distributorThreadNum); });
    }
    for (int i = 0; i < _handlerThreadNum; i++) {
        _handlerThrds[i] = thread([=] { this->requestHandler(); });
    }

    // should not reach here
    for (int i = 0; i < _distributorThreadNum; i++) {
        _distThrds[i].join();
    }
    for (int i = 0; i < _handlerThreadNum; i++) {
        _handlerThrds[i].join();
    }
}

/**
 * Since the _ip2Ctx is sorted, we just do a binary search,
 * which should be faster than unordered_map with a modest number of slaves
 *
 * TODO: We current assume that IP must be able to be found.  Should add error
 * handling afterwards
 */
int Coordinator::searchCtx(
    vector<pair<unsigned int, pair<string, redisContext*>>>& arr,
    unsigned int target, size_t sId, size_t eId) {
    int mid;
    while (sId < eId) {
        mid = (sId + eId) / 2;
        if (arr[mid].first < target)
            sId = mid + 1;
        else if (arr[mid].first > target)
            eId = mid - 1;
        else
            return mid;
    }
    return sId;
}

void Coordinator::cmdDistributor(int idx, int total) {
    printf("%s::%s() starts\n", typeid(this).name(), __func__);
    struct timeval linesStartTime;

    int startFlag = 0;
    int i, j, currIdx;
    struct timeval timeout = {1, 500000};  // 1.5 seconds
    string disQKey("disQ:");
    disQKey += to_string(idx);

    redisReply *rReply, *rReply2;

    // TODO: trying to switch communication to conditional variable
    redisContext *locCtx = redisConnectWithTimeout("127.0.0.1", 6379, timeout),
                 *opCtx;
    if (locCtx == NULL || locCtx->err) {
        if (locCtx) {
            cerr << "Connection error: " << locCtx->errstr << endl;
        } else {
            cerr << "Connection error: can't allocate redis context" << endl;
        }
        redisFree(locCtx);
        return;
    }

    while (1) {
        /* Redis command: BLPOP (LISTNAME1) [LISTNAME2 ...] TIMEOUT */
        rReply =
            (redisReply*)redisCommand(locCtx, "BLPOP %s 100", disQKey.c_str()); // [3]
        if (rReply->type == REDIS_REPLY_NIL) {
            cerr << "Coordinator::CmdDistributor() empty queue " << endl;
            freeReplyObject(rReply);
        } else if (rReply->type == REDIS_REPLY_ERROR) {
            cerr << "Coordinator::CmdDistributor() ERROR happens " << endl;
            freeReplyObject(rReply);
        } else {
            if(!startFlag) {
                gettimeofday(&linesStartTime, NULL);
                redisCommand(_selfCtx, "RPUSH beginTime %lld %lld", (long long)linesStartTime.tv_sec, (long long)linesStartTime.tv_usec);
                startFlag = 1;
            }

            memcpy((char*)&currIdx, rReply->element[1]->str, 4);
            opCtx = _ip2Ctx[currIdx].second.second;
            
            rReply2 = (redisReply*)redisCommand(opCtx, "RPUSH dr_cmds %b",
                                                rReply->element[1]->str + 4,
                                                rReply->element[1]->len - 4); 
            if (COORDINATOR_DEBUG)
                printf("%s: currIdx = %d, IP = .%d\n", __func__, currIdx, _ip2Ctx[currIdx].first >> 24);

            freeReplyObject(rReply2);
            freeReplyObject(rReply);
        }
    }
}


void Coordinator::requestHandler() {
    // now overrided by inherited classes
    // TODO: add ECPipe and PPR flag in Config.hh
    ;
}


