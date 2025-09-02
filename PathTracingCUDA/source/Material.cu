#include "../include/Material.h"

//#define DIPOLE
//#define REJECTION_SAMPLE
__managed__ unsigned int noIntersection = 0;
__managed__ unsigned int rejected = 0;
__managed__ unsigned int total = 0;
__managed__ unsigned int clampedPDFs = 0;
__managed__ unsigned int PDFs = 0;
__managed__ float radialSamplesSum = 0;
__managed__ float radialSamplesCount = 0;
__managed__ float expectedRadialAverage = 0;
__managed__ float uSum = 0;
__managed__ double  sssEnergySumR = 0.0;
__managed__ double  sssEnergySumG = 0.0;
__managed__ double  sssEnergySumB = 0.0;
__managed__ unsigned long long sssHitCount = 0;
__managed__ float dieSum = 0;
__managed__ unsigned long long h1 = 0;