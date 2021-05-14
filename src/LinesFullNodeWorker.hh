#ifndef _LINES_FNWORKER_HH_
#define _LINES_FNWORKER_HH_

#include <queue>
#include <cassert>
#include "DRWorker.hh"
#include "BUTTERFLY64Util.hh"

class LinesFullNodeWorker : public DRWorker {
    size_t _sendOnlyThreadNum;
    size_t _receiveAndSendThreadNum;
    thread _sendOnlyThrd;
    thread _receiveAndSendThrd;

    bool _sendWorkerFree;
    mutex _sendWorkerMtx;
    condition_variable _sendWorkerCv;

    bool _multiplyFree;
    mutex _multiplyMtx;
    condition_variable _multiplyCv;

    BUTTERFLYUtil* _butterflyUtil;
    int _decMatButterFly[6][20 * 8];
    int _lostBlkIdxBF;
 
    void readWorker(variablesForTran& var, string, int, int, int, int);
    void pullWorker(variablesForTran& var, redisContext*, unsigned int, int, int, string, int butterfly_id_in_stripe = 0, int butterfly_lost_blk_id_in_stripe = 0);
    void sendWorker(variablesForTran& var, redisContext* rc, unsigned int senderIp, string lostBlkName, int, int);
    public: 
        LinesFullNodeWorker(Config* conf);
        void doProcess();
        void sendOnly(redisContext* selfCtx);
        void receiveAndSend(redisContext* selfCtx);
};

#endif  //_LINES_FNWORKER_HH_

