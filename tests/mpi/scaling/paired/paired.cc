#include "IMSRG.hh"
#include "MPIContext.hh"
#include <cmath>
#include <iomanip>
#include <cstdlib>

bool check(const Operator& z, const Operator& reference, double& error) {
  error=std::abs(z.ZeroBody-reference.ZeroBody);
  double scale=std::max(1.,std::abs(reference.ZeroBody));
  bool finite=std::isfinite(z.ZeroBody) && z.OneBody.is_finite();
  error=std::max(error,arma::abs(z.OneBody-reference.OneBody).max());
  scale=std::max(scale,arma::abs(reference.OneBody).max());
  for (const auto& kv:z.TwoBody.MatEl) {
    const auto& ref=reference.TwoBody.MatEl.at(kv.first);
    if (!ref.n_elem) continue;
    finite &= kv.second.is_finite();
    error=std::max(error,arma::abs(kv.second-ref).max());
    scale=std::max(scale,arma::abs(ref).max());
  }
  return finite && std::isfinite(error) && error<=1e-11*scale;
}
int main(int argc,char** argv) {
  imsrg_mpi::Session session(argc,argv);
  const int emax=argc>1?std::atoi(argv[1]):10;
  const std::string selected=argc>2?argv[2]:"012";
  ModelSpace ms(emax,"O18","sdfp-shell"); UnitTest ut(ms);
  bool passed=true;
  imsrg_profile::checkpoint("start");
  for (int j:{0,1,2}) {
    auto x=ut.RandomOp(ms,0,0,0,2,-1);
    auto y=ut.RandomOp(ms,j,0,0,2,1);
    if (selected.find(char('0'+j))==std::string::npos) continue;
    setenv("IMSRG_MPI_FULL_INTERMEDIATES","1",1);
    auto reference=Commutator::Commutator(x,y);
    setenv("IMSRG_MPI_FULL_INTERMEDIATES","0",1);
    auto warm=Commutator::Commutator(x,y);
    double error=0; passed &= check(warm,reference,error);
    imsrg_profile::checkpoint("warmup_J"+std::to_string(j));
    for (int repeat=0; repeat<3; ++repeat) for (int k=0;k<2;++k) {
      const bool compact=(k+repeat)%2;
      setenv("IMSRG_MPI_FULL_INTERMEDIATES",compact?"0":"1",1);
      MPI_Barrier(MPI_COMM_WORLD);
      const double start=omp_get_wtime();
      auto z=Commutator::Commutator(x,y);
      double elapsed=omp_get_wtime()-start, maximum=0;
      MPI_Allreduce(&elapsed,&maximum,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
      passed &= check(z,reference,error);
      double max_error=0;
      MPI_Allreduce(&error,&max_error,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
      const std::string label="J"+std::to_string(j)+(compact?"_compact_":"_full_")+std::to_string(repeat);
      if (imsrg_mpi::root()) std::cout<<"PAIR J="<<j<<" mode="<<(compact?"compact":"full")<<" repeat="<<repeat<<" seconds="<<std::setprecision(17)<<maximum<<" max_error="<<max_error<<std::endl;
      imsrg_profile::checkpoint(label);
    }
  }
  int ok=passed,all=0; MPI_Allreduce(&ok,&all,1,MPI_INT,MPI_MIN,MPI_COMM_WORLD);
  if(imsrg_mpi::root())std::cout<<"PAIRED_CHECK "<<(all?"PASS":"FAIL")<<std::endl;
  return all?0:1;
}
