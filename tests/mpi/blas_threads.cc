// A single-threaded BLAS still needs thread-safe workspace allocation when
// several OpenMP threads call it concurrently (OpenBLAS USE_LOCKING=1).
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <omp.h>
extern "C" void dgemm_(const char*, const char*, const int*, const int*, const int*,
                       const double*, const double*, const int*, const double*,
                       const int*, const double*, double*, const int*);
int main() {
  int failures = 0;
  const int m=129, n=131, k=257;
  const double alpha=1, beta=0;
  const char normal='N';
#pragma omp parallel reduction(+:failures)
  {
    const int tid=omp_get_thread_num();
    std::vector<double> a(m*k), b(k*n), c(m*n), gold(m*n,0);
    for(int l=0;l<k;++l) for(int i=0;i<m;++i) a[i+m*l]=i%7+l%11+tid;
    for(int j=0;j<n;++j) for(int l=0;l<k;++l) b[l+k*j]=l%13+j%5+2*tid;
    for(int j=0;j<n;++j) for(int l=0;l<k;++l) for(int i=0;i<m;++i)
      gold[i+m*j]+=a[i+m*l]*b[l+k*j];
#pragma omp barrier
    for(int rep=0;rep<2000;++rep) {
      dgemm_(&normal,&normal,&m,&n,&k,&alpha,a.data(),&m,b.data(),&k,&beta,c.data(),&m);
      // These small integer-valued sums are exactly representable in double.
      if(c!=gold) ++failures;
    }
  }
  std::cout << "BLAS concurrent-call failures: " << failures << "\n";
  return failures ? 1 : 0;
}
