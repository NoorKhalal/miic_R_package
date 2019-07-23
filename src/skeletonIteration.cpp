#include <iostream>
#include <string>
#include <math.h>
#include "structure.h"
#include "computeEnsInformation.h"
#include "skeletonInitialization.h"
#include "skeletonIteration.h"
#include "utilities.h"
#include <queue>
#include <vector>
#include <algorithm>
#include <Rcpp.h>
#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;

// #include <unistd.h>

bool SortFunctionNoMore(const XJAddress* a, const XJAddress* b, const Environment& environment) {
	 return environment.edges[a->i][a->j].edgeStructure->Ixy_ui > environment.edges[b->i][b->j].edgeStructure->Ixy_ui;
}

class sorterNoMore {
	  Environment &environment;
		public:
	  sorterNoMore(Environment& env) : environment(env) {}
	  bool operator()(XJAddress const* o1, XJAddress const* o2) const {
			return SortFunctionNoMore(o1, o2, environment );
	  }
};


bool areallUiGaussian(Environment& environment, std::vector<int> uis, int size){
	for(int i = 0; i < size; i++){
		if(environment.columnAsGaussian[uis[i]] != 1)
			return false;
	}

	return true;
}

bool areallUiDiscrete(Environment& environment, std::vector<int> uis, int size){
	for(int i = 0; i < size; i++){
		if(environment.columnAsContinuous[uis[i]] != 0)
			return false;
	}

	return true;
}

/*
 * Sort the ranks of the function
 */

bool SortFunction1(const XJAddress* a, const XJAddress* b, const Environment& environment) {
	 return environment.edges[a->i][a->j].edgeStructure->Rxyz_ui > environment.edges[b->i][b->j].edgeStructure->Rxyz_ui;
}

class sorter1 {
	  Environment &environment;
		public:
	  sorter1(Environment& env) : environment(env) {}
	  bool operator()(XJAddress const* o1, XJAddress const* o2) const {
			return SortFunction1(o1, o2, environment );
	  }
};

template<bool consistentPhase=false, bool isLatent=false>
void searchAndSetZi(Environment& environment, const int posX, const int posY){
    /**
     * Search candidate nodes for the separation set of X and Y
     */
	int numZiPos = 0;
    for(int c = 0; c < environment.numNodes; c++){
        if (c == posX || c == posY)
            continue;
        if (!isLatent && !environment.edges[posX][c].areNeighboursAfterIteration && !environment.edges[posY][c].areNeighboursAfterIteration)
            continue;
        if (consistentPhase && !is_consistent(environment, posX, posY, c))
            continue;
        environment.edges[posX][posY].edgeStructure->zi_vect_idx.push_back(c);
        numZiPos++;
    }

	if(environment.isVerbose)
		cout << "The number of neighbours is: " << numZiPos << endl;
}

void searchAndSetZi(Environment& environment, const int posX, const int posY){
    if (environment.consistentPhase)
        if (environment.isLatent)
            return searchAndSetZi<true, true>(environment, posX, posY);
        else
            return searchAndSetZi<true, false>(environment, posX, posY);
    else
        if (environment.isLatent)
            return searchAndSetZi<false, true>(environment, posX, posY);
        else
            return searchAndSetZi<false, false>(environment, posX, posY);
}

