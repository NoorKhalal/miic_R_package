#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#include <algorithm>
#include <iostream>

#include "utilities.h"

#include "computeInfo.h"

using namespace std;
//functions utilities for dynamic programming optimization


///////////////////////////////////////////////////////////////////////////////////////////

//#define _MY_DEBUG_NEW 1
//#define _MY_DEBUG_NEW_UI_2 1
//#define _MY_DEBUG_NEW_UI 1
//#define _MY_DEBUG_NEW_OPTFUN 1
//#define DEBUG_JOINT 1
//#define _MY_DEBUG_NEW_UI_NML 1
//#define _MY_DEBUG_NEW_3p 1
//#define _MY_DEBUG_NEW_3p_f 1
//#define DEBUG 1



/////////////////////////////////////////////////////////////////////////////
//INPUT:
//memory_cuts: vector length n with recorded recursvely best cuts,
//possible values 0...n :
//0->stops (one bins); -k -> stops (two bin ([0 k-1][k ..];k->continute [.. k-1][k ...])
//
//OUTPUT:
//r : number cuts
//cut: vector with cuts point-> [0 cut[0]][cut[0]+1 cut[1]]...[cut[r-2] cut[r-1]]

int reconstruction_cut_coarse(vector<int>& memory_cuts, vector<int>& memory_cuts2, int np, int n,int *cut)
{
		int ncuts=0;
		int l,s;

		if(memory_cuts[np-1]==0){

			cut[0] = n-1;
			return 1;
		}

		l=memory_cuts[np-1];

		while(l>0){
			ncuts++;
			l=memory_cuts[l-1];
		}
		if(l<0) ncuts++;

		cut[ncuts]=n-1;//conventional last cut

		l=ncuts-1;
		s=memory_cuts[np-1];
		cut[l]=memory_cuts2[np-1];//truly last cut
		l--;
		while(s>0 && l>=0){
			cut[l]=memory_cuts2[s-1];
			s=memory_cuts[s-1];
			l--;
		}

		return ncuts+1; //number of levels (r)

}

/////////////////////////////////////////////////////////////////
//update datafactors of a variable from the cut positions vector <cut>
//INPUT:
//d: index of variable in datafactors
//varidx: index of variable in sortidx

void update_datafactors(int **sortidx, int varidx, int **datafactors,int d, int n, int **cut)
{
	int j,sjj,uu=0;

	for(j=0;j<=n-1;j++){
			sjj=sortidx[varidx][j];
			if(j>cut[d][uu]) uu++;
			datafactors[d][sjj]=uu;
	}
	return;
}



// /////////////////////////////////////////////////////////////////
// //update datafactors d

// void update_datafactors(int **sortidx, int **data, int varidx, int **datafactors,int d, int n, int **cut)
// {
// 	int j,sjj=sortidx[varidx][0];uu=0,rj=0;
// 	datafactors[d][sjj]=0;
// 	for(j=1;j<=n-1;j++){
// 		if(data[varidx][sjj]!=data[varidx][sortidx[varidx][j]]) rj++;;//account for repeated values
// 		sjj=sortidx[varidx][j];
// 		if(rj>cut[d][uu]) uu++;
// 		datafactors[d][sjj]=uu;
// 	}

// }


/////////////////////////////////////////////////////////////////////////////////////////

//INPUT:
//datarank, datafactors
//take 0 1 as x y and 2..<Mui>+1 for {u} indexes
//ignore index <dui> in the u

//OUTPUT
//return joint datafactors ui and uiy uix uixy, with number of levels ruiyx[0,1,2,3]
//ruiyx -> 0:u,1:uy,2:ux,3:uyx

