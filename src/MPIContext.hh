#ifndef IMSRG_MPI_CONTEXT_HH
#define IMSRG_MPI_CONTEXT_HH

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>
#include <utility>
#include <numeric>
#include "PerformanceProfile.hh"
#ifdef IMSRG_USE_MPI
#include <mpi.h>
#endif

// Replicated operators with exclusive ownership of channel calculations.
// Initialize on the main thread before entering any OpenMP region. All
// collective calls must be made outside OpenMP regions, in the same order.
namespace imsrg_mpi {
inline int& rank_storage() { static int value = 0; return value; }
inline int& size_storage() { static int value = 1; return value; }
inline int rank() { return rank_storage(); }
inline int size() { return size_storage(); }
inline bool root() { return rank() == 0; }
inline std::string scratch_suffix() {
  return size() > 1 ? "_r" + std::to_string(rank()) + "_" : "";
}
inline int owner(std::size_t item) { return item % size(); }
inline bool owns(std::size_t item) { return rank() == owner(item); }

class NullBuffer : public std::streambuf {
  int overflow(int c) override { return traits_type::not_eof(c); }
};
class Session {
  bool owned = false;
  NullBuffer null_buffer;
  std::streambuf* cout_buffer = nullptr;
public:
  Session(int& argc, char**& argv) {
#ifdef IMSRG_USE_MPI
    int initialized = 0, provided = 0;
    MPI_Initialized(&initialized);
    if (!initialized) {
      MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
      owned = true;
    } else MPI_Query_thread(&provided);
    if (provided < MPI_THREAD_FUNNELED) MPI_Abort(MPI_COMM_WORLD, 1);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_storage());
    MPI_Comm_size(MPI_COMM_WORLD, &size_storage());
    if (!root()) cout_buffer = std::cout.rdbuf(&null_buffer);
#endif
  }
  ~Session() {
    if (cout_buffer) std::cout.rdbuf(cout_buffer);
#ifdef IMSRG_USE_MPI
    if (owned) MPI_Finalize();
#endif
  }
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
};

inline void broadcast(double* data, std::size_t count, int source) {
#ifdef IMSRG_USE_MPI
  if (size() == 1) return;
  const double started = imsrg_profile::enabled() ? imsrg_profile::now() : 0;
  const std::size_t payload = count;
  // Bounded counts work with MPI implementations using 32-bit int counts.
  while (count) {
    const int chunk = static_cast<int>(std::min<std::size_t>(count, 1UL << 26));
    MPI_Bcast(data, chunk, MPI_DOUBLE, source, MPI_COMM_WORLD);
    data += chunk;
    count -= chunk;
  }
  if (imsrg_profile::enabled()) {
    imsrg_profile::add("broadcast_seconds", imsrg_profile::now() - started);
    imsrg_profile::add("broadcast_bytes", double(payload) * sizeof(double));
    imsrg_profile::add("broadcast_calls", 1);
  }
#endif
}
template<class Matrix> inline void broadcast(Matrix& matrix, int source) {
  broadcast(matrix.memptr(), matrix.n_elem, source);
}
// Stable largest-first scheduling avoids tying all expensive tensor blocks
// with the same bra channel to a single rank. Plans depend only on shapes,
// so cache ownership remains stable as matrix values evolve.
struct WorkPlan {
  std::vector<int> owners;
  WorkPlan() = default;
  explicit WorkPlan(const std::vector<std::size_t>& weights) : owners(weights.size(), 0) {
    if (size() == 1) return;
    std::vector<std::size_t> order(weights.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
      return weights[a] > weights[b];
    });
    std::vector<std::size_t> load(size(), 0);
    for (auto i : order) {
      int r = std::min_element(load.begin(), load.end()) - load.begin();
      owners[i] = r;
      load[r] += weights[i];
    }
  }
  bool owns(std::size_t i) const { return owners[i] == rank(); }
};
template<class Map> inline WorkPlan make_plan(const Map& matrices) {
  std::vector<std::size_t> weights;
  for (const auto& entry : matrices) weights.push_back(entry.second.n_elem);
  return WorkPlan(weights);
}

using Block = std::pair<double*, std::size_t>;
// Group blocks by source rank to replace many small collectives with at most
// one (chunked) broadcast per rank. Copy, never sum, to preserve arithmetic.
inline void sync_blocks(const std::vector<Block>& blocks, const WorkPlan& plan) {
  if (size() == 1) return;
  const double started = imsrg_profile::enabled() ? imsrg_profile::now() : 0;
  const double communication_before = imsrg_profile::enabled() ? imsrg_profile::metrics()["broadcast_seconds"] : 0;
  std::vector<std::size_t> counts(size(), 0);
  for (std::size_t i = 0; i < blocks.size(); ++i)
    counts[plan.owners[i]] += blocks[i].second;
  std::vector<double> buffer(*std::max_element(counts.begin(), counts.end()));
  for (int source = 0; source < size(); ++source) {
    if (counts[source] == 0) continue;
    std::size_t offset = 0;
    if (rank() == source) {
      for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (plan.owners[i] != source || blocks[i].second == 0) continue;
        std::copy_n(blocks[i].first, blocks[i].second, buffer.data() + offset);
        offset += blocks[i].second;
      }
    }
    broadcast(buffer.data(), counts[source], source);
    if (rank() != source) {
      for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (plan.owners[i] != source || blocks[i].second == 0) continue;
        std::copy_n(buffer.data() + offset, blocks[i].second, blocks[i].first);
        offset += blocks[i].second;
      }
    }
  }
  if (imsrg_profile::enabled()) {
    const double elapsed = imsrg_profile::now() - started;
    imsrg_profile::add("block_sync_seconds", elapsed);
    imsrg_profile::add("block_pack_copy_seconds", elapsed - (imsrg_profile::metrics()["broadcast_seconds"] - communication_before));
  }
}
template<class Map> inline void sync_map(Map& matrices, const WorkPlan& plan) {
  if (size() == 1) return;
  std::vector<Block> blocks;
  for (auto& entry : matrices) blocks.emplace_back(entry.second.memptr(), entry.second.n_elem);
  sync_blocks(blocks, plan);
}
template<class TwoBody> inline void sync_channels(TwoBody& two_body) {
  if (size() == 1) return;
  WorkPlan plan;
  for (const auto& entry : two_body.MatEl) plan.owners.push_back(owner(entry.first[0]));
  sync_map(two_body.MatEl, plan);
}
template<class Matrices> inline void sync_indexed(Matrices& matrices) {
  if (size() == 1) return;
  WorkPlan plan;
  std::vector<Block> blocks;
  for (auto& matrix : matrices) {
    plan.owners.push_back(owner(blocks.size()));
    blocks.emplace_back(matrix.memptr(), matrix.n_elem);
  }
  sync_blocks(blocks, plan);
}
// Synchronize an independently constructed initial operator before the flow.
template<class Operator> inline void sync_operator(Operator& op) {
  if (size() == 1) return;
  broadcast(&op.ZeroBody, 1, 0);
  broadcast(op.OneBody, 0);
  for (auto& entry : op.TwoBody.MatEl) broadcast(entry.second, 0);
}

}
#endif