bool firstStepIteration(Environment& environment){
	//cout << "End of couples: " << endl;

	if(environment.isVerbose)
		cout << "Number of searchMore: " << environment.countSearchMore << endl;

	environment.numSearchMore = environment.countSearchMore;
	environment.numNoMore= 0;

	//set the diagonal of the adj matrix to 0 Redundant, already done in setEnvironment
	for(uint i = 0; i < environment.numNodes; i++){
		environment.edges[i][i].isConnected = 0;
	}

	environment.searchMoreAddress.clear();

	// create and fill the searchMoreAddress struct, that keep track of i and j positions of searchMore Edges
	for(uint i = 0; i < environment.numNodes - 1; i++){
		for(uint j = i + 1; j < environment.numNodes; j++){
			if(environment.edges[i][j].isConnected){
				XJAddress* s = new XJAddress();
				s->i=i;
				s->j=j;
				environment.searchMoreAddress.push_back(s);
			}
		}
	}

	int threadnum = 0;
	bool interrupt = false;
	int prg_numSearchMore = -1;
	cout << "First round of conditional independences :\n";
	// if there exists serachMore edges
	if( environment.countSearchMore > 0 ) {
		if( environment.isVerbose == true )
			cout << "\n# -> searchMore edges, to get zi and noMore...\n" ;
		//// Define the pairs among which the candidate {zi} should be searched

		//// For all non-phantom pairs xy in G, find zi that are neighbours of x or y
		//// If no z is found, the edge goes to "noMore", otherwise "searchMore"

		for(int i = 0; i < environment.numSearchMore; i++){
			int posX = environment.searchMoreAddress[i]->i;
			int posY = environment.searchMoreAddress[i]->j;
			if( environment.isVerbose){ cout << "\n# --------------------\n# ----> EDGE: " << environment.nodes[posX].name << "--" << environment.nodes[posY].name << "\n# --------------------" ; }

			//// Find all zi, such that xzi or yzi is not a phantom
			// search in the row of the edge matrix

			searchAndSetZi(environment, posX, posY);
		}

		if(environment.isVerbose){
			cout << "SEARCH OF BEST Z: ";
		}

		environment.execTime.startTimeInit = get_wall_time();
		#ifdef _OPENMP
		#pragma omp parallel for shared(interrupt) firstprivate(threadnum) schedule(dynamic)
		#endif
		for(int i = 0; i < environment.numSearchMore; i++){

			if (interrupt) {
				continue; // will continue until out of for loop
			}
			#ifdef _OPENMP
				threadnum = omp_get_thread_num(); 
			#endif
			if (threadnum == 0){
				if(checkInterrupt(i/environment.nThreads%2 == 0)) {
					interrupt = true;
				}
			}
			int posX = environment.searchMoreAddress[i]->i;
			int posY = environment.searchMoreAddress[i]->j;
			if(environment.isVerbose)cout << "##  " << "XY: " << environment.nodes[posX].name << " " << environment.nodes[posY].name << "\n\n";
			if(environment.edges[posX][posY].edgeStructure->zi_vect_idx.size() > 0 ){
				//// Search for new contributing node and its rank
				if(environment.isAllGaussian == 0){
					SearchForNewContributingNodeAndItsRank(environment, posX, posY, environment.memoryThreads[threadnum]);
				} else {
					SearchForNewContributingNodeAndItsRankGaussian(environment, posX, posY, environment.memoryThreads[threadnum]);
				}
			}
			if(threadnum==0) prg_numSearchMore = printProgress(1.0*i/(environment.numSearchMore-environment.nThreads),
															   environment.execTime.startTimeInit,
															   environment.outDir, prg_numSearchMore);
		}

		if(interrupt) return false;

		for(int i = 0; i < environment.numSearchMore; i++){
			int posX = environment.searchMoreAddress[i]->i;
			int posY = environment.searchMoreAddress[i]->j;
			if(environment.edges[posX][posY].edgeStructure->z_name_idx != -1)
			{
				if(environment.isVerbose)
				{ cout << "## ------!!--> Update the edge element in 'searchMore': " <<
					environment.nodes[environment.edges[posX][posY].edgeStructure->zi_vect_idx[environment.edges[posX][posY].edgeStructure->z_name_idx]].name << " is a good zi candidate\n";}
			} else {
				if(environment.isVerbose)
				{ cout << "## ------!!--> Remove the edge element from searchMore.\n## ------!!--> Add edge to 'noMore' (no good zi candidate)\n";}
				//// Put this edge element to "noMore" and update the vector of status
				environment.noMoreAddress.push_back(environment.searchMoreAddress[i]);
				environment.numNoMore++;

				//// Remove the element from searchMore
				environment.searchMoreAddress.erase(environment.searchMoreAddress.begin() + i);
				environment.numSearchMore--;
				i--;

				//update the status
				environment.edges[posX][posY].edgeStructure->status = 3;
			}
			if(environment.isVerbose)
				cout << "\n";
		}

		if(checkInterrupt()) {
			return false;
		}
		// sort the ranks
		std::sort(environment.searchMoreAddress.begin(), environment.searchMoreAddress.end(), sorter1(environment));

	}
	environment.firstIterationDone = true;
	return(true);
}



