#include "Computation.hh"

#define ULL unsigned long long

/**
 * TODO: We assume the buffer length is a multiple of sizeof(unsigned long long)
 */
void Computation::XORBuffers(char* buf1, char* buf2, size_t length) {
    ULL* lbuf1 = (ULL*)buf1;
    ULL* lbuf2 = (ULL*)buf2;
    size_t i = 0, llen = length / sizeof(ULL);
    for (i = 0; i < llen; i++) lbuf1[i] ^= lbuf2[i];
}
