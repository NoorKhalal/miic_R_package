#include "environment.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <algorithm>

#include "compute_info.h"
#include "utilities.h"

using Rcpp::as;
using std::string;
using std::vector;
using namespace miic::utility;
using namespace miic::computation;

#define MAGNITUDE_TIES 0.00005f

namespace miic {
namespace structure {

Environment::Environment(
    const Rcpp::List& input_data, const Rcpp::List& arg_list)
    : data_numeric(as<vector<vector<int>>>(input_data["factor"])),
      n_samples(data_numeric.size()),
      n_nodes(data_numeric[0].size()),
      data_double(as<vector<vector<double>>>(input_data["double"])),
      data_numeric_idx(as<vector<vector<int>>>(input_data["order"])),
      is_continuous(as<vector<int>>(arg_list["is_continuous"])),
      levels(as<vector<int>>(arg_list["levels"])),
      n_eff(as<int>(arg_list["n_eff"])),
      orientation_phase(as<bool>(arg_list["orientation"])),
      propagation(as<bool>(arg_list["propagation"])),
      max_iteration(as<int>(arg_list["max_iteration"])),
      test_mar(as<bool>(arg_list["test_mar"])),
      n_shuffles(as<int>(arg_list["n_shuffles"])),
      conf_threshold(as<double>(arg_list["conf_threshold"])),
      sample_weights(as<vector<double>>(arg_list["sample_weights"])),
      is_k23(as<bool>(arg_list["is_k23"])),
      degenerate(as<bool>(arg_list["degenerate"])),
      no_init_eta(as<bool>(arg_list["no_init_eta"])),
      half_v_structure(as<int>(arg_list["half_v_structure"])),
      maxbins(as<int>(arg_list["max_bins"])),
      initbins(std::min(30, int(0.5 + cbrt(n_samples)))),
      n_threads(as<int>(arg_list["n_threads"])),
      verbose(as<bool>(arg_list["verbose"])) {
  auto var_names = as<vector<string>>(arg_list["var_names"]);
  std::transform(var_names.begin(), var_names.end(), std::back_inserter(nodes),
      [](string name) { return Node(name); });

  auto consistent_flag = as<std::string>(arg_list["consistent"]);
  if (consistent_flag.compare("orientation") == 0)
    consistent = 1;
  else if (consistent_flag.compare("skeleton") == 0)
    consistent = 2;

  auto latent_flag = as<std::string>(arg_list["latent"]);
  if (latent_flag.compare("yes") == 0)
    latent = true;
  else if (latent_flag.compare("orientation") == 0)
    latent_orientation = true;

  if (as<string>(arg_list["cplx"]).compare("mdl") == 0) cplx = 0;

  if (n_eff == -1 || n_eff > n_samples)
    n_eff = n_samples;

  if (sample_weights.empty()) {
    double uniform_weight{1};
    if (n_eff != n_samples)
      uniform_weight = static_cast<double>(n_eff) / n_samples;
    sample_weights.resize(n_samples, uniform_weight);
  }

#ifdef _OPENMP
  if (n_threads < 0) n_threads = omp_get_num_procs();
  omp_set_num_threads(n_threads);
#endif
  // create the 1000 entries to store c2 values
  c2terms = new double[n_samples + 1];
  for (int i = 0; i < n_samples + 1; i++) {
    c2terms[i] = -1;
  }
  // for mixed, create the log(j) lookup table with j=0..n_samples;
  looklog = new double[n_samples + 2];
  looklog[0] = 0.0;
  for (int i = 1; i < n_samples + 2; i++) {
    looklog[i] = log(1.0 * i);
  }

  lookH = new double[n_samples + 2];
  lookH[0] = 0.0;
  for (int i = 1; i < n_samples + 2; i++) {
    // lookH[i] = i*looklog[i]-(i+1)*looklog[(i+1)];
    lookH[i] = i * looklog[i];
  }
  // Number of levels r for which we want to store the
  // stochastic NML complexity LogC(n,r) for n in [1,N].
  // For r>N_COL_NML LogC() is computed with the normal
  // recurrence and the result is not stored.
  int ncol = N_COL_NML;
  cterms = new double*[ncol];
  for (int K = 0; K < (ncol); K++) {
    cterms[K] = new double[n_samples + 1];
    for (int i = 0; i < (n_samples + 1); i++) {
      if (K == 1)
        cterms[K][i] = 0;
      else if (i == 0)
        cterms[K][i] = 0;
      else
        cterms[K][i] = -1;
    }
  }
  // Initialize the c2 terms
  for (int i = 0; i < (n_samples + 1); i++) {
    computeLogC(i, 2, looklog, cterms);
  }

  lookchoose = new double*[ncol];
  for (int K = 0; K < (ncol); K++) {
    lookchoose[K] = new double[n_samples + 1];
    for (int i = 0; i < (n_samples + 1); i++) {
      lookchoose[K][i] = -1;
    }
  }

  // create the edge structure and keep track of how many searchMore we have
  edges = new Edge*[n_nodes];
  for (int i = 0; i < n_nodes; i++)
    edges[i] = new Edge[n_nodes];

  for (int i = 0; i < n_nodes; i++) {
    for (int j = 0; j < n_nodes; j++) {
      if ((!is_continuous[i] && levels[i] == n_samples) ||
          (!is_continuous[j] && levels[j] == n_samples)) {
        // If a node is discrete with as many levels as there are samples, its
        // information with other nodes is null.
        edges[i][j].status = 0;
        edges[i][j].status_prev = 0;
      } else {
        // Initialise all other edges.
        edges[i][j].status = 1;
        edges[i][j].status_prev = 1;
      }
    }
  }

  for (int i = 0; i < n_nodes; i++) {
    edges[i][i].status = 0;
    edges[i][i].status_prev = 0;
  }

  noise_vec = new double[2 * n_samples];
  for (int i = 0; i < 2 * n_samples; i++) {
    noise_vec[i] = R::runif(-MAGNITUDE_TIES, MAGNITUDE_TIES);
  }

  oneLineMatrix = new int[n_samples * n_nodes];

  readBlackbox(as<vector<vector<int>>>(arg_list["black_box"]));
}

void Environment::readBlackbox(const vector<vector<int>>& node_list) {
  for (const auto& pair : node_list) {
    edges[pair[0]][pair[1]].status = 0;
    edges[pair[0]][pair[1]].status_prev = 0;
    edges[pair[1]][pair[0]].status = 0;
    edges[pair[1]][pair[0]].status_prev = 0;
  }
}

}  // namespace structure
}  // namespace miic