bool skeletonIteration(Environment& environment){

	int iIteration_count = 0;
	int max = 0;

	if(environment.isVerbose)
		cout << "Number of numSearchMore: " << environment.numSearchMore << endl;


	cout << "\nSkeleton iteration :\n";
	environment.execTime.startTimeIter = get_wall_time();
	int start_numSearchMore = environment.numSearchMore;

	int prg_numSearchMore = -1;

	while( environment.numSearchMore > 0 )
	{
		if(checkInterrupt()){
			return(false);
		}
		iIteration_count++;
		if(environment.isVerbose) { cout << "\n# Iteration " << iIteration_count << "\n"; }
		// cout << "\n# Iteration " << iIteration_count << "\n";
		//// Get the first edge
		int posX = environment.searchMoreAddress[max]->i;
		int posY = environment.searchMoreAddress[max]->j;

		if(environment.isVerbose)
			cout << "Pos x : " << posX << " , pos y: " << posY << endl << flush;

		EdgeStructure* topEdgeElt = environment.edges[posX][posY].edgeStructure;

		if( environment.isVerbose ) cout << "# Before adding new zi to {ui}: " ; //displayEdge(topEdgeElt)

		//// Keep the previous z.name for this edge
		string accepted_z_name = environment.nodes[topEdgeElt->z_name_idx].name;

		//// Reinit ui.vect, z.name, zi.vect, z.name.idx
		if(environment.isVerbose) { cout << "# DO: Add new zi to {ui}: " << topEdgeElt->z_name_idx << endl;}

		//// Ui
		topEdgeElt->ui_vect_idx.push_back(topEdgeElt->zi_vect_idx[topEdgeElt->z_name_idx]);
		//// Zi
		// ttopEdgeElt->z.name = NA
	   	topEdgeElt->zi_vect_idx[topEdgeElt->z_name_idx] = -1;
	   	topEdgeElt->z_name_idx = -1;
		//// Nxyui
		topEdgeElt->Nxy_ui = topEdgeElt->Nxyz_ui;
		topEdgeElt->Nxyz_ui = -1;

		double* v =NULL;
		if(environment.columnAsContinuous[posX] == 0 && environment.columnAsContinuous[posY] == 0 &&
			areallUiDiscrete(environment, environment.edges[posX][posY].edgeStructure->ui_vect_idx, environment.edges[posX][posY].edgeStructure->ui_vect_idx.size())){

			v = computeEnsInformationNew(environment, &environment.edges[posX][posY].edgeStructure->ui_vect_idx[0], environment.edges[posX][posY].edgeStructure->ui_vect_idx.size(),
					NULL, 0, -1,  posX, posY, environment.cplx, environment.m);

			topEdgeElt->Ixy_ui = v[1];
			topEdgeElt->Nxy_ui = v[0];
			topEdgeElt->cplx = v[2];
			free(v);
		} else if(environment.columnAsGaussian[posX] == 1 && 
				  environment.columnAsGaussian[posY] == 1 &&
				  areallUiGaussian(environment, environment.edges[posX][posY].edgeStructure->ui_vect_idx,
				  				   environment.edges[posX][posY].edgeStructure->ui_vect_idx.size())) {

			int s = environment.edges[posX][posY].edgeStructure->ui_vect_idx.size();
			v = corrMutInfo(environment, environment.dataDouble, &environment.edges[posX][posY].edgeStructure->ui_vect_idx[0], s, NULL, 0, posX, posY, -2);
			int N = environment.nSamples[posX][posY];
			environment.edges[posX][posY].edgeStructure->Nxy_ui = N;
			topEdgeElt->cplx = 0.5 * (environment.edges[posX][posY].edgeStructure->ui_vect_idx.size() + 2) * log(N);
			topEdgeElt->Ixy_ui = v[0];
			delete [] v;

		} else {
			v = computeEnsInformationContinuous(environment, &environment.edges[posX][posY].edgeStructure->ui_vect_idx[0], 
												environment.edges[posX][posY].edgeStructure->ui_vect_idx.size(),
												NULL, 0, -1,  posX, posY, environment.cplx,environment.m);
			topEdgeElt->Nxy_ui = v[0];
			topEdgeElt->Ixy_ui = v[1];
			topEdgeElt->cplx = v[2];

			delete [] v;
		}


		double topEdgeElt_kxy_ui = topEdgeElt->cplx;

	 	if(environment.isDegeneracy)
			topEdgeElt_kxy_ui = topEdgeElt->cplx + (topEdgeElt->ui_vect_idx.size()*log(3));

		//// *****
		int leftEdges = environment.numSearchMore + environment.numNoMore;
		//// *****

		if(environment.isVerbose)
		{
			//cout << "\n# After adding new zi to {ui}:" << displayEdge(topEdgeElt);
			cout << "# --> nbrEdges L = " << leftEdges << "\n";
			cout << "# --> nbrProp P = " << environment.numNodes << "\n\n";

			//cout << "\n# After adding new zi to {ui}:" << displayEdge(topEdgeElt);
			cout << "topEdgeElt->Ixy_ui "<< topEdgeElt->Ixy_ui << "\n";
			cout << "topEdgeElt_kxy_ui "<< topEdgeElt_kxy_ui << "\n";
			cout << "environment.logEta "<< environment.logEta <<"\n";
			cout << "IsPhantom? " << (topEdgeElt->Ixy_ui - topEdgeElt_kxy_ui - environment.logEta <= 0 )<<endl;
		}
		if( topEdgeElt->Ixy_ui - topEdgeElt_kxy_ui - environment.logEta <= 0 )
		{
			if(environment.isVerbose) { cout << "# PHANTOM" << environment.nodes[posX].name << "," << environment.nodes[posY].name << "\n"; }

			//// Move this edge from the list searchMore to phantom
			environment.searchMoreAddress.erase(environment.searchMoreAddress.begin() + max);
			environment.numSearchMore--;

			// set the connection to 0 on the adj matrix
			environment.edges[posX][posY].isConnected = 0;
			environment.edges[posY][posX].isConnected = 0;
			//// Save the phantom status
			topEdgeElt->status = 1;
		} else {
			//// Reinit Rxyz_ui
			topEdgeElt->Rxyz_ui = environment.thresPc;

			if(environment.isVerbose) { cout << "# Do SearchForNewContributingNodeAndItsRank\n" ; }

			if( topEdgeElt->zi_vect_idx.size() > 0 )
			{
				//// Search for a new contributing node and its rank
				// cout << environment.nodes[posX].name << "-" << environment.nodes[posY].name  << " " << endl;

				if(environment.isAllGaussian == 0){
					SearchForNewContributingNodeAndItsRank(environment, posX, posY, environment.m);
				} else {
					SearchForNewContributingNodeAndItsRankGaussian(environment, posX, posY, environment.m);
				}
			}

			 if(environment.isVerbose){
				if(environment.edges[posX][posY].edgeStructure->z_name_idx == -1)
					cout << "# See topEdgeElt[['z.name']]: NA\n" ;
				else
					cout << "# See topEdgeElt[['z.name']]: " << environment.nodes[topEdgeElt->zi_vect_idx[topEdgeElt->z_name_idx]].name << "\n" ;
			 }
			//// Update the information about the edge
			if( topEdgeElt->z_name_idx != -1)
			{
				if(environment.isVerbose) { cout << "# Do update myAllEdges$searchMore\n" ; }
				//myGv$allEdges[["searchMore"]][[topEdgeElt[["key"]]]] = topEdgeElt

			} else {
				if(environment.isVerbose) { cout << "# Do update myAllEdges$noMore\n" ; }
					//// Move this edge from the list searchMore to noMore
					environment.noMoreAddress.push_back(environment.searchMoreAddress[max]);
					environment.numNoMore++;
					environment.searchMoreAddress.erase(environment.searchMoreAddress.begin() + max);
					environment.numSearchMore--;
					//// Update the status of the edge
					topEdgeElt->status = 3;
			}
		}

		//// Sort all pairs xy with a contributing node z in decreasing order of their ranks, R(xy;z| )
		if(environment.isVerbose) {cout <<"# Do Sort all pairs by Rxyz_ui\n";}

		max = 0;
		for(int i = 0; i < environment.numSearchMore; i++){
			if(environment.edges[environment.searchMoreAddress[i]->i][environment.searchMoreAddress[i]->j].edgeStructure->Rxyz_ui >
				environment.edges[environment.searchMoreAddress[max]->i][environment.searchMoreAddress[max]->j].edgeStructure->Rxyz_ui)
			max = i;
		}
		// cout << 1.0*(start_numSearchMore - environment.numSearchMore)/(start_numSearchMore-1) << "\t" << prg_numSearchMore << "\n" << flush;
		prg_numSearchMore = printProgress(1.0*(start_numSearchMore - environment.numSearchMore)/(start_numSearchMore),
					  					  environment.execTime.startTimeIter, environment.outDir, prg_numSearchMore);
	}
	cout << "\n";
	std::sort(environment.noMoreAddress.begin(), environment.noMoreAddress.end(), sorterNoMore(environment));
	return(true);
}

