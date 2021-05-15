#include <fstream>
#include <iostream>
#include <string>
#include <string.h>
#include <gf_complete.h>

#include <vector>
using namespace std;

int* generate_encoding_matrix(int k, int n, int alpha) {

    int* encMat = (int*)calloc(k * alpha * (n-k) * alpha, sizeof(int));

    // parity H
    int temp = 0;
    for (int i = 0; i < alpha; i++) {
        for (int j = 0; j < k; j++) {
            if (i % alpha == temp) {
                encMat[i * k * alpha + alpha * j + temp] = 1;
            }
        }
        temp++;
    }

    // parity B
    int B[8 * 32] = {
        0,0,0,0,0,0,0,1, 0,0,0,1,0,0,0,0, 0,1,0,0,0,0,0,0, 1,0,0,0,0,0,0,0, // d7+c3+b1+a0
        0,0,0,0,0,0,1,0, 0,0,1,0,0,0,0,0, 1,0,0,0,0,0,0,0, 1,1,0,0,0,0,0,0, 
        0,0,0,0,0,1,0,0, 0,1,0,0,0,0,0,0, 0,1,0,1,0,0,0,0, 0,1,1,1,0,0,0,0,
        0,0,0,0,1,0,0,0, 1,0,0,0,0,0,0,0, 1,0,1,0,0,0,0,0, 1,0,0,1,0,0,0,0, 
        0,0,0,1,0,0,0,0, 0,0,0,1,0,0,0,1, 0,0,0,1,0,1,0,1, 0,0,0,1,1,0,0,1, 
        0,0,1,0,0,0,0,0, 0,0,1,0,0,0,1,0, 0,0,1,0,1,0,1,0, 0,0,1,0,1,1,1,0,
        0,1,0,0,0,0,0,0, 0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,1,0,0,0,0,1,1, 
        1,0,0,0,0,0,0,0, 1,0,0,0,1,0,0,0, 1,0,0,0,0,0,1,0, 1,0,0,0,0,0,0,1}; // d0+c0+b0+a0+c4+b6+a7
    memcpy(encMat + 8 * 32, B, 8 * 32 * sizeof(int));


    // encMat 
    cout << "encoding matrix for BUTTERFLY (" << n << "," << k << ")" << endl;
    for(int i=0; i<n-k; ++i) {
        for(int r=0; r<alpha; ++r) {
            for(int j=0; j<alpha * k; ++j) {
                cout << encMat[i*alpha*(k*alpha)+r*alpha*k+j] << " ";
            }
            cout << endl;
        }
        cout << endl;
    }

    return encMat;
}

void region_xor(char* r1, char* r2, int len) {
    for(int i=0; i<len; i++) r1[i] ^= r2[i];
}

void usage() {
    cout << "usage:" << endl;
    cout << "	./createdata_butterfly  file " << endl;
    cout << "	file: your raw data" << endl;
}

int main(int argc, char *argv[]) {
    // check number of arguments
    if (argc != 2) {
        cout << "bad number of input arguments!" << endl;
        usage();
        return -1;
    }
    
    int k = 4, n = 6, m = 2, alpha = 8;
    int* encMat = generate_encoding_matrix(k, n, alpha);

    // check input file
    FILE *inputfile = fopen(argv[1], "r");
    string inputpath(argv[1]);
    if (inputfile == NULL) {
        cout << "raw input file does not exist!" << endl;
        usage();
        return -1;
    }
    fseek(inputfile, 0, SEEK_END);
    int size = ftell(inputfile);
    fseek(inputfile, 0, SEEK_SET);

    // check blocksize
    int blocksize = size / k;
    if (blocksize * k != size) {
        cout << "input file size should divisable by k" << endl;
        return -1;
    }
    int subBlockSize = blocksize / alpha;
    // cout << blocksize << " " << subBlockSize << endl;


    char **data = (char **)malloc(sizeof(char *) * k);
    for (int i = 0; i < k; i++)
        data[i] = (char *)malloc(sizeof(char) * blocksize);
    char **coding = (char **)malloc(sizeof(char *) * m);
    for (int i = 0; i < m; i++)
        coding[i] = (char *)malloc(sizeof(char) * blocksize);

    // get input data
    char *rawcontent = (char *)malloc(sizeof(char) * size);
    size_t sizeread = fread(rawcontent, sizeof(char), size, inputfile);
    for (int i = 0; i < k; i++) data[i] = rawcontent + (i * blocksize);

    // generate parity blocks
    int offset1, offset2;
    for(int i=0; i < m; i++) {
        for(int j=0; j < alpha; j++) {
            int offset1 = j * subBlockSize;
            for(int z=0; z < alpha/2*alpha; z++) {
                if(encMat[alpha/2*alpha*(i*alpha+j)+z]) { // coef = 1
                    offset2 = (z % alpha) * subBlockSize;
                    region_xor(coding[i]+offset1, data[z/alpha]+offset2, subBlockSize);
                }
            }
        }
    }

    // write data into blocks
    for (int i = 1; i <= k; i++) {
        char *blockname = (char *)malloc(sizeof(char) * 20);
        sprintf(blockname, "file_k%d", i);
        FILE *blockfile = fopen(blockname, "wb");
        fwrite(data[i - 1], sizeof(char), blocksize, blockfile);
    }
    for (int i = 1; i <= m; i++) {
        char *blockname = (char *)malloc(sizeof(char) * 20);
        sprintf(blockname, "file_m%d", i);
        FILE *blockfile = fopen(blockname, "wb");
        fwrite(coding[i - 1], sizeof(char), blocksize, blockfile);
    }

    return 0;
}
