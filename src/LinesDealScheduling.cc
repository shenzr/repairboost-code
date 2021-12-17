#include "LinesDealScheduling.hh"

void LinesDealScheduling::initConf(Config* conf) {
    _conf = conf;
    _ecK = _conf->_ecK;
    _ecN = _conf->_ecN;
    _ecM = _ecN - _ecK;
    _ec_LRC_L = _conf->_lrcL;
    _ec_LRC_G = _ecM - _ec_LRC_L;
    _slaveCnt = _conf->_helpersIPs.size();
}

void LinesDealScheduling::buildGraph(int repair_method, repairGraph& G, int ecK) {
    if(repair_method == SINGLE_STRIPE_PPR) { // -- ppr
        ppr_graph(ecK, G);
    } else if(repair_method == SINGLE_STRIPE_CR) { // -- conventional repair graph
        for(int i=0; i<ecK; ++i) {
            G.out[i][0] = 1;
            G.out[i][1] = ecK;
        }
        G.in[ecK][0] = ecK;
        for(int i=0; i<ecK; ++i) {
            G.in[ecK][i+1] = i;
        }
    } else if(repair_method == SINGLE_STRIPE_PATH) { // -- linear path (ecpipe/PUSH)
        for(int i=0; i<=ecK; ++i) {
            G.in[i][0] = 1, G.out[i][0] = 1;
            if(i == ecK) G.out[i][0] = 0;
            if(!i) G.in[i][0] = 0;
        }
        for(int i=0; i<ecK; ++i) {
            G.in[i+1][1] = i;
            G.out[i][1] = i+1;
        }
    }
}

void LinesDealScheduling::buildStripeGraph(repairGraph& G, repairGraph& localG, int repair_method) {
    int K = _ecK;
    if(_conf->_ecType == "BUTTERFLY") K = _ecN - 1; // for BUTTERFLY code, G, K = _ecN-1
    repairGraph g(K+1);
    G = g;
    buildGraph(repair_method, G, K);

    int nr = _ec_LRC_L ? _ecK/_ec_LRC_L : 0 ;
    repairGraph lg(nr+1);
    localG = lg;
    buildGraph(repair_method, localG, nr);
}


void LinesDealScheduling::ppr_graph(int ecK, repairGraph& G) {
    int depth = ceil(1.0*log(ecK+1)/log(2));
#if DEBUG_COORD
    cout << "structure of ppr: " << endl;
    cout << "depth = " << depth << endl;
    for(int j=1; j<=depth; ++j) {
        int st = 1<<(j-1);
        int stride = st;
        cout << "start node = " << st << ", stride = " << stride << endl;
        for(int k=st; k<=ecK; k+=2*stride) {
            cout << "from " << k << " to " << min(k+stride, ecK+1) << endl;  
        }cout << endl << endl;
    }
#endif

    for(int j=1; j<=depth; ++j) {
        int st = 1<<(j-1);
        int stride = st;
        for(int k=st; k<=ecK; k+=2*stride) {
            int to = min(k+stride, ecK+1);
            G.out[k-1][++G.out[k-1][0]] = to-1;
            G.in[to-1][++G.in[to-1][0]] = k-1;
        }
    }

#if DEBUG_COORD
    G.display();
#endif

}

bool pii_desc_cmp(pii x, pii y) {
    return x.first > y.first;
}
bool piii_cmp(piii x, piii y) {
    if(x.first.first == y.first.first)
        return x.first.second < y.first.second;
    return x.first.first < y.first.first;
}
bool piii_desc_cmp(piii x, piii y) {
    return x.first.first > y.first.first;
}