void jointfactors_uiyx(int **datafactors, int dui, int n, int Mui, int *r, int **uiyxfactors, int *ruiyx)
{

		int df,Pbin_ui;
		int *datauix=(int*)calloc(n,sizeof(int));
		int *datauiy=(int*)calloc(n,sizeof(int));
		int *datauiyx=(int*)calloc(n,sizeof(int));
		int *dataui=(int*)calloc(n,sizeof(int));

		int j,jj,l;
		int uu=0;
		bool tooManyLevels = false;

		/////////////////
		//from single datafactors to joint datafactors ui; (ui,y);(ui,x);(ui,y,x)
		int nbrLevelsJoint = 1;
		for(jj=0;jj<Mui+2 && !tooManyLevels;jj++){
			nbrLevelsJoint *= r[jj];
			if(nbrLevelsJoint>8*n){
				tooManyLevels = true;
				// cout << "tooManyLevels: " << nbrLevelsJoint << endl;
			}
		}

		// if(!tooManyLevels)
		// 	cout << "NOT " << nbrLevelsJoint << endl;

		int *vecZeroOnesui;
		int *vecZeroOnesuiy;
		int *vecZeroOnesuix;
		int *vecZeroOnesuiyx;
		//
		int * orderSample_ux;
		int * orderSample_uyx;
		//declaration
		if(!tooManyLevels){
			vecZeroOnesui=(int*)calloc(nbrLevelsJoint+1,sizeof(int));
			vecZeroOnesuiy=(int*)calloc(nbrLevelsJoint+1,sizeof(int));
			vecZeroOnesuix=(int*)calloc(nbrLevelsJoint+1,sizeof(int));
			vecZeroOnesuiyx=(int*)calloc(nbrLevelsJoint+1,sizeof(int));
		}
		else{
			orderSample_ux = new int[n];
			orderSample_uyx = new int[n];
		}

		for(jj=0;jj<=n-1;jj++){

			datauiyx[jj]=datafactors[0][jj];//yx
			datauiyx[jj]+=datafactors[1][jj]*r[0];
			datauix[jj]=datafactors[0][jj];//x
			datauiy[jj]=datafactors[1][jj];//y
			dataui[jj]=0;

			Pbin_ui=1;
			for (l=Mui+1;l>=2;l--){
				if(l!=dui){
					df=datafactors[l][jj]*Pbin_ui;
					datauiyx[jj]+=df*r[1]*r[0];
					datauix[jj]+=df*r[0];
					datauiy[jj]+=df*r[1];


					dataui[jj]+=df;
					Pbin_ui*=r[l];
				}
			}
			//init

			if(!tooManyLevels){
				vecZeroOnesui[dataui[jj]] = 1;
				vecZeroOnesuiy[datauiy[jj]] = 1;
				vecZeroOnesuix[datauix[jj]] = 1;
				vecZeroOnesuiyx[datauiyx[jj]] = 1;
			}
			else{
				orderSample_ux[jj] = jj;
				orderSample_uyx[jj] = jj;
			}
		}

		if(!tooManyLevels){

			// create the vector for storing the position
			int pos = 0;
			int pos1 = 0;
			int pos2 = 0;
			int pos3 = 0;
			for(jj=0;jj<=nbrLevelsJoint;jj++){

				if(vecZeroOnesui[jj] == 1){
					vecZeroOnesui[jj] = pos;
					pos += 1;
				}

				if(vecZeroOnesuiy[jj] == 1){
					vecZeroOnesuiy[jj] = pos1;
					pos1 += 1;
				}

				if(vecZeroOnesuix[jj] == 1){
					vecZeroOnesuix[jj] = pos2;
					pos2 += 1;
				}

				if(vecZeroOnesuiyx[jj] == 1){
					vecZeroOnesuiyx[jj] = pos3;
					pos3 += 1;
				}

			}

			ruiyx[0]=pos;//ui
			ruiyx[1]=pos1;//uiy
			ruiyx[2]=pos2;//uix
			ruiyx[3]=pos3;//uiyx


			for(j=0;j<=n-1;j++){
				uiyxfactors[0][j]=vecZeroOnesui[dataui[j]];//ui
				uiyxfactors[1][j]=vecZeroOnesuiy[datauiy[j]];;//uiy
				uiyxfactors[2][j]=vecZeroOnesuix[datauix[j]];;//uix
				uiyxfactors[3][j]=vecZeroOnesuiyx[datauiyx[j]];;//uiyx

			}
		} else {


			sort2arraysConfidence(n,datauix,orderSample_ux);
			sort2arraysConfidence(n,datauiyx,orderSample_uyx);

			//joint datafactors without gaps
			//compute term constant H(y,Ui) and H(Ui)

			int ix,iyx,iix,iiyx;

			ruiyx[0]=0;//ui
			ruiyx[1]=0;//uiy
			ruiyx[2]=0;//uix
			ruiyx[3]=0;//uiyx

			ix=orderSample_ux[0];
			iyx=orderSample_uyx[0];
			uiyxfactors[0][ix]=0;//ui
			uiyxfactors[1][iyx]=0;//uiy
			uiyxfactors[2][ix]=0;//uix
			uiyxfactors[3][iyx]=0;//uiyx

			for(j=0;j < n-1;j++){
				iix=ix;
				iiyx=iyx;
				ix=orderSample_ux[j+1];
				iyx=orderSample_uyx[j+1];

				if(dataui[ix]>dataui[iix]){
					ruiyx[0]++;
				}
				uiyxfactors[0][ix]=ruiyx[0];//ui

				if(datauiy[iyx]>datauiy[iiyx]){
					ruiyx[1]++;
				}
				uiyxfactors[1][iyx]=ruiyx[1];//uiy

				if(datauix[ix]>datauix[iix]){
					ruiyx[2]++;
				}
				uiyxfactors[2][ix]=ruiyx[2];//uix

				if(datauiyx[iyx]>datauiyx[iiyx]){
					ruiyx[3]++;
				}
				uiyxfactors[3][iyx]=ruiyx[3];//uiyx
			}


			//number joint levels
			ruiyx[0]++;//ui
			ruiyx[1]++;//uiy
			ruiyx[2]++;//uix
			ruiyx[3]++;//uiyx
			delete [] orderSample_ux;
			delete [] orderSample_uyx;

		}



		#if _MY_DEBUG_NEW_UI
			printf("j,dataui[j],datauiy[j]\n");
			for(j=0;j<=n-1;j++){
				printf("%d-> %d %d\n",j,dataui[j],datauiy[j]);
			}
		#endif

		#if _MY_DEBUG_NEW_UI_2
			printf("j,uiyxfactors[0][j],uiyxfactors[1][j],uiyxfactors[2][j],uiyxfactors[3][j]\n");
			for(j=0;j<=n-1;j++){
				printf("%d-> %d %d %d %d\n",j,uiyxfactors[0][j],uiyxfactors[1][j],uiyxfactors[2][j],uiyxfactors[3][j]);
			}
		#endif


		free(datauix);
		free(datauiy);
		free(datauiyx);
		free(dataui);
		if(!tooManyLevels){
			free(vecZeroOnesui);
			free(vecZeroOnesuiy);
			free(vecZeroOnesuix);
			free(vecZeroOnesuiyx);
		}

		return;
}
////////////////////////////////////////////////////////////////////////////////////////////////

















