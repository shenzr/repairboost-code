#include "LinesFullNodeWorker.hh"

LinesFullNodeWorker::LinesFullNodeWorker(Config* conf) : DRWorker(conf) {
    _butterflyUtil = new BUTTERFLYUtil(conf);
    if(_conf->_ecType == "BUTTERFLY") {
        _butterflyUtil->generate_decoding_matrix(_decMatButterFly);
    }
}

void LinesFullNodeWorker::sendOnly(redisContext* selfCtx) {
    redisReply* rReply;
    int repLen;
    char repStr[COMMAND_MAX_LENGTH];
    string lostBlkName, localBlkName;

    int ecK, coef;
    struct timeval t1, t2;
    while(true) {
        rReply = (redisReply*)redisCommand(selfCtx, "BLPOP cmd_send_only 100");
        if(rReply->type == REDIS_REPLY_NIL) {
            if (DR_WORKER_DEBUG)
                cout << typeid(this).name() << "::" << __func__ << "(): empty list" << endl;
        } else if(rReply->type == REDIS_REPLY_ERROR) {
            if (DR_WORKER_DEBUG)
                cout << typeid(this).name() << "::" << __func__ << "(): error happens" << endl;
        } else {
            repLen = rReply->element[1]->len;
            memcpy(repStr, rReply->element[1]->str, repLen);
            /** parse the cmd
             * | ecK(4Byte) | id(4Byte) | coefficient(4Byte)|
             * | pre_cnt(4Byte) = 0| pre 0 IP(4Byte) |...| pre pre_cnt-1 IP (4Byte) |    
             * | next_cnt(4Byte) | next 0 IP(4Byte) |...| next next_cnt-1 IP(4Byte) | 
             * | lostFileNameLen(4Byte) | lostFIleName(l) | 
             * | localFileNameLen(4Btye) | localFileName(l') | 
             */
            int cmdoffset;
            int pre_cnt, next_cnt;
            int id_send;
            unsigned int tip;
            vector<unsigned int> nextSlaves;
            memcpy((char*)&ecK, repStr, 4); 
            memcpy((char*)&id_send, repStr + 4, 4);
            memcpy((char*)&coef, repStr + 8, 4);
            memcpy((char*)&pre_cnt, repStr + 12, 4);
            assert(pre_cnt == 0);
            cmdoffset = 16;
            
            memcpy((char*)&next_cnt, repStr + cmdoffset, 4); 
            cmdoffset += 4;
            for(int i=0; i<next_cnt; ++i) {
                memcpy((char*)&tip, repStr + cmdoffset, 4);
                nextSlaves.push_back(tip);
                cmdoffset += 4;
            }

            int lostBlkNameLen, localBlkNameLen;
            memcpy((char*)&lostBlkNameLen, repStr + cmdoffset, 4);
            lostBlkName = string(repStr + cmdoffset + 4, lostBlkNameLen);
            cmdoffset += 4 +lostBlkNameLen;
            memcpy((char*)&localBlkNameLen, repStr + cmdoffset, 4);
            localBlkName = string(repStr + cmdoffset + 4, localBlkNameLen);
            cmdoffset += 4 + localBlkNameLen;

            if(DR_WORKER_DEBUG) {
                cout << "parse cmd:" << endl;
                cout << "   ecK = " << ecK << endl;
                cout << "   id_in_stripe = " << id_send << endl;
                cout << "   coefficient = " << coef << endl;
                cout << "   no preSlave ";
                cout << endl << "   nextSlaveIPs: ";
                for(auto it : nextSlaves) 
                    cout << ip2Str(it) << "  ";
                cout << endl << "   lostBlk = " << lostBlkName << ", localBlk = " << localBlkName << endl;
            }

            /* readWorker */
            thread readThread([=] {readWorker(_send, localBlkName, pre_cnt, id_send, ecK, coef);} );

            /* sendWorker */
            thread sendThread;
            if(!next_cnt) {
                assert(id_send == ecK);
                sendThread = thread([=] {sendWorker(_send, selfCtx, _localIP, lostBlkName, id_send, ecK);} ); 
            } else {
                sendThread = thread([=] {sendWorker(_send, findCtx(_send._slavesCtx, nextSlaves[0]), _localIP, lostBlkName, id_send, ecK);} );
            }

            readThread.join();
            sendThread.join();

            // send finish time
            gettimeofday(&t2, NULL);
            redisCommand(selfCtx, "RPUSH time-sec %lld", (long long)t2.tv_sec);
            redisCommand(selfCtx, "RPUSH time-usec %lld", (long long)t2.tv_usec);
            cleanup(_send);
        }
        freeReplyObject(rReply);
    }
}

