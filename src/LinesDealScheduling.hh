#ifndef _LINES_DEAL_SCHEDULING_HH_
#define _LINES_DEAL_SCHEDULING_HH_
#include <vector>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <typeinfo>

#include "ISAP.hh"
#include "LinesDefine.hh"
#include "Coordinator.hh"
#include "LinesStripe.hh"

// error messages
#define ER0 {cout << "Correctness test fail!" << endl << "ER0:vertex map to -1" << endl; exit(1);}
#define ER1 {cout << "Correctness test fail!" << endl << "ER1:vertex map to fail_node" << endl; exit(1);}
#define ER2 {cout << "Correctness test fail!" << endl << "ER2:vertex map to wrong replacement node" << endl; exit(1);}
#define ER3 {cout << "Correctness test fail!" << endl << "ER3:vertex map to wrong source node" << endl; exit(1);}
#define ER4 {cout << "Correctness test fail!" << endl << "ER4:different vertexs map to the same peer node" << endl; exit(1);}
#define ER5 {cout << "Correctness test fail!" << endl << "ER5:Topology not satisfied" << endl; exit(1);}
#define ER6 {cout << "Correctness test fail!" << endl << "ER6:peer_node send/receive more than one chunk in the round" << endl; exit(1);}
#define ER7 {cout << "Correctness test fail!" << endl << "ER7:occupy the same round more than once" << endl; exit(1);}

class LinesDealScheduling {
    protected:
        int _slaveCnt;
        int _ecK;
        int _ecN;
        int _ecM;
        int _ec_LRC_L;
        int _ec_LRC_G;

        ISAP isap;
        Config* _conf;
        void ppr_graph(int, repairGraph&);
        void buildGraph(int, repairGraph&, int);
        void buildStripeGraph(repairGraph&, repairGraph&, int);

    public:
        void initConf(Config*);

        vector<Stripe> repairboost(int, int, int, int*, int*, int*);
        vector<Stripe> random_select(int, int, int, int*, int*, int*);
        vector<Stripe> least_recent_selected(int, int, int, int*, int*, int*);
        
        int runISAP();
        void initGraph(vector<Stripe>&, int&, map<pair<int, int>, int>&, int);
        void alterRepairGraph(int, vector<Stripe>&, map<pair<int, int>, int>&, vector<pii>&, vector<pii>&);

        void unitTestForMapping(vector<Stripe>&, int*, int*, int, int);
        void unitTestForSchedulingScheme(vector<Stripe>&, int, int, int, vector<vector<pii> >&, vector<vector<pii> >&);
        void outputNodeLoad(vector<Stripe>&, int);

        vector<Stripe> heterogeneous(int, int, int, int*, int*, int*);
        void heterogeneous_init_graph(vector<Stripe>&, int&, map<_piii, int>&, int);
        void heterogeneous_alter_repairGraph(int, int*, int, vector<Stripe>&, map<_piii, int>&, vector<pii>&, vector<pii>&, int**);
    
};


#endif  //_LINES_DEAL_SCHEDULING_HH_