vector<Stripe> LinesDealScheduling::repairboost(int failNodeId, int repair_method, int lostBlkCnt, int* idxs, int* placement, int* isSoureCandidate) {

    repairGraph G, localG;
    buildStripeGraph(G, localG, repair_method);

    vector<Stripe> stripes;
    for(int i=0; i<lostBlkCnt; ++i) {
        if(_conf->_ecType == "LRC" && idxs[i] < _ecK+_ec_LRC_L) {
            stripes.push_back(Stripe(localG));
        } else {
            stripes.push_back(Stripe(G));
        }
    }

    vector<piii> peers;
    int sid, pid, vid;
    int* load_upload = (int*)calloc(_slaveCnt, sizeof(int));
    int* load_dwload = (int*)calloc(_slaveCnt, sizeof(int));
    int* load_storage = (int*)calloc(_slaveCnt, sizeof(int));

    // -- select source nodes, except for special nodes
    map<pii, bool> isSelected; // (stripeId, peerNodeId) -> bool
    vector<piii> sourceVertexs; // ((load_dwload, stripeId), vertexId)
    vector<pii> specialSource; // (stripeId, vid) download = 0
    for(int i=0; i<lostBlkCnt; ++i) {
        for(int j=0; j<stripes[i].rG._node_cnt-1; ++j) {
            if(!stripes[i].rG.in[j][0]) {
                specialSource.push_back(make_pair(i, j));
                continue;
            }
            sourceVertexs.push_back({{stripes[i].rG.in[j][0], i}, j});
        }
    }
    sort(sourceVertexs.begin(), sourceVertexs.end(), piii_desc_cmp);

    for(int i=0; i<sourceVertexs.size(); ++i) {
        sid = sourceVertexs[i].first.second;
        vid = sourceVertexs[i].second;
        peers.clear();
        for(int j=0; j<_ecN; ++j) {
            if(!isSoureCandidate[sid*_ecN+j]) continue;
            pid = placement[sid*_ecN+j];
            if(pid == failNodeId || isSelected.count({sid, pid})) continue;
            peers.push_back({{load_dwload[pid], load_upload[pid]}, pid});
        }
        sort(peers.begin(),peers.end(), piii_cmp);
        pid = peers[0].second;
        stripes[sid].vertex_to_peerNode[vid] = pid;
        load_upload[pid] += stripes[sid].rG.out[vid][0];
        load_dwload[pid] += stripes[sid].rG.in[vid][0];
        isSelected[{sid, pid}] = 1; // for unique!
    }
    
    // -- select replacement nodes 
    map<int, bool> isStore;
    vector<pii> destVertexs; // (load_download, idx) for destNode in repairGraph
    for(int i=0; i<lostBlkCnt; ++i) {
        destVertexs.push_back({stripes[i].rG.in[stripes[i].rG._node_cnt-1][0], i});
    }
    sort(destVertexs.begin(), destVertexs.end(), pii_desc_cmp);
    for(int i=0; i<lostBlkCnt; ++i) {
        sid = destVertexs[i].second;
        peers.clear();
        isStore.clear();
        for(int j=0; j<_ecN; ++j) isStore[placement[sid*_ecN+j]] = 1;
        for(int j=0; j<_slaveCnt; ++j) {
            if(isStore.count(j)) continue;
            peers.push_back({{load_storage[j], load_dwload[j]}, j});
        }
        sort(peers.begin(), peers.end(), piii_cmp);
        pid = peers[0].second;
        stripes[sid].vertex_to_peerNode[stripes[sid].rG._node_cnt-1] = pid;
        ++load_storage[pid];
        load_dwload[pid] += stripes[sid].rG.in[stripes[sid].rG._node_cnt-1][0];
    }

    // -- select special nodes
    cout << specialSource.size() << endl;
    for(int i=0; i<specialSource.size(); ++i) {
        sid = specialSource[i].first;
        vid = specialSource[i].second;
        peers.clear();
        for(int j=0; j<_ecN; ++j) {
            if(!isSoureCandidate[sid*_ecN+j]) continue;
            pid = placement[sid * _ecN + j];
            if(pid == failNodeId || isSelected.count({sid, pid})) continue;
            peers.push_back({{load_upload[pid], load_upload[pid]}, pid});
        }
        sort(peers.begin(), peers.end(), piii_cmp);
        pid = peers[0].second;
        stripes[sid].vertex_to_peerNode[vid] = pid;
        load_upload[pid] += stripes[sid].rG.out[vid][0];
        load_dwload[pid] += stripes[sid].rG.in[vid][0];
        isSelected[{sid, pid}] = 1; // !unique
    }

    unitTestForMapping(stripes, placement, isSoureCandidate, failNodeId, lostBlkCnt);
    
    int flag, max_bd, round = 0;
    map<pair<int, int>, int> edgeMap;
    vector<vector<pii> > upload_to, download_from; // (peer_node, stripe_id)
    upload_to.clear();
    download_from.clear();
    while(true) {
        flag = 0;
        edgeMap.clear();
        initGraph(stripes, flag, edgeMap, lostBlkCnt);
        if(!flag) break;
        ++round;
        max_bd = runISAP();
        vector<pii> up(_slaveCnt), dw(_slaveCnt);
        for(int i=0; i<_slaveCnt; ++i) up[i] = {-1,-1}, dw[i] = {-1, -1};
        alterRepairGraph(round, stripes, edgeMap, up, dw);
        upload_to.push_back(up);
        download_from.push_back(dw);
    }

    unitTestForSchedulingScheme(stripes, SCHEDUL_METHOD_BOOST, failNodeId, lostBlkCnt, upload_to, download_from);

    return stripes;
}