void LinesFullNodeWorker::receiveAndSend(redisContext* selfCtx) {
    redisReply* rReply;
    int repLen;
    char repStr[COMMAND_MAX_LENGTH];
    string lostBlkName, localBlkName;

    int ecK, coef;
    struct timeval t1, t2;
    RSUtil* rsu = new RSUtil();
    while(true) {
        rReply = (redisReply*)redisCommand(selfCtx, "BLPOP cmd_receive 100");
        if(rReply->type == REDIS_REPLY_NIL) {
            if (DR_WORKER_DEBUG)
                cout << typeid(this).name() << "::" << __func__ << "(): empty list" << endl;
        } else if(rReply->type == REDIS_REPLY_ERROR) {
            if (DR_WORKER_DEBUG)
                cout << typeid(this).name() << "::" << __func__ << "(): error happens" << endl;
        } else {
            repLen = rReply->element[1]->len;
            memcpy(repStr, rReply->element[1]->str, repLen);
            /** parse the cmd
             * | ecK(4Byte) | id(4Byte) | coefficient(4Byte)|
             * | pre_cnt(4Byte) | pre 0 IP(4Byte) |...| pre pre_cnt-1 IP (4Byte) |    
             * | next_cnt(4Byte) | next 0 IP(4Byte) |...| next next_cnt-1 IP(4Byte) | 
             * | lostFileNameLen(4Byte) | lostFIleName(l) | 
             * | localFileNameLen(4Btye) | localFileName(l') | 
             */
            int cmdoffset;
            int pre_cnt, next_cnt;
            int id_receive;
            unsigned int tip;
            vector<unsigned int> preSlaves, nextSlaves;
            memcpy((char*)&ecK, repStr, 4); 
            memcpy((char*)&id_receive, repStr + 4, 4);
            memcpy((char*)&coef, repStr + 8, 4);

            memcpy((char*)&pre_cnt, repStr + 12, 4);
            cmdoffset = 16;
            for(int i=0; i<pre_cnt; ++i) {
                memcpy((char*)&tip, repStr + cmdoffset, 4);
                preSlaves.push_back(tip);
                cmdoffset += 4;
            }

            memcpy((char*)&next_cnt, repStr + cmdoffset, 4); 
            cmdoffset += 4;
            for(int i=0; i<next_cnt; ++i) {
                memcpy((char*)&tip, repStr + cmdoffset, 4);
                nextSlaves.push_back(tip);
                cmdoffset += 4;
            }

            int lostBlkNameLen, localBlkNameLen;
            memcpy((char*)&lostBlkNameLen, repStr + cmdoffset, 4);
            lostBlkName = string(repStr + cmdoffset + 4, lostBlkNameLen);
            cmdoffset += 4 +lostBlkNameLen;
            memcpy((char*)&localBlkNameLen, repStr + cmdoffset, 4);
            localBlkName = string(repStr + cmdoffset + 4, localBlkNameLen);
            cmdoffset += 4 + localBlkNameLen;

            if(DR_WORKER_DEBUG) {
                cout << "parse cmd:" << endl;
                cout << "   ecK = " << ecK << endl;
                cout << "   id_in_stripe = " << id_receive << endl;
                cout << "   coefficient = " << coef << endl;
                cout << "   preSlaveIPs: ";
                for(auto it : preSlaves) 
                    cout << ip2Str(it) << "  ";
                cout << endl << "   nextSlaveIPs: ";
                for(auto it : nextSlaves) 
                    cout << ip2Str(it) << "  ";
                cout << endl << "   lostBlk = " << lostBlkName << ", localBlk = " << localBlkName << endl;
            }

            /* readWorker */
            thread readThread([=] {readWorker(_receive, localBlkName, pre_cnt, id_receive, ecK, coef);} );

            /* pullWorker */

            _receive._pktsWaiting = (int*)calloc(sizeof(int), pre_cnt);
            thread pullThread[pre_cnt];
            vector<redisContext*> selfCtxs;
            if(_conf->_ecType == "BUTTERFLY") {
                int ids = coef;
                int lostBlkIdx = ids & 7;
                for(int i=0, off=15; i<pre_cnt; ++i, off-=3) {
                    selfCtxs.push_back(initCtx(0));
                    pullThread[i] = thread([=] {
                        pullWorker(_receive, selfCtxs[i], preSlaves[i], i, pre_cnt, lostBlkName, (ids & (7<<off)) >> off, lostBlkIdx);
                    } );
                }
            } else {
                for(int i=0; i<pre_cnt; ++i) {
                    selfCtxs.push_back(initCtx(0));
                    pullThread[i] = thread([=] {
                        pullWorker(_receive, selfCtxs[i], preSlaves[i], i, pre_cnt, lostBlkName);
                    } );
                }
            }
            
            /* sendWorker */
            thread sendThread;
            if(!next_cnt) {
                assert(id_receive == ecK);
                if(DR_WORKER_DEBUG) cout << "_id = " << id_receive << endl;
                sendThread = thread([=] {sendWorker(_receive, selfCtx, _localIP, lostBlkName, id_receive, ecK);} );
            } else {
                sendThread = thread([=] {sendWorker(_receive, findCtx(_receive._slavesCtx, nextSlaves[0]), _localIP, lostBlkName, id_receive, ecK);} );
            }

            readThread.join();
            for(int i=0; i<pre_cnt; ++i) pullThread[i].join();
            sendThread.join();

            for(int i=0; i<pre_cnt; ++i) redisFree(selfCtxs[i]);

            // send finish time
            gettimeofday(&t2, NULL);
            redisCommand(selfCtx, "RPUSH time-sec %lld", (long long)t2.tv_sec);
            redisCommand(selfCtx, "RPUSH time-usec %lld", (long long)t2.tv_usec);

            cleanup(_receive);
        }
        freeReplyObject(rReply);
    }
}
void LinesFullNodeWorker::doProcess() {
    cout << __func__ << " starts" << endl;

    _multiplyFree = true;
    _sendWorkerFree = true;
    _sendOnlyThreadNum = 1;
    _receiveAndSendThreadNum = 1;
    redisContext* _selfCtxForSend = initCtx(0);
    redisContext* _selfCtxForReceive = initCtx(0);

    _sendOnlyThrd = thread([=] { this->sendOnly(_selfCtxForSend); });
    _receiveAndSendThrd = thread([=] { this->receiveAndSend(_selfCtxForReceive); });

    redisReply* rReply;
    int repLen;
    char repStr[COMMAND_MAX_LENGTH];
    string lostBlkName, localBlkName;
    int linesBeginFlag = 0;
    while(true) {
        rReply = (redisReply*)redisCommand(_selfCtx, "BLPOP dr_cmds 100");
        if(rReply->type == REDIS_REPLY_NIL) {
            if (DR_WORKER_DEBUG)
                cout << typeid(this).name() << "::" << __func__ << "(): empty list" << endl;
        } else if(rReply->type == REDIS_REPLY_ERROR) {
            if (DR_WORKER_DEBUG)
                cout << typeid(this).name() << "::" << __func__ << "(): error happens" << endl;
        } else {
            repLen = rReply->element[1]->len;
            memcpy(repStr, rReply->element[1]->str, repLen);
            // determine cmd belong to (1)send-only or (2)receive-and-send
            int pre_cnt;
            memcpy((char*)&pre_cnt, rReply->element[1]->str + 12, 4);
            if(pre_cnt) { // 2
                redisCommand(_selfCtx, "RPUSH cmd_receive %b", repStr, repLen);
            } else { // 1
                redisCommand(_selfCtx, "RPUSH cmd_send_only %b", repStr, repLen);
            }
        }
        freeReplyObject(rReply);
    }

    // should not reach here
    _sendOnlyThrd.join();
    _receiveAndSendThrd.join();
}