vector<int> bfs(const Environment& environment, int start, int end, const vector<int>& excludes)
{
    /**
     * Return the shortest path between two nodes.
     *
     * @param environment.
     * @param int start, end Starting, ending nodes.
     * @param excludes Nodes to be excluded from the path, default to be an empty vector.
     * @return Path as a vector<int> of node indices.
     */
    uint numNodes = environment.numNodes;
    vector<int> visited(numNodes, 0);
    for (auto& node : excludes) {
        if (node == start || node == end)
            return vector<int>();
        visited[node] = 1;
    }

    queue<pair<int, vector<int> > > bfs_queue;
	vector<int> vect;
    vect.push_back(start); 
    bfs_queue.push(make_pair(start, vect));
    while (!bfs_queue.empty()) {
        auto& p = bfs_queue.front();
        visited[p.first] = 1;
        for(uint i=0; i<numNodes; i++) {
            if (!visited[i] && environment.edges[p.first][i].areNeighboursAfterIteration) {
                vector<int> new_path(p.second);
                new_path.push_back(i);
                if (i == end)
                    return new_path;
                else
                    bfs_queue.push(make_pair(i, new_path));
            }
        }
        bfs_queue.pop();
    }

    return vector<int>();
}

bool is_consistent(const Environment& environment, int x, int y, int z) {
    /**
     * Check if node z lies on either path between node x and node y.
     *
     * @param environment.
     * @param x, y, z Indices of nodes.
     * @return bool.
     */
    if (x==z || y==z)
        return false;

    vector<int> x2z = bfs(environment, x, z);
    if (x2z.size() > 0)
        x2z.pop_back();  // remove z from path (not to be excluded in the following step)
    if (!x2z.empty() && !bfs(environment, y, z, x2z).empty())  // exclude nodes in path x2z
        return true;

    vector<int> y2z = bfs(environment, y, z);
    if (y2z.size() > 0)
        y2z.pop_back();
    if (!y2z.empty() && !bfs(environment, x, z, y2z).empty())
        return true;

    return false;
}

bool is_consistent(const Environment& environment, int x, int y, const vector<int>& vect_z) {
    for (auto& z : vect_z) {
        if (!is_consistent(environment, x, y, z))
            return false;
    }
    return true;
}
