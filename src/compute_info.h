#ifndef MIIC_COMPUTE_INFO_H_
#define MIIC_COMPUTE_INFO_H_

#include "computation_cache.h"
#include "structure.h"

namespace miic {
namespace computation {

double* getAllInfoNEW(const std::vector<std::vector<int>>& data,
    const std::vector<int>& ptrAllLevels, int id_x, int id_y,
    const std::vector<int>& ui_list, const structure::TempVector<int>& zi_list,
    int sampleSizeEff, int modCplx, int k23, const std::vector<double>& weights,
    const structure::TempGrid2d<double>& freqs1, bool test_mar,
    std::shared_ptr<CtermCache> cache);

}  // namespace computation
}  // namespace miic

#endif  // MIIC_COMPUTE_INFO_H_