/////////////////////////////////////////////////////////////////////////////////////////

//INPUT:
//datarank, datafactors, cut
// ptrVar Index_x, Index_y, Index_u1,..Index_uMui

//OUTPUT
//return joint datafactors ui and uiy uix uixy, with number of levels ruiyx[0,1,2,3]

void jointfactors_uyx(int **datafactors, int* ptrVar, int n, int Mui, int *r, int **uyxfactors,int *ruyx)
{


		int df,Pbin_ui;
		int *datauix=(int*)calloc(n,sizeof(int));
		int *datauiy=(int*)calloc(n,sizeof(int));
		int *datauiyx=(int*)calloc(n,sizeof(int));
		int *dataui=(int*)calloc(n,sizeof(int));

		int j,jj,l;
		int uu=0;
		bool tooManyLevels = false;

		/////////////////
		//from single datafactors to joint datafactors ui; (ui,y);(ui,x);(ui,y,x)
		int nbrLevelsJoint = 1;
		for(jj=0; jj < Mui+ 2 && !tooManyLevels;jj++){
			nbrLevelsJoint *= r[ptrVar[jj]];
			if(nbrLevelsJoint>8*n)
				tooManyLevels = true;
		}

		//decl
		int *vecZeroOnesui;
		int *vecZeroOnesuiy;
		int *vecZeroOnesuix;
		int *vecZeroOnesuiyx;
		//
		int * orderSample_ux;
		int * orderSample_uyx;

		if(!tooManyLevels){
			vecZeroOnesui=(int*)calloc(nbrLevelsJoint,sizeof(int));
			vecZeroOnesuiy=(int*)calloc(nbrLevelsJoint,sizeof(int));
			vecZeroOnesuix=(int*)calloc(nbrLevelsJoint,sizeof(int));
			vecZeroOnesuiyx=(int*)calloc(nbrLevelsJoint,sizeof(int));
		}
		else{
			orderSample_ux = new int[n];
			orderSample_uyx = new int[n];
		}

		for(jj=0;jj<=n-1;jj++){

			datauiyx[jj]=datafactors[ptrVar[0]][jj];//yx
			datauiyx[jj]+=datafactors[ptrVar[1]][jj]*r[ptrVar[0]];
			datauix[jj]=datafactors[ptrVar[0]][jj];//x
			datauiy[jj]=datafactors[ptrVar[1]][jj];//y
			dataui[jj]=0;

			Pbin_ui=1;
			for (l=Mui+1;l>=2;l--){
					df=datafactors[ptrVar[l]][jj]*Pbin_ui;
					datauiyx[jj]+=df*r[ptrVar[1]]*r[ptrVar[0]];
					datauix[jj]+=df*r[ptrVar[0]];
					datauiy[jj]+=df*r[ptrVar[1]];

					dataui[jj]+=df;
					Pbin_ui*=r[ptrVar[l]];
			}
			//init

			if(!tooManyLevels){
				vecZeroOnesui[dataui[jj]] = 1;
				vecZeroOnesuiy[datauiy[jj]] = 1;
				vecZeroOnesuix[datauix[jj]] = 1;
				vecZeroOnesuiyx[datauiyx[jj]] = 1;
			}
			else{
				orderSample_ux[jj] = jj;
				orderSample_uyx[jj] = jj;
			}

		}

		if(!tooManyLevels){
			// create the vector for storing the position
			int pos = 0;
			int pos1 = 0;
			int pos2 = 0;
			int pos3 = 0;
			for(jj=0;jj<=nbrLevelsJoint;jj++){

				if(vecZeroOnesui[jj] == 1){
					vecZeroOnesui[jj] = pos;
					pos += 1;
				}

				if(vecZeroOnesuiy[jj] == 1){
					vecZeroOnesuiy[jj] = pos1;
					pos1 += 1;
				}

				if(vecZeroOnesuix[jj] == 1){
					vecZeroOnesuix[jj] = pos2;
					pos2 += 1;
				}

				if(vecZeroOnesuiyx[jj] == 1){
					vecZeroOnesuiyx[jj] = pos3;
					pos3 += 1;
				}

			}


			ruyx[0]=pos;//ui
			ruyx[1]=pos1;//uiy
			ruyx[2]=pos2;//uix
			ruyx[3]=pos3;//uiyx


			for(j=0;j<=n-1;j++){
				uyxfactors[0][j]=vecZeroOnesui[dataui[j]];//ui
				uyxfactors[1][j]=vecZeroOnesuiy[datauiy[j]];;//uiy
				uyxfactors[2][j]=vecZeroOnesuix[datauix[j]];;//uix
				uyxfactors[3][j]=vecZeroOnesuiyx[datauiyx[j]];;//uiyx
			}
		} else {

			sort2arraysConfidence(n,datauix,orderSample_ux);
			sort2arraysConfidence(n,datauiyx,orderSample_uyx);

			//joint datafactors without gaps
			//compute term constant H(y,Ui) and H(Ui)

			int ix,iyx,iix,iiyx;

			ruyx[0]=0;//ui
			ruyx[1]=0;//uiy
			ruyx[2]=0;//uix
			ruyx[3]=0;//uiyx

			ix=orderSample_ux[0];
			iyx=orderSample_uyx[0];
			uyxfactors[0][ix]=0;//ui
			uyxfactors[1][iyx]=0;//uiy
			uyxfactors[2][ix]=0;//uix
			uyxfactors[3][iyx]=0;//uiyx

			for(j=0;j<n-1;j++){
				iix=ix;
				iiyx=iyx;
				ix=orderSample_ux[j+1];
				iyx=orderSample_uyx[j+1];

				if(dataui[ix]>dataui[iix]){
					ruyx[0]++;
				}
				uyxfactors[0][ix]=ruyx[0];//ui

				if(datauiy[iyx]>datauiy[iiyx]){
					ruyx[1]++;
				}
				uyxfactors[1][iyx]=ruyx[1];//uiy

				if(datauix[ix]>datauix[iix]){
					ruyx[2]++;
				}
				uyxfactors[2][ix]=ruyx[2];//uix

				if(datauiyx[iyx]>datauiyx[iiyx]){
					ruyx[3]++;
				}
				uyxfactors[3][iyx]=ruyx[3];//uiyx
			}


			//number joint levels
			ruyx[0]++;//ui
			ruyx[1]++;//uiy
			ruyx[2]++;//uix
			ruyx[3]++;//uiyx

			delete [] orderSample_ux;
			delete [] orderSample_uyx;

		}

		#if _MY_DEBUG_NEW_UI
			printf("j,dataui[j],datauiy[j]\n");
			for(j=0;j<=n-1;j++){
				printf("%d-> %d %d\n",j,dataui[j],datauiy[j]);
			}
		#endif


		#if _MY_DEBUG_NEW_UI_2
			printf("j,uyxfactors[0][j],uyxfactors[1][j],uyxfactors[2][j],uyxfactors[3][j]\n");
			for(j=0;j<=n-1;j++){
				printf("%d-> %d %d %d %d\n",j,uyxfactors[0][j],uyxfactors[1][j],uyxfactors[2][j],uyxfactors[3][j]);
			}
		#endif

		// free memory

		free(datauix);
		free(datauiy);
		free(datauiyx);
		free(dataui);
		if(!tooManyLevels){
			free(vecZeroOnesui);
			free(vecZeroOnesuiy);
			free(vecZeroOnesuix);
			free(vecZeroOnesuiyx);
		}

		return;
}