void LinesDealScheduling::initGraph(vector<Stripe>& stripes, int& flag, map<pair<int, int>, int>& edgeMap, int lostBlkCnt) {
    int _p = _slaveCnt, _r = lostBlkCnt;
    int S = 0, T = 2*_p+1, n = 2*_p+2;
    isap.init(n, S, T);
    isap.edge_level = 3;

    /*
        id range for vertex in each level
        S 0
        1 -> _p
        _p+1 -> 2*_p
        T 2*_p+1
    */
    for(int i=0; i<_p; ++i) {
        // --add edge  S -> peer_node_from
        isap.addEdge(S, i+1, 1);
    }
    isap.edgeNum[0] = _p;
    
    int to, from;
    for(int i=0; i<lostBlkCnt; ++i) {
        for(int j=0; j<stripes[i].rG._node_cnt; ++j) {
            if(!stripes[i].rG.in[j][0] && stripes[i].rG.out[j][0]) { // indegree=0 and outdegree>0
                if(stripes[i].rG.out[j][0]<0){exit(0);}
                flag = 1;
                from = stripes[i].vertex_to_peerNode[j];

                for(int x=1; x<=stripes[i].rG.out[j][0]; ++x) {
                    to = stripes[i].vertex_to_peerNode[stripes[i].rG.out[j][x]];
                    if(!edgeMap[make_pair(from, to)]) { 
                        edgeMap[make_pair(from, to)] = i+1;
                        // -- add edge   peer_node_from -> peer_node_to
                        isap.addEdge(from+1, _p+1+to, 1);
                        ++isap.edgeNum[1];
                    } else { 
                        if(stripes[i].rG.in[stripes[i].rG.out[j][x]][0] == 1) 
                            edgeMap[make_pair(from, to)] = i+1;
                    }
                }
            }
        }
    }

    for(int i=0; i<_p; ++i) {
        // --add edge peer_node_to -> T
        isap.addEdge(_p+1+i, T, 1);
    }
    isap.edgeNum[2] = _p;
    int tmp = 0;
    for(int i=0; i<isap.edge_level; ++i) {
        isap.edge_st_ed[i][0] = tmp;
        tmp += isap.edgeNum[i] << 1;
        isap.edge_st_ed[i][1] = tmp;
    }
}

