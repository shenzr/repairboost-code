#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <gf_complete.h>

#include "Util/jerasure.h"

using namespace std;

void usage() {
    cout << "usage:" << endl;
    cout << "	./createdata_rs matrix file k n" << endl;
    cout << "	matrix: path to your coding matrix" << endl;
    cout << "	file: your raw data" << endl;
    cout << "	k: erasure code parameter k" << endl;
    cout << "	n: erasure code parameter n" << endl;
}

int main(int argc, char *argv[]) {
    // check number of arguments
    if (argc != 5) {
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
    string::size_type sz;
    string kstr(argv[3]);
    int k = stoi(kstr, &sz);
    string nstr(argv[4]);
    int n = stoi(nstr, &sz);
    int m = n - k;

    // check blocksize
    int blocksize = size / k;
    if (blocksize * k != size) {
        cout << "input file size should divisable by k" << endl;
        return -1;
    }

    // get coding matrix
    ifstream ifs(matrixpath);
    int *rsEncMat = (int *)malloc(sizeof(int) * m * k);
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < k; j++) {
            ifs >> rsEncMat[k * i + j];
        }
    }

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

    jerasure_matrix_encode(k, m, 8, rsEncMat, data, coding, blocksize);

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
