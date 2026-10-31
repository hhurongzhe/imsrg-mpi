#include "IMSRG.hh"
#include "MPIContext.hh"
#include <cmath>
#include <iomanip>
#include <fstream>
#include <array>
#include <vector>

// Compare every element against the same kernels with MPI disabled, on every
// rank. Fractional occupations exercise ensemble-normal-ordered VS-IMSRG.
int main(int argc, char** argv) {
  imsrg_mpi::Session session(argc, argv);
  int emax = argc > 1 ? std::atoi(argv[1]) : 2;
  ModelSpace ms(emax, "O18", "sd-shell");
  UnitTest ut(ms);
  bool passed = true;
  std::vector<std::array<int,3>> cases;
  for (int j : {0, 1, 2}) for (int hx : {-1, 1}) cases.push_back({j,hx,1});
  cases.push_back({0,-1,-1}); // BCH_Product also commutes two anti-Hermitian scalars.
  for (const auto& test : cases) {
    const int j=test[0], hermitian=test[1], hy=test[2];
    Operator x = ut.RandomOp(ms, 0, 0, 0, 2, hermitian);
    Operator y = ut.RandomOp(ms, j, 0, 0, 2, hy);
    // Run distributed first, including cache warm-up.
    const auto z = Commutator::Commutator(x, y);
    if (argc > 2 && imsrg_mpi::root()) {
      std::ofstream out(std::string(argv[2])+"_J"+std::to_string(j)+"_h"+std::to_string(hermitian)+(hy<0?"_y-1":"")+".bin", std::ios::binary);
      out.write(reinterpret_cast<const char*>(&z.ZeroBody), sizeof(double));
      out.write(reinterpret_cast<const char*>(z.OneBody.memptr()), sizeof(double)*z.OneBody.n_elem);
      for (const auto& entry : z.TwoBody.MatEl)
        out.write(reinterpret_cast<const char*>(entry.second.memptr()), sizeof(double)*entry.second.n_elem);
      if (!out) return 2;
    }
    const int rank = imsrg_mpi::rank(), size = imsrg_mpi::size();
    imsrg_mpi::rank_storage() = 0;
    imsrg_mpi::size_storage() = 1;
    const auto reference = Commutator::Commutator(x, y);
    imsrg_mpi::rank_storage() = rank;
    imsrg_mpi::size_storage() = size;
    double error = std::abs(z.ZeroBody - reference.ZeroBody);
    double scale = std::max(1., std::abs(reference.ZeroBody));
    error = std::max(error, arma::abs(z.OneBody-reference.OneBody).max());
    scale = std::max(scale, arma::abs(reference.OneBody).max());
    for (const auto& entry : z.TwoBody.MatEl) {
      const auto& ref = reference.TwoBody.MatEl.at(entry.first);
      if (ref.n_elem == 0) continue;
      error = std::max(error, arma::abs(entry.second-ref).max());
      scale = std::max(scale, arma::abs(ref).max());
    }
    passed &= std::isfinite(error) && error < 1e-11 * scale;
    if (imsrg_mpi::root())
      std::cout << "MPI_CHECK J=" << j << " hX=" << hermitian
                << " hY=" << hy << " max_error=" << std::setprecision(17) << error
                << " scale=" << scale << "\n";
    // Repeat with warm caches.
    const double begin = omp_get_wtime();
    auto repeat = Commutator::Commutator(x, y);
    if (imsrg_mpi::root()) std::cout << "MPI_TIMING J=" << j << " hX=" << hermitian << " seconds=" << (omp_get_wtime()-begin) << "\n";
    passed &= (repeat-z).Norm() < 1e-10 * std::max(1.,z.Norm());
  }
#ifdef IMSRG_USE_MPI
  int ok = passed, all_ok = 0;
  MPI_Allreduce(&ok, &all_ok, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
  passed = all_ok;
#endif
  return passed ? 0 : 1;
}