//////////////////////////////////////////////////////////////////

//INPUT:
//datarank, datafactors, cut

//OUTPUT
//return joint datafactors ui , with number of levels rui
//entropy term Hui
void jointfactors_u(int **datafactors,int *ptrIdx,int n, int Mui, int *r, int *ufactors,int *ru)
{

		//from cuts to joint datafactors

		int jj;
		int i,j,l;

		if(Mui==1){
			for(jj=0;jj<=n-1;jj++){
				ufactors[jj]=datafactors[ptrIdx[0]][jj];
			}
			*ru=r[ptrIdx[0]];
			return;
		}
		/////////////////
		//update joint datafactors (with gaps) ui and (ui,y)

		int df,Pbin_ui;
		int *datau=(int*)calloc(n,sizeof(int));

		bool tooManyLevels = false;

		int nbrLevelsJoint = 1;
		for(l=0;l<Mui && !tooManyLevels;l++){
			nbrLevelsJoint *= r[ptrIdx[l]];
			if(nbrLevelsJoint>8*n)
				tooManyLevels = true;
		}

		//decl
		int *vecZeroOnesui;
		int * orderSample_u;
		if(!tooManyLevels){
			vecZeroOnesui=(int*)calloc(nbrLevelsJoint+1,sizeof(int));
		}
		else{
			orderSample_u= new int[n];
		}

		for(jj=0;jj<=n-1;jj++){

			datau[jj]=0;

			Pbin_ui=1;
			for (l=Mui-1;l>=0;l--){
					df=datafactors[ptrIdx[l]][jj]*Pbin_ui;
					datau[jj]+=df;
					Pbin_ui*=r[ptrIdx[l]];
			}

			//init
			if(!tooManyLevels){
				vecZeroOnesui[datau[jj]] = 1;
			}
			else{
				orderSample_u[jj] = jj;
			}
		}

		if(!tooManyLevels){
			// create the vector for storing the position
			int pos = 0;

			for(jj=0;jj<=nbrLevelsJoint;jj++){

				if(vecZeroOnesui[jj] == 1){
					vecZeroOnesui[jj] = pos;
					pos += 1;
				}

			}

			*ru=pos;//ui

			for(j=0;j<=n-1;j++){
				ufactors[j]=vecZeroOnesui[datau[j]];//ui
			}

		 } else {

			sort2arraysConfidence(n,datau,orderSample_u);

			int ix, iix;
			ix=orderSample_u[0];
			*ru=0;
			ufactors[ix]=0;//ui



			for(j=0;j < n-1;j++){
				iix=ix;
				ix=orderSample_u[j+1];

				if(datau[ix]>datau[iix]){
					*ru=*ru+1;
				}
				ufactors[ix]=*ru;//ui

				// cout << ufactors[j] << " ";
			}


			*ru=*ru+1;

			// cout << "\n" << *ru << "\n";
			delete [] orderSample_u;
		}

		#if DEBUG_JOINT
			printf("\nj -> datau[j]\n");
			for(j=0;j<=n-1;j++){
				printf("%d -> %d \n",j,datau[j]);
			}
			fflush(stdout);
		#endif


		#if DEBUG_JOINT
			printf("j,orderSample[j+1],sampleKey[j+1],datau[orderSample[j+1]]\n");
			for(j=0;j<=n-1;j++){
				printf("%d: %d %d, %d \n",j,orderSample[j+1],sampleKey[j+1],datau[orderSample[j+1]]);
			}
			fflush(stdout);
		#endif

		//joint datafactors without gaps
		//compute term constant H(y,Ui) and H(Ui)

		#if DEBUG
			printf("Hu=%lf\n",*Hu);
		#endif

		//number joint levels ui and uiy
		// *ru=(*ru)+1;

		#if DEBUG_JOINT
			printf("j,uiyfactors[0][i] (ru=%d)\n",*ru);
			for(j=0;j<=n-1;j++){
				printf("%d-> %d\n",j,ufactors[j]);
			}
			fflush(stdout);
		#endif

		free(datau);
		if(!tooManyLevels){
			free(vecZeroOnesui);
		}

		return;
}


