#include "LRCUtil.hh"

LRCUtil::LRCUtil(Config* conf, int* encMat) : _encMat(encMat) {
    // Azure LRC (k,l,r) 
    _ecN = conf->_ecN;
    _ecK = conf->_ecK;

    _LRC_L = conf->_lrcL;
    _LRC_G = _ecN - _ecK - _LRC_L;
    
    cout << __func__ << ": data =" << _ecK << " local parity =" << _LRC_L << " global parity = " << _LRC_G << endl;

    _completeEncMat = (int*)calloc(_ecK * _ecN, sizeof(int));

    // k data blocks
    for(int i=0; i<_ecK; ++i) {
        _completeEncMat[i * _ecK + i] = 1;
    }

    // l local parities and g global parities
    memcpy((char*)_completeEncMat + _ecK * _ecK * sizeof(int), (char*)_encMat, _ecK * (_LRC_L + _LRC_G) * sizeof(int));

    printf("%s: CompleteEncMat for LRC:\n", __func__);
    for (int i = 0; i < _ecN; i++) {
        for (int j = 0; j < _ecK; j++)
            printf("%4d ", _completeEncMat[i * _ecK + j]);
        printf("\n");
    }

    _group_id = conf->_group_id;
}

void LRCUtil::multiply(char* blk, int mulby, int size) {
    galois_w08_region_multiply(blk, mulby, size, blk, 0);
}

int* LRCUtil::getCoefficient_specifiedBlks(int idx, int* status) {
    int selectedBlkCnt = 0;
    int* coeff = (int*)calloc(_ecK, sizeof(int));
    if(idx >= _ecK+_LRC_L) { // global parity
        for(int i=0; i<_ecN; ++i) {
            if(status[i] && (i == idx)) return nullptr;
            if(status[i] && i>=_ecK && i<_ecK+_LRC_L) return nullptr;
            if(status[i]) ++ selectedBlkCnt;
        }

        if (selectedBlkCnt != _ecK) return nullptr;

        int* eMat = (int*)calloc(_ecK * _ecK, sizeof(int));
        int* dMat = (int*)calloc(_ecK * _ecK, sizeof(int));
        for(int i=0, cnt=0; i<_ecN; ++i) {
            if(status[i]) {
                memcpy((char*)eMat + cnt*_ecK*sizeof(int), (char*)_completeEncMat + i*_ecK*sizeof(int), _ecK*sizeof(int));
                ++cnt;
            }
        }

        jerasure_invert_matrix(eMat, dMat, _ecK, 8);
        coeff = jerasure_matrix_multiply(_completeEncMat + idx * _ecK, dMat, 1, _ecK, _ecK, _ecK, 8);

        free(eMat);
        free(dMat);
    } else { // data or local parity
        int nr = _ecK/_LRC_L;
  
        for(int i=0; i<_ecN; ++i) {
            if(status[i] && (i == idx)) return nullptr;
            if(status[i] && (_group_id[i]!=_group_id[idx])) return nullptr;
            if(status[i]) ++ selectedBlkCnt;
        }

        if (selectedBlkCnt != nr) return nullptr;
        int* eMat = (int*)calloc(nr * nr, sizeof(int));
        int* dMat = (int*)calloc(nr * nr, sizeof(int));
        for(int i=0, cnt=0; i<_ecN; ++i) {
            if(status[i]) {
                memcpy((char*)eMat + cnt*nr*sizeof(int), (char*)_completeEncMat + (i*_ecK+_group_id[idx]*nr)*sizeof(int), nr*sizeof(int));
                ++cnt;
            }
        }

        jerasure_invert_matrix(eMat, dMat, nr, 8);
        coeff = jerasure_matrix_multiply(_completeEncMat + idx * _ecK + _group_id[idx]*nr, dMat, 1, nr, nr, nr, 8);

        free(eMat);
        free(dMat);
    }
    
    return coeff;
}