void LinesDealScheduling::alterRepairGraph(int round, vector<Stripe>& stripes, map<pair<int, int>, int>& edgeMap, vector<pii>& up, vector<pii>& dw) {
    int sid, from, to, from_in_g, to_in_g;
    for(int i=isap.edge_st_ed[1][0]; i<isap.edge_st_ed[1][1]; i+=2) {
        if(isap.edges[i].flow) {
            to = isap.edges[i].to - 1 - _slaveCnt;
            from = isap.edges[i^1].to - 1;

            sid = edgeMap[make_pair(from, to)] - 1;
           
            for(int j=0; j<stripes[sid].rG._node_cnt; ++j) {
                if(stripes[sid].vertex_to_peerNode[j] == to) 
                    to_in_g = j;
                if(stripes[sid].vertex_to_peerNode[j] == from) 
                    from_in_g = j;
            }

            for(int x=1; x<=stripes[sid].rG.in[to_in_g][0]; ++x) {
                if(stripes[sid].rG.in[to_in_g][x] == from_in_g) {
                    stripes[sid].rG.in[to_in_g][x] = stripes[sid].rG.in[to_in_g][stripes[sid].rG.in[to_in_g][0]-1];
                    break;
                }
            }
            --stripes[sid].rG.in[to_in_g][0];
            for(int x=1; x<=stripes[sid].rG.out[from_in_g][0]; ++x) {
                if(stripes[sid].rG.out[from_in_g][x] == to_in_g) {
                    stripes[sid].rG.out[from_in_g][x] = stripes[sid].rG.out[from_in_g][stripes[sid].rG.out[from_in_g][0]-1];
                    break;
                }
            }
            --stripes[sid].rG.out[from_in_g][0];

            stripes[sid].repairTime[from_in_g][to_in_g] = round;
            if(up[from].first != -1) ER6;
            up[from].first = to;
            up[from].second = sid;
            if(dw[to].first != -1) ER6;
            dw[to].first = from;
            dw[to].second = sid;
            if(stripes[sid].rG.in[to_in_g][0] < 0 || stripes[sid].rG.out[from_in_g][0] < 0) {exit(0);}
        }
        
    }
}

int LinesDealScheduling::runISAP() {
    int max_bd = isap.SAP(isap.S, isap.T, isap.node_num);
    return max_bd;
}

void LinesDealScheduling::unitTestForMapping(vector<Stripe>& stripes, int* placement, int* isSoureCandidate, int failNodeId, int lostBlkCnt) {
    int p;
    int* is_place = (int*)malloc(sizeof(int)*_slaveCnt);
    for(int i=0; i<lostBlkCnt; ++i) {
        memset(is_place, 0, sizeof(int)*_slaveCnt);
        for(int j=0; j<_ecN; ++j) 
            is_place[placement[i*_ecN+j]] = 1; 
    
        for(int j=0; j<stripes[i].rG_bp._node_cnt; ++j) {
            p = stripes[i].vertex_to_peerNode[j];
            if(p == -1) ER0;
            if(p == failNodeId) ER1;
            // j is the replacement node
            if(j == stripes[i].rG._node_cnt-1) {  
                if(is_place[p]) ER2;
                continue;
            }
            // j is the source node
            if(!is_place[p]) ER3; 
            if(is_place[p] == -1) ER4;
            is_place[p] = -1;
        }
    }
}

void LinesDealScheduling::unitTestForSchedulingScheme(vector<Stripe>& stripes, int schedul_method, int fail_node, int lostBlkCnt, vector<vector<pii> >& upload_to, vector<vector<pii> >& download_from) {

    // -- check if all edges are scheduled 
    if(schedul_method == SCHEDUL_METHOD_BOOST) 
        for(int i=0; i<lostBlkCnt; ++i) {
            for(int j=0; j<stripes[i].rG._node_cnt; ++j) {
                if(stripes[i].rG.in[j][0] || stripes[i].rG.out[j][0]) {
                    cout << "ERR: check wrong, isap not finish" << endl;
                    exit(1);
                }
            }
        }
        
    // -- Check whether the repair sequence matches the topology
    // For each point in the stripe, all downloads need to be completed before upload
    int to, from;
    int latest_dw, earliest_up;
    for(int i=0; i<lostBlkCnt; ++i) {
        for(int j=0; j<stripes[i].rG_bp._node_cnt; ++j) {
            if(!stripes[i].rG_bp.out[j][0] || !stripes[i].rG_bp.in[j][0]) continue;  
            latest_dw = -1, earliest_up = INF;
            for(int x=1; x<=stripes[i].rG_bp.out[j][0]; ++x) {
                to = stripes[i].rG_bp.out[j][x];
                earliest_up = min(earliest_up, stripes[i].repairTime[j][to]);
            }
            for(int x=1; x<=stripes[i].rG_bp.in[j][0]; ++x) {
                from = stripes[i].rG_bp.in[j][x];
                latest_dw = max(latest_dw, stripes[i].repairTime[from][j]);
            }
            if(earliest_up <= latest_dw) ER5;

        }
    }
}

