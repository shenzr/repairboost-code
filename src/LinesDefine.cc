#include "LinesDefine.hh"

string ip2Str(unsigned int ip) {
    string retVal;
    retVal += to_string(ip & 0xff);
    retVal += '.';
    retVal += to_string((ip >> 8) & 0xff);
    retVal += '.';
    retVal += to_string((ip >> 16) & 0xff);
    retVal += '.';
    retVal += to_string((ip >> 24) & 0xff);
    return retVal;
}

