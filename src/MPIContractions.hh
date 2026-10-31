#ifndef IMSRG_MPI_CONTRACTIONS_HH
#define IMSRG_MPI_CONTRACTIONS_HH
#include "MPIContext.hh"
#include "TwoBodyME.hh"
#include <map>

namespace imsrg_mpi {
inline bool compact_intermediates() {
  const char* full = std::getenv("IMSRG_MPI_FULL_INTERMEDIATES");
  return size() > 1 && !(full && std::string(full) == "1");
}

// Batch updates to the same result operator. All contributing two-body kernels
// use the same owner for each output block; none reads another rank's partial Z.
// Intermediate arrays still synchronize normally. Pointer scoping also isolates
// a nested commutator or a standalone kernel from the outer result operator.
inline const void*& deferred_result() { static const void* target=nullptr; return target; }
inline void sync_result_channels(TwoBodyME& result) {
  if (deferred_result()!=&result) sync_channels(result);
}
inline void sync_result_map(TwoBodyME& result, const WorkPlan& plan) {
  if (deferred_result()!=&result) sync_map(result.MatEl,plan);
}
class ResultBatch {
  bool active;
  const void* previous;
public:
  ResultBatch(TwoBodyME& result, bool enable) : active(enable), previous(deferred_result()) {
    if (active) deferred_result()=&result;
  }
  ~ResultBatch() { if (active) deferred_result()=previous; }
  void finish(TwoBodyME& result, bool tensor) {
    if (!active) return;
    active=false;
    deferred_result()=previous;
    if (tensor) sync_map(result.MatEl,make_plan(result.MatEl));
    else sync_channels(result);
  }
  ResultBatch(const ResultBatch&)=delete;
  ResultBatch& operator=(const ResultBatch&)=delete;
};

// A one-body contraction reads only <c i|M|c j>. Keep the full local
// intermediate for its owned two-body output, but exchange just shared-orbit
// matrix elements for the subsequent one-body loop. No reductions/reordering.
inline const std::vector<std::vector<std::size_t>>& contraction_indices(const TwoBodyME& matrices) {
  // Exact structural cache key, not a ModelSpace pointer: addresses can be
  // recycled and model spaces can change between core/valence calculations.
  std::vector<std::size_t> key;
  auto& ms = *matrices.modelspace;
  key.push_back(ms.GetNumberTwoBodyChannels());
  for (std::size_t ch=0; ch<ms.GetNumberTwoBodyChannels(); ++ch) {
    auto& tbc=ms.GetTwoBodyChannel(ch);
    key.push_back(tbc.GetNumberKets());
    for (int k=0; k<tbc.GetNumberKets(); ++k) {
      const auto& ket=tbc.GetKet(k);
      key.push_back(ket.p); key.push_back(ket.q);
    }
  }
  for (const auto& kv:matrices.MatEl) {
    key.push_back(kv.first[0]); key.push_back(kv.first[1]);
    key.push_back(kv.second.n_rows); key.push_back(kv.second.n_cols);
  }
  using Selection=std::vector<std::vector<std::size_t>>;
  static std::map<std::vector<std::size_t>,Selection> cache;
  auto found=cache.find(key);
  if (found!=cache.end()) return found->second;
  if (cache.size()>=8) cache.clear();
  Selection selection;
  for (const auto& kv:matrices.MatEl) {
    auto& bra=ms.GetTwoBodyChannel(kv.first[0]);
    auto& ket=ms.GetTwoBodyChannel(kv.first[1]);
    std::map<std::size_t,std::vector<std::size_t>> rows;
    for (std::size_t i=0; i<kv.second.n_rows; ++i) {
      const auto& b=bra.GetKet(i);
      rows[b.p].push_back(i);
      if (b.p!=b.q) rows[b.q].push_back(i);
    }
    std::vector<std::size_t> indices;
    for (std::size_t j=0; j<kv.second.n_cols; ++j) {
      const auto& k=ket.GetKet(j);
      std::vector<std::size_t> selected=rows[k.p];
      if (k.p!=k.q) selected.insert(selected.end(),rows[k.q].begin(),rows[k.q].end());
      std::sort(selected.begin(),selected.end());
      selected.erase(std::unique(selected.begin(),selected.end()),selected.end());
      for (auto i:selected) indices.push_back(i+j*kv.second.n_rows);
    }
    selection.push_back(std::move(indices));
  }
  return cache.emplace(std::move(key),std::move(selection)).first->second;
}

inline void sync_one_body_slices(TwoBodyME& pp, TwoBodyME& hh, const WorkPlan& plan) {
  if (size()==1) return;
  const double started=imsrg_profile::enabled()?imsrg_profile::now():0;
  const auto& selection=contraction_indices(pp);
  std::vector<std::vector<double>> values(selection.size());
  std::vector<Block> blocks;
  std::size_t i=0, full=0, compact=0;
  for (auto& kv:pp.MatEl) {
    auto& h=hh.MatEl.at(kv.first);
    auto& data=values[i]; data.resize(2*selection[i].size());
    full+=2*kv.second.n_elem; compact+=data.size();
    if (plan.owns(i)) for (std::size_t k=0; k<selection[i].size(); ++k) {
      data[2*k]=kv.second[selection[i][k]];
      data[2*k+1]=h[selection[i][k]];
    }
    blocks.emplace_back(data.data(),data.size()); ++i;
  }
  sync_blocks(blocks,plan);
  i=0;
  for (auto& kv:pp.MatEl) {
    auto& h=hh.MatEl.at(kv.first);
    if (!plan.owns(i)) for (std::size_t k=0; k<selection[i].size(); ++k) {
      kv.second[selection[i][k]]=values[i][2*k];
      h[selection[i][k]]=values[i][2*k+1];
    }
    ++i;
  }
  if (imsrg_profile::enabled()) {
    imsrg_profile::add("intermediate_full_elements",full);
    imsrg_profile::add("intermediate_slice_elements",compact);
    imsrg_profile::add("intermediate_slice_seconds",imsrg_profile::now()-started);
  }
}
}
#endif
