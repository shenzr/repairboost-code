#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <gf_complete.h>
#include "Util/jerasure.h"

using namespace std;

void usage() {
    cout << "usage:" << endl;
    cout << "	./createdata_lrc matrix file k l g" << endl;
    cout << "	matrix: path to your coding matrix" << endl;
    cout << "	file: your raw data" << endl;
    cout << "	k: erasure code parameter k" << endl;
    cout << "	l: local parities" << endl;
    cout << "	g: global parities" << endl;
}

int main(int argc, char *argv[]) {
    // check number of arguments
    if (argc != 6) {
        cout << "bad number of input arguments!" << endl;
        usage();
        return -1;
    }

    // check matrix file
    FILE *matrixfile = fopen(argv[1], "r");
    string matrixpath(argv[1]);
    if (matrixfile == NULL) {
        cout << "coding matrix file does not exist!" << endl;
        usage();
        return -1;
    }

    // check input file
    FILE *inputfile = fopen(argv[2], "r");
    string inputpath(argv[2]);
    if (inputfile == NULL) {
        cout << "raw input file does not exist!" << endl;
        usage();
        return -1;
    }
    fseek(inputfile, 0, SEEK_END);
    int size = ftell(inputfile);
    fseek(inputfile, 0, SEEK_SET);

    // get erasure coding parameters
    int k = stoi(argv[3]);
    int l = stoi(argv[4]);
    int g = stoi(argv[5]);

    // check blocksize
    int blocksize = size / k;
    if (blocksize * k != size) {
        cout << "input file size should divisable by k" << endl;
        return -1;
    }

    // get coding matrix
    ifstream ifs(matrixpath);
    int *rsEncMat = (int *)malloc(sizeof(int) * (l+g) * k);
    for (int i = 0; i < l+g; i++) {
        for (int j = 0; j < k; j++) {
            ifs >> rsEncMat[k * i + j];
        }
    }

    char **data = (char **)malloc(sizeof(char *) * k);
    for (int i = 0; i < k; i++)
        data[i] = (char *)malloc(sizeof(char) * blocksize);

    char **coding = (char **)malloc(sizeof(char *) * (l+g));
    for (int i = 0; i < l+g; i++)
        coding[i] = (char *)malloc(sizeof(char) * blocksize);

    // get input data
    char *rawcontent = (char *)malloc(sizeof(char) * size);
    size_t sizeread = fread(rawcontent, sizeof(char), size, inputfile);
    for (int i = 0; i < k; i++) data[i] = rawcontent + (i * blocksize);

    jerasure_matrix_encode(k, l+g, 8, rsEncMat, data, coding, blocksize);

    // write data into blocks
    for (int i = 1; i <= k; i++) {
        char *blockname = (char *)malloc(sizeof(char) * 20);
        sprintf(blockname, "file_k%d", i);
        FILE *blockfile = fopen(blockname, "wb");
        fwrite(data[i - 1], sizeof(char), blocksize, blockfile);
    }
    for (int i = 1; i <= l+g; i++) {
        char *blockname = (char *)malloc(sizeof(char) * 20);
        sprintf(blockname, "file_m%d", i);
        FILE *blockfile = fopen(blockname, "wb");
        fwrite(coding[i - 1], sizeof(char), blocksize, blockfile);
    }

    return 0;
}