///////////////////////////////////////////////

//ruyx -> 0:u,1:uy,2:ux,3:uyx

double* computeMIcond_knml(int **uiyxfactors, int *ruiyx, int *r,int n, double* c2terms, double* looklog){

	double *I=(double*)calloc(2,sizeof(double));

	int j,u,ux,uy,uyx;

	double Hux=0,Huy=0,Huyx=0,Hu=0,SC=0;

	int *nui=(int*)calloc(ruiyx[0],sizeof(int));
	int *nuiy=(int*)calloc(ruiyx[1],sizeof(int));
	int *nuix=(int*)calloc(ruiyx[2],sizeof(int));
	int *nuiyx=(int*)calloc(ruiyx[3],sizeof(int));


	for(j=0;j<n;j++){
		nui[uiyxfactors[0][j]]++;
		nuiy[uiyxfactors[1][j]]++;
		nuix[uiyxfactors[2][j]]++;
		nuiyx[uiyxfactors[3][j]]++;
	}

	for(u=0;u<ruiyx[0];u++){
		if(nui[u]>0) Hu-=nui[u]*looklog[nui[u]];
		SC-=0.5*computeLogC(nui[u],r[0],looklog,c2terms);
		SC-=0.5*computeLogC(nui[u],r[1],looklog,c2terms);
	}

	for(uy=0;uy<ruiyx[1];uy++){
		if(nuiy[uy]>0) Huy-=nuiy[uy]*looklog[nuiy[uy]];
		SC+=0.5*computeLogC(nuiy[uy],r[0],looklog,c2terms);
	}

	for(ux=0;ux<ruiyx[2];ux++){
		if(nuix[ux]>0) Hux-=nuix[ux]*looklog[nuix[ux]];
		SC+=0.5*computeLogC(nuix[ux],r[1],looklog,c2terms);
	}

	for(uyx=0;uyx<ruiyx[3];uyx++){
		if(nuiyx[uyx]>0) Huyx-=nuiyx[uyx]*looklog[nuiyx[uyx]];
	}

	I[0]=(Hux+Huy-Hu-Huyx)/n;

	I[1]=I[0]-SC/n;


	free(nui);
	free(nuiy);
	free(nuix);
	free(nuiyx);

	return I;

}