void LinesFullNodeWorker::pullWorker(variablesForTran& var, redisContext* rc, unsigned int senderIp, int tid, int thread_num, string loskBLkName, int bfid, int bflid) {
    if(DR_WORKER_DEBUG) cout << __func__ << " starts " << endl;

    int finishedPkt;
    redisReply* rReply;
    if(_conf->_ecType == "BUTTERFLY") {
        int subBlockIndex, subPktCnt = _packetCnt / 8;
        vector<vector<int>> contributions(4, vector<int>());
        int* xorNeededCnt = (int*)calloc(8*subPktCnt, sizeof(int));
        int idBegin = (bflid < bfid) ? ((bfid-1)*4) : (bfid*4); 
        for(int i=idBegin; i<idBegin+4; ++i) {
            for(int j=0; j<8; ++j) {
                if(_decMatButterFly[bflid][j*20+i]) {
                    contributions[i-idBegin].push_back(j);
                    for(int z=j*subPktCnt; z<(j+1)*subPktCnt; ++z) ++xorNeededCnt[z];
                }
            }
        }
        
        for(int i=0; i<_packetCnt/2; ++i) {
            subBlockIndex = i / subPktCnt;
            while(!var._diskFlag[i]) {
                unique_lock<mutex> lck(var._diskMtx[i]);
                var._diskCv.wait(lck);
            }
            int ipd = (int)((senderIp >> 24) & 0xff);
            rReply = (redisReply*)redisCommand(rc, "BLPOP tmp:%s:%d 0", loskBLkName.c_str(), ipd);
            
            int sz = contributions[subBlockIndex].size(), xorOff;
            for(int j=0; j<sz; ++j) {
                xorOff = contributions[subBlockIndex][j]*subPktCnt + i%subPktCnt;
                var._diskCalMtx[xorOff].lock();
                Computation::XORBuffers(var._diskPkts[xorOff], rReply->element[1]->str, _packetSize);
                var._diskCalMtx[xorOff].unlock();
                --xorNeededCnt[xorOff];
            }
            freeReplyObject(rReply);

            finishedPkt = var._pktsWaiting[tid];
            while(finishedPkt<_packetCnt && (!xorNeededCnt[finishedPkt])) ++finishedPkt;
            var._pktsWaiting[tid] = finishedPkt;

            var._pullWorkerMtx.lock();
            for(int j=0; j<thread_num; ++j) finishedPkt = min(finishedPkt, var._pktsWaiting[j]);
            if(finishedPkt > var._waitingToSend) {
                var._waitingToSend = finishedPkt;
                unique_lock<mutex> lck(var._mainSenderMtx);
                var._mainSenderCondVar.notify_all();
            }
            var._pullWorkerMtx.unlock();
        }
    } else {
        for(int i=0; i<_packetCnt; ++i) {
            while(!var._diskFlag[i]) {
                unique_lock<mutex> lck(var._diskMtx[i]);
                var._diskCv.wait(lck);
            }
            
            int ipd = (int)((senderIp >> 24) & 0xff);
            rReply = (redisReply*)redisCommand(rc, "BLPOP tmp:%s:%d 0", loskBLkName.c_str(), ipd);
            var._diskCalMtx[i].lock();
            Computation::XORBuffers(var._diskPkts[i], rReply->element[1]->str, _packetSize);
            var._diskCalMtx[i].unlock();

            freeReplyObject(rReply);
            ++var._pktsWaiting[tid];
            finishedPkt = var._pktsWaiting[tid];

            var._pullWorkerMtx.lock();
            for(int j=0; j<thread_num; ++j) finishedPkt = min(finishedPkt, var._pktsWaiting[j]);
            if(finishedPkt > var._waitingToSend) {
                var._waitingToSend = finishedPkt;
                unique_lock<mutex> lck(var._mainSenderMtx);
                var._mainSenderCondVar.notify_all();
            }
            var._pullWorkerMtx.unlock();

        }
    }
}


