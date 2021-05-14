#include <iostream>

#include "BlockReporter.hh"
#include "Config.hh"
#include "DRWorker.hh"
#include "LinesFullNodeWorker.hh"

using namespace std;

int main(int argc, char** argv) {
    setbuf(stdout, NULL);  // for debugging
    setbuf(stderr, NULL);
    Config* conf = new Config("conf/config.xml");
    BlockReporter::report(conf->_coordinatorIP, conf->_blkDir.c_str(),
                          conf->_localIP);

    if (conf->_coordinatorIP == conf->_localIP) {
        printf("%s: ERROR: local IP is wrong\n", __func__);
        return 1;
    }
    DRWorker* worker = new LinesFullNodeWorker(conf);
    worker->doProcess();
    return 0;
}

