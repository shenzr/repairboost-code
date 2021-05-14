#include "ISAP.hh"

// init S, T, n, m
void ISAP::init(int n, int _S, int _T) {
    node_num = n;
    S = _S;
    T = _T;
    tot = 0;
    memset(head, -1, sizeof head);
    for(int i=0; i<5; ++i) edgeNum[i] = 0;
}

void ISAP::addEdge(int u, int v, int w) {
    edges[tot] = Edge(v, w, head[u], 0);
    head[u] = tot++;
    // reverse edges
    edges[tot] = Edge(u, 0, head[v], 0);
    head[v] = tot++;
}    

void ISAP::bfs(int vs, int vt) {
    memset(gap, 0, sizeof gap);
    memset(level, -1, sizeof level);

    queue<int> que;
    que.push(vt);
    level[vt] = 0;
    while(!que.empty()) {
        int x = que.front(); que.pop();
        for(int i=head[x]; i!=-1; i=edges[i].next) {
            int v = edges[i].to;
            if(level[v] != -1) continue;
            que.push(v);
            level[v] = level[x]+1;
            ++gap[level[v]];
        }
    }
}

int ISAP::SAP(int vs, int vt, int n) {
    bfs(vs, vt);
    for(int i=0;i<n;++i) cur[i] = head[i];
    int stackTop = 0;
    int u = vs, v;
    int max_flow = 0, flag;
    while(level[vs] < n) {
        flag = 0;
        // augment road 
        if(u == vt) {
            int aug = INF, neck = -1;
            for(int i=0;i<stackTop;++i) {
                if(edges[stack[i]].cap-edges[stack[i]].flow < aug) {
                    aug = edges[stack[i]].cap-edges[stack[i]].flow;
                    neck = i;
                }
            }
            for(int i=0;i<stackTop;++i) {
                edges[stack[i]].flow += aug;
                edges[stack[i]^1].flow -= aug;
            }
            max_flow += aug;
            stackTop = neck;
            u = edges[stack[stackTop]^1].to;
        }

        for(int i=cur[u]; i!=-1; i=edges[i].next) {
            v = edges[i].to;
            if(edges[i].cap>edges[i].flow && level[u]==level[v]+1) {
                flag = 1;
                cur[u] = i;
                stack[stackTop++] = cur[u];
                u = v;
                break;
            }
        }

        if(!flag) {
            if(!--gap[level[u]]) break; 
            int minLevel = n;
            for(int i=head[u]; i!=-1; i=edges[i].next) {
                v = edges[i].to;
                if(edges[i].cap>edges[i].flow && level[v]<minLevel) { 
                    minLevel = level[v];
                    cur[u] = i;
                }
            }
            level[u] = minLevel+1;
            ++gap[level[u]];
            if(u!=vs) u = edges[stack[--stackTop]^1].to;
        }
    }
    return max_flow;
}

void ISAP::printFlow(int n) {
    cout<< tot << endl;
    for(int i=0;i<tot;i+=2) {
        if(!edges[i].flow) continue;
        cout << i << " ("<< edges[i^1].to <<", "<< edges[i].to << ") "<< edges[i].flow << endl; 
    }
}
