#ifndef COMPUTEINFO_INTERFACE_H
#define COMPUTEINFO_INTERFACE_H
double* getAllInfoNEWThreads( int* ptrAllData, int* ptrAllLevels, int* ptrVarIdx, int nbrUi, int* ptrZiIdx, int nbrZi, int ziPos, int sampleSize, int sampleSizeEff, int modCplx, int k23, double*, MemorySpace*, int,MemorySpace*, double*, double**, bool);;
double* getAllInfoNEW( int* ptrAllData, int* ptrAllLevels, int* ptrVarIdx, int nbrUi, int* ptrZiIdx, int nbrZi, int ziPos, int sampleSize, int sampleSizeEff, int modCplx, int k23, double*, MemorySpace*, double*, double**, bool);
#endif
