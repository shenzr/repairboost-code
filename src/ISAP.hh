#ifndef _ISAP_HH_
#define _ISAP_HH_
#include <queue>

#include <cstdio>
#include <cstring>
#include <iostream>

using namespace std;

const int N = 1000005, M = 2000005;
const int INF = 0x3f3f3f3f;

struct Edge {
    int to, cap, next, flow;
    Edge(int _to=0, int _cap=0, int _next=0, int _flow=0): to(_to), cap(_cap), next(_next), flow(_flow) {}
};

struct ISAP {
public:
    int S, T;
    int node_num, tot;
    int head[M<<1];
    int level[N], gap[N], cur[N], stack[N];
    int edge_level;
    int edgeNum[5], edge_st_ed[5][2];
    Edge edges[M<<1];
    // init S, T, n, m
    ISAP(int _S=0, int _T=0, int _n=0, int _tot=0) : S(_S), T(_T), node_num(_n), tot(_tot){};
    void init(int n, int _S, int _T);
    void addEdge(int u, int v, int w); 
    void bfs(int vs, int vt); 
    int SAP(int vs, int vt, int n); 
    void printFlow(int n);
};

#endif 