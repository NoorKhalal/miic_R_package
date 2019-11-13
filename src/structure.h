#ifndef STRUCTURE_H
#define STRUCTURE_H
#include <string>
#include <vector>
#include <map>
#include <tuple>

#include "memory.h"

using namespace std;
using uint=unsigned int;

struct EdgeSharedInfo {
	int z_name_idx; // index of the last best contributor
	std::vector<int> ui_vect_idx; // index of ui
	std::vector<int> zi_vect_idx; // index of ui
	double Rxyz_ui; // score of the best contributor

	double Ixy_ui;
	double cplx;
	int Nxy_ui;
	short int connected;
	// Keeping track of jointCounts (discrete) and jointSpace(continuous) for KL divergence when
	//adding Us to the conditioning set.
	double mutInfo;  // mutual information without conditioning
	double cplx_noU;  // complexity without conditioning
	int Nxy;  // count of joint factors without NA

	EdgeSharedInfo():
		z_name_idx(-1),
		ui_vect_idx(std::vector<int>()),
		zi_vect_idx(std::vector<int>()),
		Rxyz_ui(0.0),

		Ixy_ui(0.0),
		cplx(0.0),
		Nxy_ui(-1),
		connected(-1),
		mutInfo(0.0),
		cplx_noU(0.0),
		Nxy(-1)
	{}

    // Remove knowledge about all contributing nodes.
    void reset() {
        z_name_idx = -1;
        zi_vect_idx.clear();
        ui_vect_idx.clear();
        Rxyz_ui = 0;
        connected = -1;
        Ixy_ui = mutInfo;
        cplx = cplx_noU;
        Nxy_ui = Nxy;
    }

    void setUndirected () {
        z_name_idx = -1;
        ui_vect_idx.clear();
        Rxyz_ui = 0;
        connected = 3;
        Ixy_ui = mutInfo;
        cplx = cplx_noU;
        Nxy_ui = Nxy;
    }
};

struct Node{
	std::string name;
	int level;
};

struct ExecutionTime{
	double startTimeInit;
	double startTimeIter;
	long double init;
	long double iter;
	long double initIter;
	long double ort;
	long double cut;
	long double ort_after_cut;
	long double total;
};

struct EdgeID{
	int i;
	int j;

    EdgeID() = default;
    EdgeID(int i, int j) : i(i), j(j) {}
};

struct Edge{
	// Edge is stored in Edge** edges
	// Status code (suppose edges[X][Y]):
	// 0: not connected;
	// 1: connected and undirected;
	// 2: connected directed X -> Y;
	// -2: connected directed X <- Y;
	// 6: connected bidirected X <-> Y;
	short int status;  // Current status.
	short int status_init;  // Status after initialization.
	short int status_prev;  // Status in the previous iteration.
	EdgeSharedInfo* shared_info;
};

// Structure for all the needed parameters (input plus state variables)
struct Environment {
	ExecutionTime execTime;
	int consistentPhase;
	// for gaussian case
	double** rho;
	double* standardDeviations;
	double* means;
	double** dataDouble;
	double ** pMatrix;
	int ** nSamples;
	int ** iterative_cuts;
	double* sampleWeights;
	std::vector<double> sampleWeightsVec;
	bool flag_sample_weights;

	bool testDistribution;
	int seed;
	uint nThreads;
	MemorySpace m;
	std::vector<int> steps;
	MemorySpace* memoryThreads;
    // Matrix to keep the number of edges for each eta and shuffle
	double** shuffleListNumEdges;
	double* c2terms;
	double** cterms;
	double** lookchoose;
	int* columnAsContinuous;
	int* columnAsGaussian;
	std::vector<int> cntVarVec;
	int* oneLineMatrix;

	std::string myVersion; // -i parameter

	std::string outDir; // -i parameter
	std::string inData; // -o parameter
	std::string cplxType; // -c parameter
	std::string blackbox_name;
	std::string edgeFile;
	std::string dataTypeFile;
	//std::string sampleWeightsFile;// -w parameter

	uint numNodes;
	uint numSamples;
	bool firstIterationDone;

	std::vector<EdgeID*> searchMoreAddress;
	std::vector<EdgeID*> noMoreAddress;
	int numSearchMore;
	int numNoMore;

	int typeOfData;
	int isAllGaussian;
	int atLeastTwoGaussian;
	int atLeastOneContinuous;
	int atLeastTwoDiscrete;

	// keep a trace of the number of edges for every state
	int phantomEdgenNum;

	Node* nodes;
	Edge** edges;
	std::vector<std::string>  vectorData;
	std::vector< std::vector <std::string> > data;
	int** dataNumeric;
	int** dataNumericIdx;
	int* allLevels;

	double** proportions;

	int** cut;

	double logEta;

	bool myTest;
	bool isDegeneracy; // -d parameter
	bool isVerbose; // -v parameter
	bool isLatent; // -l parameter
	bool isLatentOnlyOrientation; // -l parameter
	bool isNoInitEta; // -f parameter
	bool propag;
	bool isK23; // -k parameter
	bool isPropagation;

	int isTplReuse; // -r parameter
	int cplx;
	int halfVStructures;

	int numberShuffles; // -s parameter
	double confidenceThreshold; // -e parameter

	uint effN; // -n parameter
	int minN;
	//int nDg;
	int thresPc;

	int maxbins;
	int initbins;
	double* looklog;
	double* lookH;

	double* noiseVec;
};

#endif
