#include "LinesDefine.hh"
#include "LinesCoordinator.hh"

LinesDealScheduling lds;

bool repairTimeCmp(pair<int, pair<string, pair<char*, size_t>>> x, pair<int, pair<string, pair<char*, size_t>>> y) {
    return x.first < y.first;
}

LinesCoordinator::LinesCoordinator(Config* c) : Coordinator(c) {
    lds.initConf(_conf);
    _helperIp2Id.clear();
    _helperId2Ip.clear();
    for(int i=0; i<_conf->_helpersIPs.size(); ++i) {
        _helperId2Ip[i] = _conf->_helpersIPs[i];
        _helperIp2Id[_conf->_helpersIPs[i]] = i;
    }
    
}

void LinesCoordinator::requestHandler() {
    struct timeval timeout = {1, 500000};  // 1.5 seconds
    redisContext* rContext = redisConnectWithTimeout("127.0.0.1", 6379, timeout);
    if (rContext == NULL || rContext->err) {
        if (rContext) {
            cerr << "Connection error: " << rContext->errstr << endl;
            redisFree(rContext);
        } else {
            cerr << "Connection error: can't allocate redis context" << endl;
            redisFree(rContext);
        }
        return;
    }

    redisReply* rReply;
    char* reqStr;
    int reqLen;
    vector<pair<string, pair<char*, size_t>>> cmds; // (sendListName, (cmd, cmdLen)) 
    vector<pair<string, unsigned int>> lostFiles;
    struct timeval tv1, tv2;

    while (true) {
        if (COORDINATOR_DEBUG) cout << "waiting for requests ..." << endl;
        /* Redis command: BLPOP (LISTNAME1) [LISTNAME2 ...] TIMEOUT */
        rReply = (redisReply*)redisCommand(rContext, "BLPOP dr_request 100"); // [0]
        if (rReply->type == REDIS_REPLY_NIL) {
            cerr << __func__ << " empty queue " << endl;
            freeReplyObject(rReply);
            continue;
        } else if (rReply->type == REDIS_REPLY_ERROR) {
            cerr << __func__ << " ERROR happens " << endl;
            freeReplyObject(rReply);
            continue;
        } else {
            if((int)rReply->elements == 0) {
                cerr << __func__ << " rReply->elements = 0, ERROR " << endl;
                continue;
            }
            reqLen = rReply->element[1]->len;
            reqStr = (char*)malloc(sizeof(char)*reqLen);
            for(int i=0; i<reqLen; ++i) reqStr[i] = *(rReply->element[1]->str+i);

            int requestType;
            memcpy((char*)&requestType, reqStr, 4);
            freeReplyObject(rReply);
            if(requestType == CLIENT_FULLNODE_RECOVERY) {
                /* client format
                 * |<---request type(4Byte)--->|<---Requestor IP (4Byte)--->|
                 * request type: 0 dr; 1 fn;
                 */
                solveFullNodeRequest(reqStr+4, reqLen-4, rContext);
            }
        }
    }
    // should never end ...
}

void LinesCoordinator::parseDRRequest(char* reqStr, int reqLen, unsigned int& requestorIP, string& reqFile,
    vector<pair<string, unsigned int>>& lostFiles) {
    /* Lines request cmd [DR]
    * |<---Requestor IP (4Byte)--->|<---l_1(4Byte)--->|<---Lost filename 1 (l_1 Byte)--->|...|
    * |<---Lost filename f (l_f Byte)--->|
    */
    if (reqLen >= 4) memcpy((char*)&requestorIP, reqStr, 4);
    if(COORDINATOR_DEBUG)
        cout << typeid(this).name() << "::" << __func__ << "() from IP ." << (requestorIP >> 24) << " starts\n";
    
    int cmdoffset = 4, flen;
    unsigned int ip;
    while(cmdoffset < reqLen) {
        memcpy((char*)&flen, reqStr + cmdoffset, 4);
        string lostFile(reqStr + cmdoffset + 4, flen);
        ip = _metadataBase->getBlockIp(lostFile);
        lostFiles.push_back({lostFile, ip});
        cmdoffset += 4 + flen;
    }

    if(lostFiles.size()) reqFile = lostFiles[0].first;
}

void LinesCoordinator::getSurviHelperIds(map<string, int>* status, vector<pair<string, int> >& survivBlks) {
    survivBlks.clear();
    for(auto it : (*status)) {
        if(it.second) {
            survivBlks.push_back({it.first, _helperIp2Id[_metadataBase->getBlockIp(it.first)]});
        }
    }
    
}

