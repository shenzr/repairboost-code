#ifndef _LINES_STRIPE_HH_
#define _LINES_STRIPE_HH_

#include <string>
#include <cstring>
#include <iostream>
using namespace std;

typedef pair<int, int> pii;
typedef pair<pair<int, int>, int>  piii;
typedef pair<int, pii>  _piii;

struct repairGraph {
    int _node_cnt;
    int **in, **out;

    repairGraph(){}
    repairGraph(int node_cnt) {
        _node_cnt = node_cnt;
        in = (int**)malloc(sizeof(int*)*_node_cnt);
        out = (int**)malloc(sizeof(int*)*_node_cnt);
        for(int i=0; i<_node_cnt; ++i) {
            in[i] = (int*)malloc(sizeof(int)*_node_cnt);
            out[i] = (int*)malloc(sizeof(int)*_node_cnt);
            in[i][0] = out[i][0] = 0;
        }
    }
    repairGraph operator=(repairGraph& G) {
        _node_cnt = G._node_cnt;
        in = (int**)malloc(sizeof(int*)*_node_cnt);
        out = (int**)malloc(sizeof(int*)*_node_cnt);
        for(int i=0; i<_node_cnt; ++i) {
            in[i] = (int*)malloc(sizeof(int)*_node_cnt);
            out[i] = (int*)malloc(sizeof(int)*_node_cnt);
            memcpy(in[i], G.in[i], sizeof(int)*_node_cnt);
            memcpy(out[i], G.out[i], sizeof(int)*_node_cnt);
        }
    }

    void display() {
        cout << "node_cnt = " << _node_cnt << endl;
        for(int i=0; i<_node_cnt; ++i) {
            cout << "in_degree[" << i << "] = " << in[i][0] << endl;
            for(int j=1; j<=in[i][0]; ++j) cout << in[i][j] << " ";
            cout << endl;

            cout << "out_degree[" << i << "] = " << out[i][0] << endl;
            for(int j=1; j<=out[i][0]; ++j) cout << out[i][j] << " ";
            cout << endl;
        }
    }
};


class Stripe {
    public:
        repairGraph rG;
        repairGraph rG_bp; // back_up for rG
        int** repairTime; // repairTime[i][j] i->j
        pii** up_dw_time; // upload time and download time 
        int* vertex_to_peerNode;
        Stripe(){}                                      
        Stripe(repairGraph G) {
            rG = G;
            rG_bp = G;

            vertex_to_peerNode = (int*)malloc(sizeof(int)*rG._node_cnt);
            memset(vertex_to_peerNode, -1, sizeof(int)*rG._node_cnt);

            repairTime = (int**)malloc(sizeof(int*)*rG._node_cnt);
            up_dw_time = (pii**)malloc(sizeof(pii*)*rG._node_cnt);
            for(int i=0; i<rG._node_cnt; ++i) {
                repairTime[i] = (int*)malloc(sizeof(int)*rG._node_cnt);
                memset(repairTime[i], -1, sizeof(int)*rG._node_cnt);
                up_dw_time[i] = (pii*)malloc(sizeof(pii)*rG._node_cnt);
                for(int j=0; j<rG._node_cnt; ++j)
                    up_dw_time[i][j] = make_pair(-1, -1);
            }
        }
        
};

#endif  //_LINES_STRIPE_HH_

