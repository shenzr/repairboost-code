#include <string>
#include <iostream>

#include "Config.hh"
#include "LinesDefine.hh"
#include "LinesFullNodeClientStream.hh"
using namespace std;

int main(int argc, char** argv) {

    Config* conf = new Config("conf/config.xml");
    LinesFullNodeClientStream lfs(conf, conf->_packetCnt, conf->_packetSize, conf->_coordinatorIP, conf->_localIP);

    return 0;
}