void LinesFullNodeWorker::readWorker(variablesForTran& var, string blkName, int preSlaveCnt, int _id, int _ecK, int _coefficient) { // read from local storage

    string blkPath = _conf->_blkDir + '/' + blkName;
    int fd = open(blkPath.c_str(), O_RDONLY);
    if(fd == -1) {
        blkPath = "";
        bool isFind = false;
        readFile(_conf->_blkDir, blkName, blkPath, isFind);
        fd = open(blkPath.c_str(), O_RDONLY);
        if(fd == -1) {
            cout << "Test by LinesYao: fd: " << fd << endl;
            exit(1);
        }
    }
    
    if(DR_WORKER_DEBUG) cout << typeid(this).name() << "::" << __func__ << "() openLocal fd = " << fd << endl;

    int base = _conf->_packetSkipSize;

    if(_conf->_ecType == "BUTTERFLY") {
        int alpha = 1<<(_conf->_ecK -1); // alpha = 2^(k-1)
        int sub_pkt_cnt = _packetCnt / alpha;
        char* buffer = (char*)malloc(sizeof(char) * _packetSize);
        vector<int> subBlockIndex;
        for(int subId=0, off=(alpha/2-1)*alpha; subId<alpha/2; ++subId, off-=alpha) {
            subBlockIndex.clear();
            for(int j=0; j<alpha; ++j) {
                if(_coefficient & (1<<(off+alpha-1-j))) subBlockIndex.push_back(j);
            }

            int cnt = subBlockIndex.size(), pktid;
            for(int pkt=0; pkt<sub_pkt_cnt; ++pkt) { 
                pktid = subId*sub_pkt_cnt+pkt;
                if(_id != _ecK) {
                    for(int i=0; i<cnt; ++i) {
                        base = _conf->_packetSkipSize + subBlockIndex[i] * sub_pkt_cnt * _packetSize;
                        if(!i) {
                            readPacketFromDisk(fd, var._diskPkts[pktid], _packetSize, base + pkt * _packetSize);
                        } else {
                            readPacketFromDisk(fd, buffer, _packetSize, base + pkt * _packetSize);
                            Computation::XORBuffers(var._diskPkts[pktid], buffer, _packetSize);
                        }
                    }
                }
                
                {
                    unique_lock<mutex> lck(var._diskMtx[pktid]);
                    var._diskFlag[pktid] = true;
                    var._diskCv.notify_all(); // linesyao one()->all()
                    if(!preSlaveCnt) {
                        ++var._waitingToSend;
                        var._mainSenderCondVar.notify_one();
                    }
                }
            }
        }
        
        free(buffer);
    } else {
        for(int i=0; i<_packetCnt; ++i) {
            if(_id != _ecK) {
                readPacketFromDisk(fd, var._diskPkts[i], _packetSize, base);
                while(_multiplyFree == false) {
                    unique_lock<mutex> lck(_multiplyMtx);
                    _multiplyCv.wait(lck);
                }
                {
                    unique_lock<mutex> lck(_multiplyMtx);
                    _multiplyFree = false;
                }
                RSUtil::multiply(var._diskPkts[i], _coefficient, _packetSize);
                {
                    unique_lock<mutex> lck(_multiplyMtx);
                    _multiplyFree = true;
                    _multiplyCv.notify_all();

                }
            }
            base += _packetSize;
            {
                unique_lock<mutex> lck(var._diskMtx[i]);
                var._diskFlag[i] = true;
                var._diskCv.notify_all(); 
                if(!preSlaveCnt) {
                    ++var._waitingToSend;
                    var._mainSenderCondVar.notify_all();
                }
            }
 
            if(DR_WORKER_DEBUG) cout << typeid(this).name() << "::" << __func__ << "() processing pkt " << i << endl; 
        }
    }

    close(fd);
}