vector<Stripe> LinesDealScheduling::random_select(int fail_node, int repair_method, int lostBlkCnt, int* idxs, int* placement, int* isSoureCandidate) {
    cout << typeid(this).name() << "::" << __func__ << " starts\n";

    repairGraph G, localG;
    buildStripeGraph(G, localG, repair_method);

    vector<Stripe> stripes;
    for(int i=0; i<lostBlkCnt; ++i) {
        if(_conf->_ecType == "LRC" && idxs[i] < _ecK+_ec_LRC_L) {
            stripes.push_back(Stripe(localG));
        } else {
            stripes.push_back(Stripe(G));
        }
    }
    srand(time(0));
    int rid;
    int* randArray = (int*)malloc(sizeof(int)*max(_ecN, _slaveCnt));
    int* vis = (int*)malloc(sizeof(int)*_slaveCnt);
    vector<int> v;
    for(int i=0; i<lostBlkCnt; ++i) {
        memset(vis, 0, sizeof(int)*_slaveCnt);
        // -- source nodes
        int p = 0;
        for(int j=0; j<_ecN; ++j) {
            vis[placement[i*_ecN+j]] = 1;
            if(!isSoureCandidate[i*_ecN+j]) continue;
            if(placement[i*_ecN+j] == fail_node) continue;
            randArray[p++] = placement[i*_ecN+j];
        }
        for(int j=0; j<stripes[i].rG._node_cnt-1; ++j) {
            rid = rand()%p;
            stripes[i].vertex_to_peerNode[j] = randArray[rid];
            randArray[rid] = randArray[p-1];
            --p;
        }
        // -- replacement node
        p = 0;
        for(int j=0; j<_slaveCnt; ++j) {
            if(vis[j]) continue;
            randArray[p++] = j;
        }

        rid = rand()%p;
        stripes[i].vertex_to_peerNode[stripes[i].rG._node_cnt-1] = randArray[rid];
    }

    unitTestForMapping(stripes, placement, isSoureCandidate, fail_node, lostBlkCnt);
   
    // outputNodeLoad(stripes, fail_node);

    return stripes;
}

vector<Stripe> LinesDealScheduling::least_recent_selected(int fail_node, int repair_method, int lostBlkCnt, int* idxs, int* placement, int* isSoureCandidate) {
    cout << typeid(this).name() << "::" << __func__ << " starts\n";

    repairGraph G, localG;
    buildStripeGraph(G, localG, repair_method);

    vector<Stripe> stripes;
    for(int i=0; i<lostBlkCnt; ++i) {
        if(_conf->_ecType == "LRC" && idxs[i] < _ecK+_ec_LRC_L) {
            stripes.push_back(Stripe(localG));
        } else {
            stripes.push_back(Stripe(G));
        }
    }

    srand(time(0));
    int pid;
    int *vis = (int *)malloc(sizeof(int) * _slaveCnt);
    vector<pii> timestamp;
    vector<vector<int>> change(_slaveCnt, vector<int>());
    vector<int> uuload(_slaveCnt, 0), ddload(_slaveCnt, 0);
    for (int i = 0; i < _slaveCnt; i++) timestamp.push_back({0, i});
    for (int i = 0; i < lostBlkCnt; ++i) {
        memset(vis, 0, sizeof(int) * _slaveCnt);
        for (int j = 0; j < _ecN; ++j) vis[placement[i * _ecN + j]] = 1;
        int cnt = 0, localK = stripes[i].rG._node_cnt - 1;
        sort(timestamp.begin(), timestamp.end());
        for(int j = 0; j < _slaveCnt && cnt < localK; ++j) {
            pid = timestamp[j].second;
            if(pid == fail_node || !vis[pid]) continue;
            timestamp[j].first = i + 1;
            change[pid].push_back(i+1);
            uuload[pid] += stripes[i].rG.out[cnt][0];
            ddload[pid] += stripes[i].rG.in[cnt][0];
            stripes[i].vertex_to_peerNode[cnt] = pid;
            ++cnt;
        }

        for(int j = 0; j < _slaveCnt; ++j) {
            pid = timestamp[j].second;
            if(!vis[pid]) {
                timestamp[j].first = i + 1;
                change[pid].push_back(i+1);
                uuload[pid] += stripes[i].rG.out[localK][0];
                ddload[pid] += stripes[i].rG.in[localK][0];
                stripes[i].vertex_to_peerNode[localK] = pid;
                break;
            }
        }
    }

    unitTestForMapping(stripes, placement, isSoureCandidate, fail_node, lostBlkCnt);
    // outputNodeLoad(stripes, fail_node);

    return stripes;
}

