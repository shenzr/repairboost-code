#include "LinesFullNodeClientStream.hh"

LinesFullNodeClientStream::~LinesFullNodeClientStream() {
    if (_coordinatorCtx) {
        redisFree(_coordinatorCtx);
        _coordinatorCtx = 0;
    }
    if (_selfCtx) {
        redisFree(_selfCtx);
        _selfCtx = 0;
    }
}

LinesFullNodeClientStream::LinesFullNodeClientStream(Config* conf, size_t packetCnt,
                                     size_t packetSize,
                                     unsigned int coorIP, unsigned int locIP)
    : _conf(conf),
      _packetCnt(packetCnt),
      _packetSize(packetSize),
      _locIP(locIP) {

    initRedis(_coordinatorCtx, coorIP, 6379);
    initRedis(_selfCtx, 0x0100007F, 6379);  // 127.0.0.1

    _slaveCtx.clear();
    for (auto hip : _conf->_helpersIPs) {
        redisContext* tmpCtx;
        initRedis(tmpCtx, hip, 6379);
        _slaveCtx.push_back({hip, tmpCtx});
    }
    sendFullNodeRequest();
}

void LinesFullNodeClientStream::sendFullNodeRequest() {
    if (EIS_DEBUG) printf("%s::%s() starts\n", typeid(this).name(), __func__);
    assert(_coordinatorCtx && _selfCtx);

    sendCmdToCoordinator();
    pipeCollector();

    if (EIS_DEBUG) printf("%s::%s() end\n", typeid(this).name(), __func__);
}

void LinesFullNodeClientStream::sendCmdToCoordinator() {
    if (EIS_DEBUG) printf("%s::%s() starts\n", typeid(this).name(), __func__);

    char cmd[COMMAND_MAX_LENGTH];
    redisReply* rReply;

    /* cmd to coordinator format: (full-node-recovery)
     * |<---request type(4Byte)--->|<---Requestor IP (4Byte)--->|
     * request type: 0 dr; 1 fn;
     */
    int clientType = CLIENT_FULLNODE_RECOVERY;
    memcpy(cmd, (char*)&(clientType), 4);
    memcpy(cmd + 4, (char*)&_locIP, 4);
    int cmdoffset = 8;

    redisAppendCommand(_coordinatorCtx, "RPUSH dr_request %b", cmd, cmdoffset);
    redisGetReply(_coordinatorCtx, (void**)&rReply); // send "dr_request"
    freeReplyObject(rReply);
    assert(_coordinatorCtx);
    
    char* replyStr = rReply->element[1]->str;

    if (EIS_DEBUG) printf("%s::%s() end\n", typeid(this).name(), __func__);
}