void LinesCoordinator::solveFullNodeRequest(char* reqStr, int reqLen, redisContext* rContext) {
    if(COORDINATOR_DEBUG) printf("%s starts \n", __func__);

    unsigned int failNodeIP;
    vector<string> lostBlks;
    lostBlks.clear();
    parseFNRequest(reqStr, reqLen, failNodeIP, lostBlks);
    if (COORDINATOR_DEBUG) cout << "request recv'd: ip: " << failNodeIP << endl;

    int lostBlkCnt = lostBlks.size();   
    vector<pair<string, unsigned int> > placeIpInfo = _metadataBase->getBlksPlacement(lostBlks); // (blkName, ip)
    int* idxs = _metadataBase->getLostBlkIdxInStripe(lostBlks);
    int* placement = (int*)malloc(_ecN * lostBlkCnt * sizeof(int));
    int* isSoureCandidate = (int*)malloc(_ecN * lostBlkCnt * sizeof(int));

    int local_flag, global_flag;
    for(int i=0; i<lostBlkCnt; ++i) {
        local_flag = (_conf->_ecType == "LRC" && idxs[i] < _ecK+_ec_LRC_L);
        global_flag = (_conf->_ecType == "LRC" && idxs[i] >= _ecK+_ec_LRC_L);
        for(int j=0; j<_ecN; ++j) {
            placement[i*_ecN+j] = _helperIp2Id[placeIpInfo[i*_ecN+j].second];
            isSoureCandidate[i*_ecN+j] = 1;
            if(local_flag  && _conf->_group_id[j] != _conf->_group_id[idxs[i]]) isSoureCandidate[i*_ecN+j] = 0;
            if(global_flag && j >= _ecK && j < _ecK+_ec_LRC_L) isSoureCandidate[i*_ecN+j] = 0;
        }
    }
 
    int repairMethod = _conf->_chunkRepairMethod;
    int schedulMethod = SCHEDUL_METHOD_BOOST;
    vector<Stripe> stripes;
    if(schedulMethod == SCHEDUL_METHOD_BOOST) {
        stripes = lds.repairboost(_helperIp2Id[failNodeIP], repairMethod, lostBlkCnt, idxs, placement, isSoureCandidate);
    } else if(schedulMethod == SCHEDUL_METHOD_RANDOM) {
        stripes = lds.random_select(_helperIp2Id[failNodeIP], repairMethod, lostBlkCnt, idxs, placement, isSoureCandidate);
    } else if(schedulMethod == SCHEDUL_METHOD_LRU) {  
        stripes = lds.least_recent_selected(_helperIp2Id[failNodeIP], repairMethod, lostBlkCnt, idxs, placement, isSoureCandidate);
    } 
 

    // -- if HDFS3, send targets info to hdfs-3.1.4
    string blkName;
    char* targetCmd;
    redisReply* rReply;
    if(_conf->_fileSysType == "HDFS3") {
        int src, dest;
        int finishTime, real_stripe_id;
        vector<pair<int, int> > orderedTargets; // (finishTime, stripeid)
        orderedTargets.clear();
        // send targets in asc order 
        for(int i=0; i<lostBlkCnt; ++i) {
            dest = stripes[i].rG_bp._node_cnt - 1;
            finishTime = -1;
            for(int j=1; j<=stripes[i].rG_bp.in[dest][0]; ++j) {
                src = stripes[i].rG_bp.in[dest][j];
                finishTime = max(finishTime, stripes[i].repairTime[src][dest]);
            }
            orderedTargets.push_back({finishTime, i});
        }
        sort(orderedTargets.begin(), orderedTargets.end());

        for(int i=0; i<lostBlkCnt; ++i) {
            real_stripe_id = orderedTargets[i].second;
            targetCmd = (char*)calloc(sizeof(char), COMMAND_MAX_LENGTH); 
            int helperId = stripes[real_stripe_id].vertex_to_peerNode[stripes[real_stripe_id].rG._node_cnt-1];
            memcpy(targetCmd, (char*)&(_helperId2Ip[helperId]), 4);
            // blkName & targetDataNode
            // blkName is not lostBlkName, but stripeID
            blkName = _metadataBase->getBlockStripeName(lostBlks[real_stripe_id]);
            rReply = (redisReply*)redisCommand(_selfCtx, "RPUSH HDFS3_blkName %b", blkName.c_str(), blkName.length());
            freeReplyObject(rReply);

            rReply = (redisReply*)redisCommand(_selfCtx, "RPUSH HDFS3_target %b", targetCmd, 4);
            freeReplyObject(rReply);
       }
   }

    /** cmd format (for all single_policy, including conv,ppr...)
     * | ctxID(4Byte) (for cmdDistributor)| ecK(4Byte) | id(4Byte) | coefficient(4Byte)|
     * | pre_cnt(4Byte) | pre 0 IP(4Byte) |...| pre pre_cnt-1 IP (4Byte) |    
     * | next_cnt(4Byte) | next 0 IP(4Byte) |...| next next_cnt-1 IP(4Byte) | 
     * | lostFileNameLen(4Byte) | lostFIleName(l) | 
     * | localFileNameLen(4Btye) | localFileName(l') | 
     */
    vector<pair<int, pair<string, pair<char*, size_t>>>> cmds[_slaveCnt]; // (repairTime, (sendListName, (cmd, cmdLen))
    for(int i=0; i<_slaveCnt; ++i) cmds[i].clear();

    int cmdoffset;
    char* fnCmd;
    string localBlkName;
    map<string, int> coef, status;
    map<int, string> pid2Blk; // pid: peerNodeId/helperId
    int pid, vid, llen, pre_cnt, next_cnt, send_time, dest_time;
    vector<pair<int, pair<string, int>>> blk2CompleteTime;
    blk2CompleteTime.clear();
    vector<int> slave_send_stripe_id[_slaveCnt]; 
    for(int i=0; i<lostBlkCnt; ++i) {
        dest_time = 0;
        status.clear();
        pid2Blk.clear();
        for(int j=0; j<_ecN; ++j) 
            pid2Blk[_helperIp2Id[placeIpInfo[i*_ecN+j].second]] = placeIpInfo[i*_ecN+j].first;
        for(int j=0; j<stripes[i].rG_bp._node_cnt-1; ++j) { // k
            pid = stripes[i].vertex_to_peerNode[j];
            status[pid2Blk[pid]] = COEF_SELECT_STATUS;
        }
        coef = _metadataBase->getCoefficient(lostBlks[i], &status);

        if(_conf->_ecType == "BUTTERFLY") {
            int ncnt = stripes[i].rG_bp._node_cnt;
            int tmp;
            pid = stripes[i].vertex_to_peerNode[stripes[i].rG_bp._node_cnt-1];
            string s = pid2Blk[pid];
            coef[s] = 0;
            for(int c=0, off=15; c<ncnt-1; ++c, off-=3) {
                pid = stripes[i].vertex_to_peerNode[c];
                tmp = _metadataBase->getBlkIdInStripe(pid2Blk[pid]);
                coef[s] |= tmp << off;
            }
            coef[s] |= _metadataBase->getBlkIdInStripe(lostBlks[i]);
        }

        for(int j=0; j<stripes[i].rG_bp._node_cnt; ++j) {
            fnCmd = (char*)calloc(sizeof(char), COMMAND_MAX_LENGTH); 
            pid = stripes[i].vertex_to_peerNode[j];
            int ctxId = searchCtx(_ip2Ctx, _helperId2Ip[pid], 0, _slaveCnt-1);
            localBlkName = pid2Blk[pid];

            memcpy(fnCmd, (char*)&(ctxId), 4); // ctxID
            int retrieveNum = stripes[i].rG_bp._node_cnt-1;
            memcpy(fnCmd+4, (char*)&retrieveNum, 4);
            memcpy(fnCmd+8, (char*)&j, 4); // id
            memcpy(fnCmd+12, (char*)&(coef[localBlkName]), 4); // coef

            pre_cnt = stripes[i].rG_bp.in[j][0]; // pre_cnt
            memcpy(fnCmd+16, (char*)&pre_cnt, 4);
            cmdoffset = 20;
            for(int q=1; q<=stripes[i].rG_bp.in[j][0]; ++q) {
                vid = stripes[i].rG_bp.in[j][q];
                memcpy(fnCmd+cmdoffset, (char*)&(_helperId2Ip[stripes[i].vertex_to_peerNode[vid]]), 4); // pre ip
                cmdoffset += 4;
            }
 
            next_cnt = stripes[i].rG_bp.out[j][0];
            memcpy(fnCmd+cmdoffset, (char*)&next_cnt, 4);
            cmdoffset += 4;
            for(int q=1; q<=stripes[i].rG_bp.out[j][0]; ++q) {
                vid = stripes[i].rG_bp.out[j][q];
                memcpy(fnCmd+cmdoffset, (char*)&(_helperId2Ip[stripes[i].vertex_to_peerNode[vid]]), 4); // next ip
                cmdoffset += 4;
            }
            llen = lostBlks[i].length();
            memcpy(fnCmd+cmdoffset, (char*)&llen, 4); // lostBlkNameLen
            memcpy(fnCmd+cmdoffset+4, lostBlks[i].c_str(), llen); 
            cmdoffset += 4+llen;
            llen = localBlkName.length();
            memcpy(fnCmd+cmdoffset, (char*)&llen, 4); // localBlkNameLen
            memcpy(fnCmd+cmdoffset+4, localBlkName.c_str(), llen); 
            cmdoffset += 4+llen;

            if(next_cnt) {
                send_time = stripes[i].repairTime[j][stripes[i].rG_bp.out[j][1]];
                if(stripes[i].rG_bp.out[j][1] == stripes[i].rG_bp._node_cnt-1) 
                    dest_time = max(dest_time, send_time + 1);
            } else {
                assert(j == stripes[i].rG_bp._node_cnt-1);
                send_time = dest_time;
                blk2CompleteTime.push_back({send_time, {lostBlks[i], i}});
            } 
            slave_send_stripe_id[pid].push_back(i);
            cmds[pid].push_back({send_time, {_ip2Ctx[ctxId].second.first, {fnCmd, cmdoffset} } }); // (repairTime, (sendListName, (cmd, cmdLen))
        }
    }

    if(schedulMethod == SCHEDUL_METHOD_BOOST) { // [tocheck]
        cout << " !!!!!! " << "sort cmds for each slave by repairTime " << endl;
        for(int i=0; i<_slaveCnt; ++i) 
            sort(cmds[i].begin(), cmds[i].end(), repairTimeCmp);
    }
        
    redisAppendCommand(rContext, "MULTI");
 
    for(int i=0; i<_slaveCnt; ++i) {
        for(int j=0; j<cmds[i].size(); ++j) {
            redisAppendCommand(rContext, "RPUSH %s %b", cmds[i][j].second.first.c_str(), cmds[i][j].second.second.first, cmds[i][j].second.second.second);
        }
    }

    redisAppendCommand(rContext, "EXEC");
    redisGetReply(rContext, (void**)&rReply); // execute "MULTI"
    freeReplyObject(rReply);
    for(int i=0; i<_slaveCnt; ++i) {
        for(auto c : cmds[i]) {
            redisGetReply(rContext, (void**)&rReply);
            freeReplyObject(rReply);
            free(c.second.second.first);
        }
    }
    redisGetReply(rContext, (void**)&rReply); // execute "EXEC"
    freeReplyObject(rReply);

    int ctxId = searchCtx(_ip2Ctx, failNodeIP, 0, _slaveCnt - 1);;
    redisContext* rCtx = _ip2Ctx[ctxId].second.second;
    redisAppendCommand(rCtx, "MULTI");
    sort(blk2CompleteTime.begin(), blk2CompleteTime.end());
    for(auto &b : blk2CompleteTime) {
        fnCmd = (char*)calloc(sizeof(char), COMMAND_MAX_LENGTH); 
        llen = b.second.first.length();
        pid = stripes[b.second.second].vertex_to_peerNode[stripes[b.second.second].rG_bp._node_cnt-1];
        memcpy(fnCmd, (char*)&llen, 4);
        memcpy(fnCmd+4, b.second.first.c_str(), llen);
        memcpy(fnCmd+4+llen, (char*)&(_helperId2Ip[pid]), 4); // destNode ip
        redisAppendCommand(rCtx, "RPUSH allLostBlks %b", fnCmd, 8+llen);
    }
    fnCmd = (char*)calloc(sizeof(char), COMMAND_MAX_LENGTH); 
    llen = -1;
    memcpy(fnCmd, (char*)&llen, 4);
    redisAppendCommand(rCtx, "RPUSH allLostBlks %b", fnCmd, 4); // for stop 

    redisAppendCommand(rCtx, "EXEC");
    redisGetReply(rCtx, (void**)&rReply); // execute "MULTI"
    freeReplyObject(rReply);

    for(int i=0; i<lostBlks.size() + 1; ++i) { // +1 for stop
        redisGetReply(rCtx, (void**)&rReply);
        freeReplyObject(rReply);
    }

    redisGetReply(rCtx, (void**)&rReply); // execute "EXEC"
    freeReplyObject(rReply);

    if(COORDINATOR_DEBUG) printf("%s end \n", __func__);

}

void LinesCoordinator::parseFNRequest(char* reqStr, int reqLen, unsigned int& failNodeIP, vector<string>& lostBlks) {
    /* client format: [full-node-recovery]
     * |<---Requestor IP (4Byte)--->|
     */
    if (reqLen >= 4) memcpy((char*)&failNodeIP, reqStr, 4);
    cout << typeid(this).name() << "::" << __func__ << "() from IP ." << (failNodeIP >> 24) << " starts\n";
    lostBlks = _metadataBase->getBlksInIP(failNodeIP);

    if(COORDINATOR_DEBUG) {
        cout << "all blks in fail node : " << lostBlks.size() << endl;
        for(int i=0; i<lostBlks.size(); ++i) {
            cout << lostBlks[i] << " ";
        }
        cout << endl;
    }

}