///////////////////////////////////////////////

//rux -> 0:x,1;u,2:ux

double* computeMI_knml(int* xfactors,int* ufactors,int* uxfactors,int* rux,int n, double* c2terms,double* looklog, int flag){

	double *I=(double*)calloc(2,sizeof(double));

	int j,x,u,ux;

	double Hux=0,Hu=0,Hx=0,SC=0;

	int *nx=(int*)calloc(rux[0],sizeof(int));
	int *nu=(int*)calloc(rux[1],sizeof(int));
	int *nux=(int*)calloc(rux[2],sizeof(int));

	for(j=0;j<n;j++){
		nx[xfactors[j]]++;
		nu[ufactors[j]]++;
		nux[uxfactors[j]]++;
	}

	for(x=0;x<rux[0];x++){
		if(nx[x]>0) Hx-=nx[x]*looklog[nx[x]];
		if(flag==0 || flag==2) SC+=computeLogC(nx[x],rux[1],looklog,c2terms);
	}
	for(u=0;u<rux[1];u++){
		if(nu[u]>0) Hu-=nu[u]*looklog[nu[u]];
		if(flag==0 || flag==1) SC+=computeLogC(nu[u],rux[0],looklog,c2terms);
	}

	for(ux=0;ux<rux[2];ux++){
		if(nux[ux]>0) Hux-=nux[ux]*looklog[nux[ux]];
	}

	if(flag==0) SC-=computeLogC(n,rux[0],looklog,c2terms);
	if(flag==0) SC-=computeLogC(n,rux[1],looklog,c2terms);


	I[0]=looklog[n]+(Hu+Hx-Hux)/n;

	if(flag == 0) I[1]=I[0]-0.5*SC/n;
	else I[1]=I[0]-SC/n;

	free(nx);
	free(nu);
	free(nux);

	return I;
}