vector<Stripe> LinesDealScheduling::heterogeneous(int failNodeId, int repair_method, int lostBlkCnt, int* idxs, int* placement, int* isSoureCandidate) {

    printf("linkWeightFile %s\n", _conf->_linkWeightFile.c_str());
    ifstream ifs(_conf->_linkWeightFile);
    if(!ifs) {
        cerr << __func__ << " opening file error" << endl;
        exit(-1);
    }
    int bandwidth_weight[_slaveCnt*_slaveCnt];
    for(int i=0; i<_slaveCnt; ++i) {
        for(int j=0; j<_slaveCnt; ++j) {
            ifs >> bandwidth_weight[i * _slaveCnt + j];
        }
    }
    ifs.close();

    cout << typeid(this).name() << "::" << __func__ << " starts\n";

    repairGraph G, localG;
    buildStripeGraph(G, localG, repair_method);
    vector<Stripe> stripes;
    for(int i=0; i<lostBlkCnt; ++i) {
        if(_conf->_ecType == "LRC" && idxs[i] < _ecK+_ec_LRC_L) {
            stripes.push_back(Stripe(localG));
        } else {
            stripes.push_back(Stripe(G));
        }
    }

    int** cur_load = (int**)malloc(2 * sizeof(int*)); // 0 up, 1 dw
    for(int i=0; i<2; ++i) cur_load[i] = (int*)calloc(_slaveCnt, sizeof(int));
    int* vis = (int*)calloc(_slaveCnt, sizeof(int));
    
    map<int, bool> isStore;
    vector<piii> peers;
    int pid, to, from;
    int bottleNeckFlag = 0; // 0 up, 1 dw
    for(int i=0; i<lostBlkCnt; ++i) {
        memset(vis, 0, sizeof(int)*_slaveCnt);
        peers.clear();
        for(int j=0; j<_ecN; ++j) {
            pid = placement[i*_ecN+j];
            vis[pid] = 1;
            if(pid == failNodeId) continue;
            peers.push_back({{cur_load[bottleNeckFlag][pid], cur_load[~bottleNeckFlag][pid]}, pid});
        }
        sort(peers.begin(), peers.end(), piii_cmp);
        for(int h=0; h<stripes[i].rG_bp._node_cnt-1; ++h) {
            pid = peers[h].second;
            stripes[i].vertex_to_peerNode[h] = pid;
        }

        int max_up = 0, max_dw = 0; 
        for(int j=0; j<_slaveCnt; ++j) {
            max_up = max(max_up, cur_load[0][j]);
            max_dw = max(max_dw, cur_load[1][j]);
        }
        bottleNeckFlag = (max_up>max_dw) ? 0 : 1;

        peers.clear();
        for(int j=0; j<_slaveCnt; ++j) {
            if(vis[j]) continue;
            peers.push_back({{cur_load[1][j], cur_load[0][j]}, j});
        }
        sort(peers.begin(), peers.end(), piii_cmp);
        pid = peers[0].second;
        stripes[i].vertex_to_peerNode[stripes[i].rG_bp._node_cnt-1] = pid;

        int from_pid, to_pid;
        for(int h=0; h<stripes[i].rG_bp._node_cnt; ++h) {
            for(int x=1; x<=stripes[i].rG_bp.in[h][0]; ++x) {
                from = stripes[i].rG_bp.in[h][x];
                from_pid = stripes[i].vertex_to_peerNode[from];
                to = h;
                to_pid = stripes[i].vertex_to_peerNode[to];
                cur_load[1][to_pid] += bandwidth_weight[from_pid*_slaveCnt + to_pid];
                cur_load[0][from_pid] += bandwidth_weight[from_pid*_slaveCnt + to_pid];
            }
        }

        max_up = 0, max_dw = 0; 
        for(int j=0; j<_slaveCnt; ++j) {
            max_up = max(max_up, cur_load[0][j]);
            max_dw = max(max_dw, cur_load[1][j]);
        }
        bottleNeckFlag = (max_up>max_dw) ? 0 : 1;
        
    }
    unitTestForMapping(stripes, placement, isSoureCandidate, failNodeId, lostBlkCnt);


    int flag, max_bd, round = 0;
    map<_piii, int> edgeMap; 
    vector<vector<pii> > upload_to, download_from; // (peer_node, stripe_id)
    upload_to.clear();
    download_from.clear();

    int** slave_load = (int**)malloc(2 * sizeof(int*)); // 0 up, 1 dw
    for(int i=0; i<2; ++i) slave_load[i] = (int*)calloc(_slaveCnt, sizeof(int));
    while(true) {
        flag = 0;
        edgeMap.clear();
        heterogeneous_init_graph(stripes, flag, edgeMap, lostBlkCnt);
        if(!flag) break;
        ++round;
        vector<pii> up(_slaveCnt);
        vector<pii> dw(_slaveCnt);
        for(int i=0; i<_slaveCnt; ++i) 
            up[i] = {-1,-1}, dw[i] = {-1, -1};
        heterogeneous_alter_repairGraph(failNodeId, bandwidth_weight, round, stripes, edgeMap, up, dw, slave_load);
        upload_to.push_back(up);
        download_from.push_back(dw);
    }

    unitTestForSchedulingScheme(stripes, SCHEDUL_METHOD_BOOST, failNodeId, lostBlkCnt, upload_to, download_from);

    return stripes;
}

