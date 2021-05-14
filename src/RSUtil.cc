#include "RSUtil.hh"

RSUtil::RSUtil(Config* conf, int* encMat) : _encMat(encMat) {
    _ecN = conf->_ecN;
    _ecK = conf->_ecK;
    _ecM = _ecN - _ecK;
    
    _completeEncMat = (int*)calloc(_ecN * _ecK, sizeof(int));
    for(int i=0; i<_ecK; ++i) _completeEncMat[i * _ecK + i] = 1;
    memcpy((char*)_completeEncMat + _ecK * _ecK * sizeof(int), (char*)_encMat, _ecK * _ecM * sizeof(int));

    printf("%s: RSEncMat:\n", __func__); 
    for (int i = 0; i < _ecN; i++) {
        for (int j = 0; j < _ecK; j++)
            printf("%4d ", _completeEncMat[i * _ecK + j]);
        printf("\n");
    }
}

void RSUtil::multiply(char* blk, int mulby, int size) {
    galois_w08_region_multiply(blk, mulby, size, blk, 0);
}

int* RSUtil::getCoefficient_specifiedBlks(int idx, int* status) {
    int selectedBlkCnt = 0;
    for (int i = 0; i < _ecN; i++) {
        if (status[i] && i == idx) return nullptr;
        if (status[i]) ++selectedBlkCnt;
    }
    if (selectedBlkCnt != _ecK) return nullptr;
    
    int* coeff = (int*)calloc(_ecK, sizeof(int));
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
    return coeff;
}