#include <iostream>

#include "MetadataBase.hh"
#include "LinesCoordinator.hh"

using namespace std;

/**
 * The main function of DRcoordinator
 */

int main(int argc, char** argv) {
    setbuf(stdout, NULL);  // for debugging
    setbuf(stderr, NULL);
    Config* conf = new Config("conf/config.xml");
    Coordinator* coord = new LinesCoordinator(conf);
    coord->doProcess();
    return 0;
}
