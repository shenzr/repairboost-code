#include "DRWorker.hh"

using namespace std;

DRWorker::DRWorker(Config* conf) : _conf(conf) {
    _packetCnt = _conf->_packetCnt;
    _packetSize = _conf->_packetSize;
    init(_conf->_coordinatorIP, _conf->_localIP, _conf->_helpersIPs);
}

void DRWorker::cleanup(variablesForTran& var) {

    fill_n(var._diskFlag, _packetCnt, false);
    var._waitingToSend = 0;
    for (int i = 0; i < _packetCnt; i++) memset(var._diskPkts[i], 0, _packetSize);
    cout << "DRWorker() cleanup finished" << endl;
}

/**
 * Init the data structures
 */
void DRWorker::init(unsigned int cIP, unsigned int sIP,
                    vector<unsigned int>& slaveIP) {
    _localIP = sIP;

    // -- init variables for send
    _send._diskPkts = (char**)malloc(_packetCnt * sizeof(char*));
    _send._diskFlag = (bool*)calloc(_packetCnt, sizeof(bool));
    _send._diskMtx = vector<mutex>(_packetCnt);
    _send._diskCalMtx = vector<mutex>(_packetCnt);
    _send._waitingToSend = 0;
    _send.isNeedPull = false;
    for (int i = 0; i < _packetCnt; i++)
        _send._diskPkts[i] = (char*)calloc(sizeof(char), _packetSize);
    for (auto& it : slaveIP) {
        _send._slavesCtx.push_back({it, initCtx(it)});
    }
    sort(_send._slavesCtx.begin(), _send._slavesCtx.end());
    
    // -- init variables for receive
    _receive._diskPkts = (char**)malloc(_packetCnt * sizeof(char*));
    _receive._diskFlag = (bool*)calloc(_packetCnt, sizeof(bool));
    _receive._diskMtx = vector<mutex>(_packetCnt);
    _receive._diskCalMtx = vector<mutex>(_packetCnt);
    _receive._waitingToSend = 0;
    _receive.isNeedPull = true;
    for (int i = 0; i < _packetCnt; i++)
        _receive._diskPkts[i] = (char*)calloc(sizeof(char), _packetSize);
    for (auto& it : slaveIP) {
        _receive._slavesCtx.push_back({it, initCtx(it)});
    }
    sort(_receive._slavesCtx.begin(), _receive._slavesCtx.end());

    _selfCtx = initCtx(0);

}


redisContext* DRWorker::initCtx(unsigned int redisIP) {
    cout << "initing cotex to " << ip2Str(redisIP) << endl;
    struct timeval timeout = {1, 500000};  // 1.5 seconds
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

redisContext* DRWorker::findCtx(vector<pair<unsigned int, redisContext*>>& slavesCtx, unsigned int ip) {
    int sid = 0, eid = slavesCtx.size() - 1, mid;
    while (sid < eid) {
        mid = (sid + eid) / 2;
        if (slavesCtx[mid].first == ip) {
            return slavesCtx[mid].second;
        } else if (slavesCtx[mid].first < ip) {
            sid = mid + 1;
        } else {
            eid = mid - 1;
        }
    }
    return slavesCtx[sid].first == ip ? slavesCtx[sid].second : NULL;
}

void DRWorker::readFile(string blkDir, string fileName, string& blkPath, bool& isFind) {
    if(isFind) return ;
    DIR* dir;
    struct dirent* ent;
    if ((dir = opendir(blkDir.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            string strName(ent->d_name);
            if (strcmp(ent->d_name, ".") == 0 ||
                strcmp(ent->d_name, "..") == 0 || strName.find("meta") == 30) {
                continue;
            }

            if (ent->d_type == 8) {  // file
                if (strcmp(ent->d_name, fileName.c_str()) == 0) {
                    blkPath = string(blkDir + '/' + string(ent->d_name));
                    isFind = true;
                    return;
                }
            } else if (ent->d_type == 4) {  // dir
                readFile(blkDir + '/' + string(ent->d_name), fileName, blkPath,
                         isFind);
            }
        }
        closedir(dir);
    } else {
        // TODO: error handling
        cerr << __func__ <<" opening directory error" << endl;
    }
}

void DRWorker::readPacketFromDisk(int fd, char* content, int packetSize,
                                  int base) {
    int readLen = 0, readl;
    while (readLen < packetSize) {
        if ((readl = pread(fd, content + readLen, packetSize - readLen,
                           base + readLen)) < 0) {
            cerr << "ERROR During disk read" << endl;
            exit(1);
        } else {
            readLen += readl;
        }
    }
}


