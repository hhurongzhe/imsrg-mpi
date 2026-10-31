#ifndef IMSRG_PERFORMANCE_PROFILE_HH
#define IMSRG_PERFORMANCE_PROFILE_HH
#include "IMSRGProfiler.hh"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>
#include <stdexcept>

// Optional observational profiling. No MPI collectives or changes to arithmetic.
// Call only from the main thread. One file per process, flushed at phase boundaries.
namespace imsrg_profile {
inline bool enabled() { static bool value = std::getenv("IMSRG_PROFILE_DIR") != nullptr; return value; }
inline double now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline std::map<std::string,double>& metrics() { static std::map<std::string,double> values; return values; }
inline void add(const std::string& name, double value) { if (enabled()) metrics()[name] += value; }
inline void checkpoint(const std::string& phase) {
  if (!enabled()) return;
  static double previous = now();
  static unsigned sequence = 0;
  const double end = now();
  const char* rank = std::getenv("OMPI_COMM_WORLD_RANK");
  const std::string path = std::string(std::getenv("IMSRG_PROFILE_DIR")) + "/rank-" + (rank ? rank : "0") + ".tsv";
  std::ofstream out(path, sequence == 0 ? std::ios::trunc : std::ios::app);
  if (!out) throw std::runtime_error("Cannot write performance profile: " + path);
  out << std::setprecision(17);
  out << sequence << '\t' << phase << "\tphase_seconds\t" << end-previous << '\n';
  for (const auto& kv : IMSRGProfiler::timer)
    out << sequence << '\t' << phase << "\ttimer:" << kv.first << '\t' << kv.second << '\n';
  for (const auto& kv : metrics())
    out << sequence << '\t' << phase << "\tmetric:" << kv.first << '\t' << kv.second << '\n';
  out.flush();
  ++sequence;
  previous = now(); // Exclude profile serialization from the next phase.
}
}
#endif
