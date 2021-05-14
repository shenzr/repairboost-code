#include "Config.hh"

Config::Config(std::string confFile) {
    XMLDocument doc;
    doc.LoadFile(confFile.c_str());
    XMLElement* element;
    for (element =
             doc.FirstChildElement("setting")->FirstChildElement("attribute");
         element != NULL; element = element->NextSiblingElement("attribute")) {
        XMLElement* ele = element->FirstChildElement("name");
        std::string attName = ele->GetText();
        if (attName == "erasure.code.type")
            _ecType = ele->NextSiblingElement("value")->GetText();
        else if (attName == "erasure.code.k")
            _ecK = std::stoi(ele->NextSiblingElement("value")->GetText());
        else if (attName == "erasure.code.n")
            _ecN = std::stoi(ele->NextSiblingElement("value")->GetText());
        else if (attName == "lrc.code.l")
            _lrcL = std::stoi(ele->NextSiblingElement("value")->GetText());
        else if (attName == "encode.matrix.file")
            _ecConfigFile = ele->NextSiblingElement("value")->GetText();
        else if (attName == "link.weight.matrix")
            _linkWeightFile = ele->NextSiblingElement("value")->GetText();
        else if (attName == "packet.count")
            _packetCnt = std::stoi(ele->NextSiblingElement("value")->GetText());
        else if (attName == "packet.size")
            _packetSize =
                std::stoi(ele->NextSiblingElement("value")->GetText());
        else if (attName == "repair.method") {
            std::string s = ele->NextSiblingElement("value")->GetText();
            if(s == "cr") _chunkRepairMethod = SINGLE_STRIPE_CR;
            else if(s == "ppr") _chunkRepairMethod = SINGLE_STRIPE_PPR;
            else if(s == "path") _chunkRepairMethod = SINGLE_STRIPE_PATH;
        }    
        else if (attName == "packet.skipsize")
            _packetSkipSize =
                std::stoi(ele->NextSiblingElement("value")->GetText());
        else if (attName == "file.system.type") {
            _fileSysType = ele->NextSiblingElement("value")->GetText();
            if (_fileSysType == "standalone") {
                std::cout << "standalone mode share the same block format and "
                             "stripe store format with HDFS mode"
                          << std::endl;
                _fileSysType = "HDFS";
            } else if(_fileSysType == "HDFS3") {
                std::cout << "FSTYPE: HDFS3" << std::endl;
            }
        } else if (attName == "meta.stripe.dir")
            _stripeStore = ele->NextSiblingElement("value")->GetText();  
        else if (attName == "block.directory")
            _blkDir =
                ele->NextSiblingElement("value")->GetText();  
        else if (attName == "coordinator.address")
            _coordinatorIP =
                inet_addr(ele->NextSiblingElement("value")->GetText());
        else if (attName == "local.ip.address")
            _localIP = inet_addr(ele->NextSiblingElement("value")->GetText());
        else if (attName == "helpers.address") {
            for (ele = ele->NextSiblingElement("value"); ele != NULL;
                 ele = ele->NextSiblingElement("value")) {
                std::string tempstr = ele->GetText();
                int pos = tempstr.find("/");
                int len = tempstr.length();
                std::string rack = tempstr.substr(0, pos);
                std::string ip = tempstr.substr(pos + 1, len - pos - 1);
                _helpersIPs.push_back(
                    inet_addr(ip.c_str()));  //  ip from str to unsigned int
            }
            sort(_helpersIPs.begin(), _helpersIPs.end());
        } 
    }

    if (_fileSysType == "HDFS3") {
        std::cout << "FSTYPE: HDFS3" << std::endl;
    }

    _group_id = (int*)calloc(_ecN, sizeof(int));
    std::cout << "ecType = " << _ecType  << std::endl;
    if(_ecType == "LRC") {
        for(int i=0; i<_ecK; ++i) _group_id[i] = i / (_ecK/_lrcL); // data chunks
        for(int i=_ecK; i<_ecK+_lrcL; ++i) _group_id[i] = i - _ecK; // local parity
        for(int i=_ecK+_lrcL; i<_ecN; ++i) _group_id[i] = -1; // global parity
    }
}

void Config::display() {
    std::cout << "Global info: " << std::endl;
    std::cout << "_ecK = " << _ecK << std::endl;
    std::cout << "_packetSize = " << _packetSize << std::endl;
    std::cout << "_packetSkipSize = " << _packetSkipSize << std::endl;
    std::cout << "_packetCnt = " << _packetCnt << std::endl << std::endl;
    std::cout << "_fileSysType = " << _fileSysType << std::endl << std::endl;

    std::cout << "Coordinator info: " << std::endl;
    std::cout << "\t _coordinatorIP = " << _coordinatorIP << std::endl;
    std::cout << "\t _coCmdReqHandlerThreadNum = " << _coCmdReqHandlerThreadNum
              << std::endl;
    std::cout << "\t _coCmdDistThreadNum = " << _coCmdDistThreadNum << std::endl
              << std::endl;

    std::cout << "helper info: " << std::endl;
    std::cout << "\t _helpersIPs: " << std::endl;
    for (auto it : _helpersIPs) {
        std::cout << "\t\t" << it << std::endl;
    }
}

