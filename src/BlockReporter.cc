#include "BlockReporter.hh"

void readFileList(vector<string>& blks, string blkDir) {
    DIR* dir;
    struct dirent* ent;
    if ((dir = opendir(blkDir.c_str())) != NULL) {
        while((ent = readdir(dir)) != NULL) {
            string strName(ent->d_name);
            if (strcmp(ent->d_name, ".") == 0 ||
                strcmp(ent->d_name, "..") == 0 || strName.find("meta") == 30) {
                continue;
            } 

            if(ent->d_type == 8) { // file
                blks.push_back(ent->d_name);
            } else if(ent->d_type == 4) { // dir
                readFileList(blks, blkDir + '/' + string(ent->d_name));
            }
        }
        closedir(dir);
    } else {
        // TODO: error handling
        cerr << "BlockReporter::report() opening directory error" << endl;
    }
    
}

void BlockReporter::report(unsigned int coIP, const char* blkDir,
                           unsigned int localIP) {
    // add by Zuoru
    cerr << blkDir << endl;
    DIR* dir;
    FILE* file;
    struct dirent* ent;

    string rServer;
    rServer += to_string(coIP & 0xff);
    rServer += ".";
    rServer += to_string((coIP >> 8) & 0xff);
    rServer += ".";
    rServer += to_string((coIP >> 16) & 0xff);
    rServer += ".";
    rServer += to_string((coIP >> 24) & 0xff);

    cout << "coordinator server: " << rServer << endl;
    struct timeval timeout = {1, 500000};  // 1.5 seconds
    redisContext* rContext =
        redisConnectWithTimeout(rServer.c_str(), 6379, timeout);
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
    char info[256];

    vector<string> blks;
    readFileList(blks, string(blkDir));
    cout << "blks.size = " << blks.size() << endl;
    for(int i=0; i<blks.size(); ++i) {
        cout << blks[i] << endl;
        memcpy(info, (char*)&localIP, 4);
        memcpy(info + 4, blks[i].c_str(), strlen(blks[i].c_str()));
        rReply = (redisReply*)redisCommand(rContext, "RPUSH blk_init %b", info, 4 + strlen(blks[i].c_str()));
        freeReplyObject(rReply);
    }

    redisFree(rContext);
}