void LinesFullNodeWorker::sendWorker(variablesForTran& var, redisContext* rc, unsigned int ip, string lostBlkName, int _id, int _ecK) {
    if(DR_WORKER_DEBUG) cout << __func__ << " starts " << endl;
    redisReply* rReply;

    if(_id == _ecK) {
        ofstream ofs(_conf->_stripeStore+"/"+lostBlkName);
        for(int i=0; i<_packetCnt; ++i) {
            while(i >= var._waitingToSend) {
                unique_lock<mutex> lck(var._mainSenderMtx);
                var._mainSenderCondVar.wait(lck);
            }
            if(_conf->_fileSysType == "HDFS3") {
                rReply = (redisReply*)redisCommand(rc, "RPUSH %s %b", lostBlkName.c_str(), var._diskPkts[i], _packetSize);
                freeReplyObject(rReply);
            } else {
                ofs.write(var._diskPkts[i], _packetSize);
            }
            if(DR_WORKER_DEBUG) 
                cout << typeid(this).name() << "::" << __func__ << "() processing pkt " << i << endl;
        }
        ofs.close();
        if(_conf->_fileSysType == "HDFS3") { 
            redisCommand(rc, "RPUSH ACK %s", lostBlkName.c_str());
        } else {
            redisCommand(rc, "RPUSH %s ack", lostBlkName.c_str()); // [linesyao] finished ack!
        }
    } else {
        
        for(int i=0; i<_packetCnt; ++i) {
            if(_conf->_ecType == "BUTTERFLY" && i==_packetCnt/2) break;
            while(i >= var._waitingToSend) {
                unique_lock<mutex> lck(var._mainSenderMtx);
                var._mainSenderCondVar.wait(lck);
            }
            int ipd = (int)((ip >> 24) & 0xff);
            rReply = (redisReply*)redisCommand(rc, "RPUSH tmp:%s:%d %b", lostBlkName.c_str(), ipd, var._diskPkts[i], _packetSize);
            freeReplyObject(rReply);

            if(DR_WORKER_DEBUG) 
                cout << typeid(this).name() << "::" << __func__ << "() processing pkt " << i << endl;
        }
    }
}


