// Module to compute the conditional mutual information for mixed continuous
// and discrete variables.
#ifndef MIIC_INFO_CNT_H_
#define MIIC_INFO_CNT_H_

#include <algorithm>
#include <functional>
#include <vector>

#include "environment.h"
#include "structure.h"

namespace miic {
namespace computation {

double* compute_mi_cond_alg1(const std::vector<std::vector<int>>& data,
    const std::vector<std::vector<int>>& sortidx,
    const std::vector<int>& AllLevels, const std::vector<int>& ptr_cnt,
    const std::vector<int>& ptrVarIdx, int nbrUi, int n,
    const std::vector<double>& sample_weights, bool flag_sample_weights,
    structure::Environment& environment, bool saveIterations = false);

double* compute_Rscore_Ixyz_alg5(const std::vector<std::vector<int>>& data,
    const std::vector<std::vector<int>>& sortidx,
    const std::vector<int>& AllLevels, const std::vector<int>& ptr_cnt,
    const std::vector<int>& ptrVarIdx, int nbrUi, int ptrZiIdx, int n,
    const std::vector<double>& sample_weights, bool flag_sample_weights,
    structure::Environment& environment, bool saveIterations = false);

}  // namespace computation
}  // namespace miic

#endif  // MIIC_INFO_CNT_H_