////////////////////////////////////////////////////////////////////////////////////////////

double* computeMIcond_kmdl(int **uiyxfactors, int *ruiyx, int *r,int n, double* looklog)
{

	double *I=(double*)calloc(2,sizeof(double));

	int j,l,u,ux,uy,uyx;

	double Hux=0,Huy=0,Huyx=0,Hu=0,SC=0;

	int *nui=(int*)calloc(ruiyx[0],sizeof(int));
	int *nuiy=(int*)calloc(ruiyx[1],sizeof(int));
	int *nuix=(int*)calloc(ruiyx[2],sizeof(int));
	int *nuiyx=(int*)calloc(ruiyx[3],sizeof(int));


	for(j=0;j<n;j++){
		nui[uiyxfactors[0][j]]++;
		nuiy[uiyxfactors[1][j]]++;
		nuix[uiyxfactors[2][j]]++;
		nuiyx[uiyxfactors[3][j]]++;
	}

	for(u=0;u<ruiyx[0];u++){
		if(nui[u]>0) Hu-=nui[u]*looklog[nui[u]];
	}
	for(uy=0;uy<ruiyx[1];uy++){
		if(nuiy[uy]>0) Huy-=nuiy[uy]*looklog[nuiy[uy]];
	}
	for(ux=0;ux<ruiyx[2];ux++){
		if(nuix[ux]>0) Hux-=nuix[ux]*looklog[nuix[ux]];
	}
	for(uyx=0;uyx<ruiyx[3];uyx++){
		if(nuiyx[uyx]>0) Huyx-=nuiyx[uyx]*looklog[nuiyx[uyx]];
	}

	SC=0.5*(r[0]-1)*(r[1]-1)*looklog[n];
	SC*=ruiyx[0];

	I[0]=(Hux+Huy-Hu-Huyx)/n;

	I[1]=I[0]-SC/n;

	free(nui);
	free(nuiy);
	free(nuix);
	free(nuiyx);

	return I;

}

///////////////////////////////////////////////

//rux -> 0:x,1;u,2:ux

double* computeMI_kmdl(int* xfactors,int* ufactors,int* uxfactors,int* rux,int n,double* looklog, int flag){

	double *I=(double*)calloc(2,sizeof(double));

	int j,x,u,ux;

	double Hux=0,Hu=0,Hx=0,SC=0;

	int *nx=(int*)calloc(rux[0],sizeof(int));
	int *nu=(int*)calloc(rux[1],sizeof(int));
	int *nux=(int*)calloc(rux[2],sizeof(int));

	for(j=0;j<n;j++){
		nx[xfactors[j]]++;
		nu[ufactors[j]]++;
		nux[uxfactors[j]]++;
	}

	for(x=0;x<rux[0];x++){
		if(nx[x]>0) Hx-=nx[x]*looklog[nx[x]];
	}
	for(u=0;u<rux[1];u++){
		if(nu[u]>0) Hu-=nu[u]*looklog[nu[u]];
	}

	for(ux=0;ux<rux[2];ux++){
		if(nux[ux]>0) Hux-=nux[ux]*looklog[nux[ux]];
	}

	SC=0.5*looklog[n];
	if(flag==0 || flag==1) SC*=(rux[0]-1);
	if(flag==0 || flag==2) SC*=(rux[1]-1);

	I[0]=looklog[n]+(Hu+Hx-Hux)/n;

	I[1]=I[0]-SC/n;

	free(nx);
	free(nu);
	free(nux);

	return I;

}