void LinesDealScheduling::heterogeneous_init_graph(vector<Stripe>& stripes, int& flag, map<_piii, int>& edgeMap, int lostBlkCnt) {
    int from, to;
    for(int i=0; i<lostBlkCnt; ++i) {
        for(int j=0; j<stripes[i].rG._node_cnt; ++j) {
            if(!stripes[i].rG.in[j][0] && stripes[i].rG.out[j][0]>0) { 
                if(stripes[i].rG.out[j][0]<0) {exit(0);}
                flag = 1;
                from = stripes[i].vertex_to_peerNode[j];
                for(int x=1; x<=stripes[i].rG.out[j][0]; ++x) {
                    to = stripes[i].vertex_to_peerNode[stripes[i].rG.out[j][x]];
                    if(!edgeMap[{i, {from, to}}]) edgeMap[{i, {from, to}}] = 1; 
                }
            }
        }
    }

}

void LinesDealScheduling::heterogeneous_alter_repairGraph(int fail_node_id, int* bandwidth_weight, int round, vector<Stripe>& stripes, map<_piii, int>& edgeMap, vector<pii>& up, vector<pii>& dw, int** cur_slave_load) {
    int pid, sid;
    int ok, tmp_weight, tmp_sid;
    map<_piii, int>::iterator it;
    vector<piii> peers;
    int max_slave_up = 0, max_slave_dw = 0;
    for(int i=0; i<_slaveCnt; ++i) {
        if(i == fail_node_id) continue;
        max_slave_dw = max(max_slave_dw, cur_slave_load[1][i]);
        max_slave_up = max(max_slave_up, cur_slave_load[0][i]);
    }
    int bottleneckFlag = (max_slave_up>max_slave_dw) ? 0 : 1;

    peers.clear();
    for(int j=0; j<_slaveCnt; ++j) {
        if(j == fail_node_id) continue;
        peers.push_back({{cur_slave_load[~bottleneckFlag][j], cur_slave_load[bottleneckFlag][j]}, j});
    }
    sort(peers.begin(), peers.end(), piii_cmp);
    int from_to[2];
    for(int i=0; i<peers.size(); ++i) {
        int find;
        ok = 0;
        tmp_weight = -1;
        pid = peers[i].second;
        for(it=edgeMap.begin(); it!=edgeMap.end(); ++it) {
            from_to[0] = it->first.second.first;
            from_to[1] = it->first.second.second;
            if(it->second == 1 && from_to[~bottleneckFlag] == pid) {
                ok = 1;
                if(tmp_weight == -1 || bandwidth_weight[from_to[0]*_slaveCnt+from_to[1]] < tmp_weight) {
                    tmp_weight = bandwidth_weight[from_to[0]*_slaveCnt+from_to[1]];
                    find = from_to[bottleneckFlag];
                    tmp_sid = it->first.first;
                } 
            }
        }
        
        if(ok) {
            sid = tmp_sid;
            cur_slave_load[0][find] += tmp_weight;
            cur_slave_load[1][pid] += tmp_weight;

            if(!bottleneckFlag) edgeMap[{sid, {find, pid}}] = -1;
            else edgeMap[{sid, {pid, find}}] = -1;
            int vfrom_to[2];
            for(int j=0; j<stripes[sid].rG_bp._node_cnt; ++j) {
                if(stripes[sid].vertex_to_peerNode[j] == find) vfrom_to[bottleneckFlag] = j;
                if(stripes[sid].vertex_to_peerNode[j] == pid) vfrom_to[~bottleneckFlag] = j;
            }
            stripes[sid].repairTime[vfrom_to[0]][vfrom_to[1]] = round;

            for(int x=1; x<=stripes[sid].rG.in[vfrom_to[1]][0]; ++x) {
                if(stripes[sid].rG.in[vfrom_to[1]][x] == vfrom_to[0]) {
                    stripes[sid].rG.in[vfrom_to[1]][x] = stripes[sid].rG.in[vfrom_to[1]][stripes[sid].rG.in[vfrom_to[1]][0]-1];
                    break;
                }
            }
            --stripes[sid].rG.in[vfrom_to[1]][0];
            for(int x=1; x<=stripes[sid].rG.out[vfrom_to[0]][0]; ++x) {
                if(stripes[sid].rG.out[vfrom_to[0]][x] == vfrom_to[1]) {
                    stripes[sid].rG.out[vfrom_to[0]][x] = stripes[sid].rG.out[vfrom_to[0]][stripes[sid].rG.out[vfrom_to[0]][0]-1];
                    break;
                }
            }
            --stripes[sid].rG.out[vfrom_to[0]][0];
            break;
        } 
    }

}

void LinesDealScheduling::outputNodeLoad(vector<Stripe>& stripes, int fail_node) {
    int* load_upload = (int*)calloc(sizeof(int), _slaveCnt);;
    int* load_dwload = (int*)calloc(sizeof(int), _slaveCnt);
    int lostBlkCnt = stripes.size();
    int max_up = -1, min_up = INF, max_dw = -1, min_dw = INF;
    for(int i=0; i<lostBlkCnt; ++i) {
        for(int j=0; j<stripes[i].rG._node_cnt; ++j) {
            load_upload[stripes[i].vertex_to_peerNode[j]] += stripes[i].rG.out[j][0]; 
            load_dwload[stripes[i].vertex_to_peerNode[j]] += stripes[i].rG.in[j][0];
        }
    }
    for(int i=0; i<_slaveCnt; ++i) {
        if(i == fail_node) continue;
        max_up = max(max_up, load_upload[i]);
        min_up = min(min_up, load_upload[i]);
        max_dw = max(max_dw, load_dwload[i]);
        min_dw = min(min_dw, load_dwload[i]);
    }
    cout << "max_up = " << max_up << " , min_up = " << min_up << endl;
    cout << "max_dw = " << max_dw << " , min_dw = " << min_dw << endl;
}