void LinesFullNodeClientStream::pipeCollector() {
    redisReply* rReply;
    int repLen;
    char repStr[COMMAND_MAX_LENGTH];
    vector<pair<string, unsigned int>> lostBlks;
    lostBlks.clear();
    int llen;
    unsigned int fetchIP;
    while(true) {
        rReply = (redisReply*)redisCommand(_selfCtx, "BLPOP allLostBlks 100");
        if(rReply->type == REDIS_REPLY_NIL) {
            if (EIS_DEBUG)
                cout << typeid(this).name() << "::" << __func__ << "(): empty list" << endl;
        } else if(rReply->type == REDIS_REPLY_ERROR) {
            if (EIS_DEBUG)
                cout << typeid(this).name() << "::" << __func__ << "(): error happens" << endl;
        } else {
            repLen = rReply->element[1]->len;
            memcpy(repStr, rReply->element[1]->str, repLen);
            // llen blkName destIp(fetchIP)
            memcpy((char*)&llen, repStr, 4);
            if(llen == -1) {
                freeReplyObject(rReply);
                break; // for stop
            }
            memcpy((char*)&fetchIP, repStr+4+llen, 4);
            lostBlks.push_back({string(repStr+4, llen), fetchIP});

            freeReplyObject(rReply);
        }
    }
    cout << "lostBlkCnt in ip ." << (_locIP>>24) << " = " << lostBlks.size() << endl;
    cout << "allLostBlks : " << endl;
    for(int i=0; i<lostBlks.size(); ++i)  // lostBlk alreay sorted by the complete time 
        cout << lostBlks[i].first << " " <<  (lostBlks[i].second >> 24) << endl;
    

    redisContext* pipeCtx;
    for(int i=0; i<lostBlks.size(); ++i) {
        _contents = (char**)malloc(_packetCnt * sizeof(char*));
        pipeCtx = findCtx(lostBlks[i].second);
        assert(pipeCtx);
        rReply = (redisReply*)redisCommand(pipeCtx, "BLPOP %s 0", lostBlks[i].first.c_str());
        freeReplyObject(rReply);
    }

    // calTime
    char cc[COMMAND_MAX_LENGTH];
    redisReply* rReply2;
    long long endTimeSec = -1, endTimeUsec = -1, tmp;
    for(int i=0; i<_slaveCtx.size(); ++i) {
        rReply = (redisReply*)redisCommand(_slaveCtx[i].second, "LLEN time-sec");
        int tnum = rReply->integer;
 
        if(tnum) {
            int f = 0;
            rReply2 = (redisReply*)redisCommand(_slaveCtx[i].second, "RPOP time-sec");
            memcpy(cc, rReply2->str, rReply2->len);
            // str -> long long tmp
            tmp = atoll(string(cc, rReply2->len).c_str());
            if(endTimeSec < tmp) {
                endTimeSec = tmp;
                f = 1;
            } else if(endTimeSec == tmp) {
                f = -1;
            }
            freeReplyObject(rReply2);

            rReply2 = (redisReply*)redisCommand(_slaveCtx[i].second, "RPOP time-usec");
            if(f) {
                memcpy(cc, rReply2->str, rReply2->len);
                // str -> long long tmp
                tmp = atoll(string(cc, rReply2->len).c_str());
                endTimeUsec = (f == -1) ? max(endTimeUsec, tmp): tmp;
            }
            freeReplyObject(rReply2);
        }
        freeReplyObject(rReply);
    }

    rReply = (redisReply*)redisCommand(_coordinatorCtx, "LLEN beginTime");
    assert(rReply->integer >= 2);
    freeReplyObject(rReply);
    long long beginSec, beginUsec, cmdEndSec, cmdEndUsec;
    rReply2 = (redisReply*)redisCommand(_coordinatorCtx, "LPOP beginTime");
    memcpy(cc, rReply2->str, rReply2->len);
    beginSec = atoll(string(cc, rReply2->len).c_str());
    freeReplyObject(rReply2);
    rReply2 = (redisReply*)redisCommand(_coordinatorCtx, "LPOP beginTime");
    memcpy(cc, rReply2->str, rReply2->len);
    beginUsec = atoll(string(cc, rReply2->len).c_str());
    freeReplyObject(rReply2);
    double durationTime = endTimeSec - beginSec + (endTimeUsec - beginUsec) * 1.0 / 1000000;
    // printf("duration time = %.6f\n", durationTime);

}

void LinesFullNodeClientStream::initRedis(redisContext*& ctx, unsigned int ip,
                                  unsigned short port) {
    if (EIS_DEBUG) cout << "Initializing IP: " << ip2Str(ip) << endl;
    struct timeval timeout = {1, 500000};
    char ipStr[INET_ADDRSTRLEN];

    ctx = redisConnectWithTimeout(
        inet_ntop(AF_INET, &ip, ipStr, INET_ADDRSTRLEN), port, timeout);
    if (ctx == NULL || ctx->err) {
        if (ctx) {
            cerr << "Connection error: redis host: " << ipStr << ":" << port
                 << ", error msg: " << ctx->errstr << endl;
            redisFree(ctx);
            ctx = 0;
        } else {
            cerr << "Connection error: can't allocate redis context at IP: "
                 << ipStr << endl;
        }
    }
}

redisContext* LinesFullNodeClientStream::findCtx(unsigned int ip) {
    int sid = 0, eid = _slaveCtx.size() - 1, mid;
    while (sid < eid) {
        mid = (sid + eid) / 2;
        if (_slaveCtx[mid].first == ip) {
            return _slaveCtx[mid].second;
        } else if (_slaveCtx[mid].first < ip) {
            sid = mid + 1;
        } else {
            eid = mid - 1;
        }
    }
    return _slaveCtx[sid].first == ip ? _slaveCtx[sid].second : NULL;
}


