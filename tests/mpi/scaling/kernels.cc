#include "IMSRG.hh"
#include "MPIContext.hh"
#include <cmath>
#include <iomanip>
int main(int argc, char** argv) {
  imsrg_mpi::Session session(argc,argv);
  const int emax = argc > 1 ? std::atoi(argv[1]) : 10;
  ModelSpace ms(emax,"O18","sdfp-shell");
  UnitTest ut(ms);
  imsrg_profile::checkpoint("start");
  bool passed = true;
  for (int j : {0,1,2}) {
    Operator x=ut.RandomOp(ms,0,0,0,2,-1);
    Operator y=ut.RandomOp(ms,j,0,0,2,1);
    auto z=Commutator::Commutator(x,y);
    imsrg_profile::checkpoint("warmup_J"+std::to_string(j));
    for(int repeat=0; repeat<3; ++repeat) {
#ifdef IMSRG_USE_MPI
      MPI_Barrier(MPI_COMM_WORLD);
#endif
      const double begin=omp_get_wtime();
      auto result=Commutator::Commutator(x,y);
      const double elapsed=omp_get_wtime()-begin;
      double slowest=elapsed;
#ifdef IMSRG_USE_MPI
      MPI_Allreduce(&elapsed,&slowest,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
#endif
      if(imsrg_mpi::root()) std::cout << "KERNEL J="<<j<<" repeat="<<repeat<<" seconds="<<std::setprecision(17)<<slowest<<" norm="<<result.Norm()<<std::endl;
      imsrg_profile::checkpoint("repeat_J"+std::to_string(j)+"_"+std::to_string(repeat));
    }
    // Untimed element-by-element check with distribution disabled, on every rank.
    const int rank=imsrg_mpi::rank(), size=imsrg_mpi::size();
    imsrg_mpi::rank_storage()=0; imsrg_mpi::size_storage()=1;
    auto reference=Commutator::Commutator(x,y);
    imsrg_mpi::rank_storage()=rank; imsrg_mpi::size_storage()=size;
    double error=std::abs(z.ZeroBody-reference.ZeroBody), scale=std::max(1.,std::abs(reference.ZeroBody));
    error=std::max(error,arma::abs(z.OneBody-reference.OneBody).max());
    scale=std::max(scale,arma::abs(reference.OneBody).max());
    for(const auto& kv:z.TwoBody.MatEl) {
      const auto& ref=reference.TwoBody.MatEl.at(kv.first);
      if(!ref.n_elem) continue;
      error=std::max(error,arma::abs(kv.second-ref).max());
      scale=std::max(scale,arma::abs(ref).max());
    }
    passed &= std::isfinite(error) && error<1e-11*scale;
    if(imsrg_mpi::root()) std::cout<<"CHECK J="<<j<<" error="<<error<<" scale="<<scale<<std::endl;
    imsrg_profile::checkpoint("check_J"+std::to_string(j));
  }
#ifdef IMSRG_USE_MPI
  int local=passed, global=0; MPI_Allreduce(&local,&global,1,MPI_INT,MPI_MIN,MPI_COMM_WORLD); passed=global;
#endif
  return passed?0:1;
}
