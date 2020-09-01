#include "skeleton.h"

#include <Rcpp.h>

#include <algorithm>  // std::sort, std::max_element
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "compute_ens_information.h"
#include "structure.h"
#include "utilities.h"

using Rcpp::Rcout;
using Rcpp::Rcerr;
using std::endl;
using std::vector;
using namespace miic::computation;
using namespace miic::reconstruction;
using namespace miic::structure;
using namespace miic::utility;

template <bool latent = false>
void searchAndSetZi(
    Environment& environment, const int posX, const int posY) {
  // Search candidate nodes for the separation set of X and Y
  int numZiPos = 0;
  for (int c = 0; c < environment.n_nodes; c++) {
    if (c == posX || c == posY) continue;
    if (!latent && !environment.edges[posX][c].status_prev &&
        !environment.edges[posY][c].status_prev)
      continue;
    environment.edges[posX][posY].shared_info->zi_list.push_back(c);
    numZiPos++;
  }

  if (environment.verbose)
    Rcout << "The number of neighbours is: " << numZiPos << endl;
}

void searchAndSetZi(
    Environment& environment, const int posX, const int posY) {
  if (environment.latent)
    return searchAndSetZi<true>(environment, posX, posY);
  else
    return searchAndSetZi<false>(environment, posX, posY);
}

namespace miic {
namespace reconstruction {

// Initialize the edges of the network
int initializeEdge(Environment& environment, int i, int j) {
  // Compute the mutual information and the corresponding CPLX
  auto info = environment.edges[i][j].shared_info;
  double* res = NULL;
  if (!environment.is_continuous[i] && !environment.is_continuous[j]) {
    TempAllocatorScope scope;
    res = computeEnsInformationNew(
        environment, i, j, vector<int>(), TempVector<int>(), environment.cplx);
    info->Ixy_ui = res[1];
    info->cplx = res[2];
    info->Nxy_ui = res[0];
  } else {
    res = computeEnsInformationContinuous(environment, i, j, std::vector<int>(),
        std::vector<int>(), environment.cplx);
    info->Ixy_ui = res[1];
    info->cplx = res[2];
    info->Nxy_ui = res[0];
  }
  delete[] res;

  info->Ixy = info->Ixy_ui;
  info->cplx_no_u = info->cplx;
  info->Nxy = info->Nxy_ui;

  double myTest = info->Ixy_ui - info->cplx;
  if (!environment.no_init_eta)
    myTest -= environment.log_eta;

  if (myTest <= 0) {
    // Unconditional independence
    info->connected = 0;
    environment.edges[i][j].status = 0;
    environment.edges[j][i].status = 0;
    environment.edges[i][j].status_init = 0;
    environment.edges[j][i].status_init = 0;
  } else {
    info->connected = 1;
    environment.edges[i][j].status = 1;
    environment.edges[j][i].status = 1;
    environment.edges[i][j].status_init = 1;
    environment.edges[j][i].status_init = 1;
  }

  return environment.edges[i][j].status;
}

bool skeletonInitialization(Environment& environment) {
  for (int i = 0; i < environment.n_samples; i++) {
    for (int j = 0; j < environment.n_nodes; j++) {
      environment.oneLineMatrix[j * environment.n_samples + i] =
          environment.data_numeric[i][j];
    }
  }

  bool interrupt = false;
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
  for (int i = 0; i < environment.n_nodes - 1; i++) {
    if (interrupt) {
      continue;  // will continue until out of for loop
    }
    int threadnum = 0;
#ifdef _OPENMP
    threadnum = omp_get_thread_num();
#endif
    if (threadnum == 0) {
      if (checkInterrupt()) {
        interrupt = true;
        continue;
      }
    }
    for (int j = i + 1; j < environment.n_nodes && !interrupt; j++) {
      environment.edges[i][j].shared_info = std::make_shared<EdgeSharedInfo>();
      environment.edges[j][i].shared_info = environment.edges[i][j].shared_info;
      if (environment.edges[i][j].status) {
        initializeEdge(environment, i, j);
      }
    }
  }
  if (interrupt) return false;
  return true;
}

bool firstStepIteration(Environment& environment, BiconnectedComponent& bcc) {
  // During first step iteration, search for U contributors is not
  // parallelizable see flag "parallelizable" in
  // computeEnsInformationContinuous() in computeEnsInformation.cpp
  environment.first_iter_done = false;

  environment.connected_list.clear();
  environment.unsettled_list.clear();

  // create and fill the unsettled_list struct, that keep track of i and j
  // positions of searchMore Edges
  environment.numSearchMore = 0;
  environment.numNoMore = 0;
  for (int i = 0; i < environment.n_nodes - 1; i++) {
    for (int j = i + 1; j < environment.n_nodes; j++) {
      // Do dot consider edges removed with unconditional independence
      if (!environment.edges[i][j].status) continue;
      environment.edges[i][j].shared_info->reset();
      environment.unsettled_list.emplace_back(i, j, environment.edges[i][j]);
      environment.numSearchMore++;
    }
  }

  if (environment.numSearchMore > 0) {
    if (environment.verbose) {
      Rcout << "SEARCH OF BEST Z: ";
    }

    bool interrupt = false;
    int progress_percentile = -1;
    int n_jobs_done{0};
    auto loop_start_time = getLapStartTime();
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
    for (int i = 0; i < environment.numSearchMore; i++) {
      if (interrupt) {
        continue;  // will continue until out of for loop
      }
      int threadnum = 0;
#ifdef _OPENMP
      threadnum = omp_get_thread_num();
#endif
      if (threadnum == 0) {
        if (checkInterrupt()) interrupt = true;
      }
      const auto& edgeid = environment.unsettled_list[i];
      int X = edgeid.X;
      int Y = edgeid.Y;
      if (environment.verbose) {
        Rcout << "\n# --------------------\n# ----> EDGE: "
             << environment.nodes[X].name << "--"
             << environment.nodes[Y].name << "\n# --------------------";
      }
      if (environment.consistent > 0)
        bcc.setCandidateZ(X, Y);
      else
        searchAndSetZi(environment, X, Y);
      if (edgeid.getEdge().shared_info->zi_list.size() > 0)
        SearchForNewContributingNodeAndItsRank(environment, X, Y);

#ifdef _OPENMP
#pragma omp atomic
      ++n_jobs_done;
#endif
      if (threadnum == 0)
        printProgress(
            static_cast<double>(n_jobs_done) / environment.numSearchMore,
            loop_start_time, progress_percentile);
    }
    // Print finished progress bar
    printProgress(1.0, loop_start_time, progress_percentile);
    Rcerr << '\n';

    if (interrupt) return false;

    for (int i = 0; i < environment.numSearchMore; i++) {
      int posX = environment.unsettled_list[i].X;
      int posY = environment.unsettled_list[i].Y;
      if (environment.edges[posX][posY].shared_info->z_name_idx != -1) {
        if (environment.verbose) {
          Rcout << "## ------!!--> Update the edge element in 'searchMore': "
               << environment
                      .nodes[environment.edges[posX][posY].shared_info->zi_list
                                 [environment.edges[posX][posY]
                                         .shared_info->z_name_idx]]
                      .name
               << " is a good zi candidate\n";
        }
      } else {
        if (environment.verbose) {
          Rcout << "## ------!!--> Remove the edge element from searchMore.\n## "
                  "------!!--> Add edge to 'noMore' (no good zi candidate)\n";
        }
        // Move this edge element to "noMore"
        environment.connected_list.push_back(environment.unsettled_list[i]);
        environment.numNoMore++;
        environment.unsettled_list.erase(
            environment.unsettled_list.begin() + i);
        environment.numSearchMore--;
        i--;
        // Update the status
        environment.edges[posX][posY].shared_info->connected = 1;
      }
      if (environment.verbose) Rcout << "\n";
    }

    if (checkInterrupt()) {
      return false;
    }
  }
  environment.first_iter_done = true;
  return true;
}

bool skeletonIteration(Environment& environment) {
  if (environment.verbose)
    Rcout << "Number of numSearchMore: " << environment.numSearchMore << endl;

  auto& unsettled_list = environment.unsettled_list;

  int iIteration_count = 0;
  auto loop_start_time = getLapStartTime();
  int start_numSearchMore = environment.numSearchMore;
  int progress_percentile = -1;
  while (environment.numSearchMore > 0) {
    if (checkInterrupt()) {
      return false;
    }
    iIteration_count++;
    if (environment.verbose) {
      Rcout << "\n# Iteration " << iIteration_count << "\n";
    }

    auto it_max = std::max_element(begin(unsettled_list), end(unsettled_list),
        [&environment](const EdgeID& a, const EdgeID& b) {
          return environment.edges[a.X][a.Y].shared_info->Rxyz_ui <
                 environment.edges[b.X][b.Y].shared_info->Rxyz_ui;
        });
    int X = it_max->X;
    int Y = it_max->Y;
    if (environment.verbose)
      Rcout << "Pos x : " << X << " , pos y: " << Y << endl;

    auto top_info = environment.edges[X][Y].shared_info;

    if (environment.verbose) Rcout << "# Before adding new zi to {ui}: ";
    // Reinit ui.vect, z.name, zi.vect, z.name.idx
    if (environment.verbose) {
      Rcout << "# DO: Add new zi to {ui}: " << top_info->z_name_idx << endl;
    }
    // move top z_name_idx from zi_vect to ui_vect
    top_info->ui_list.push_back(top_info->zi_list[top_info->z_name_idx]);
    top_info->zi_list.erase(top_info->zi_list.begin() + top_info->z_name_idx);
    top_info->z_name_idx = -1;

    double* v = NULL;
    if (!environment.is_continuous[X] && !environment.is_continuous[Y] &&
        std::all_of(top_info->ui_list.cbegin(), top_info->ui_list.cend(),
            [&environment](int i) { return !environment.is_continuous[i]; })) {
      TempAllocatorScope scope;
      v = computeEnsInformationNew(environment, X, Y, top_info->ui_list,
          TempVector<int>(), environment.cplx);

      top_info->Ixy_ui = v[1];
      top_info->Nxy_ui = v[0];
      top_info->cplx = v[2];
    } else {
      v = computeEnsInformationContinuous(environment, X, Y, top_info->ui_list,
          vector<int>(), environment.cplx);
      top_info->Nxy_ui = v[0];
      top_info->Ixy_ui = v[1];
      top_info->cplx = v[2];
    }
    delete[] v;
    double top_info_kxy_ui = top_info->cplx;

    if (environment.degenerate)
      top_info_kxy_ui = top_info->cplx + (top_info->ui_list.size() * log(3.0));

    int nRemainingEdges = environment.numSearchMore + environment.numNoMore;

    if (environment.verbose) {
      Rcout << "# --> nbrEdges L = " << nRemainingEdges << "\n";
      Rcout << "# --> nbrProp P = " << environment.n_nodes << "\n\n";
      Rcout << "topEdgeElt->Ixy_ui " << top_info->Ixy_ui << "\n";
      Rcout << "topEdgeElt_kxy_ui " << top_info_kxy_ui << "\n";
      Rcout << "environment.log_eta " << environment.log_eta << "\n";
      Rcout << "IsPhantom? "
            << (top_info->Ixy_ui - top_info_kxy_ui - environment.log_eta <= 0)
            << endl;
    }
    if (top_info->Ixy_ui - top_info_kxy_ui - environment.log_eta <= 0) {
      // Conditional independence found, remove edge
      if (environment.verbose) {
        Rcout << "# PHANTOM" << environment.nodes[X].name << ","
              << environment.nodes[Y].name << "\n";
      }
      unsettled_list.erase(it_max);
      environment.numSearchMore--;
      // Set the connection to 0 on the adj matrix
      environment.edges[X][Y].status = 0;
      environment.edges[Y][X].status = 0;
      // Save the phantom status
      top_info->connected = 0;
    } else {
      // Reinit Rxyz_ui
      top_info->Rxyz_ui = environment.thresPc;

      if (environment.verbose) {
        Rcout << "# Do SearchForNewContributingNodeAndItsRank\n";
      }

      if (top_info->zi_list.size() > 0) {
        SearchForNewContributingNodeAndItsRank(environment, X, Y);
      }

      if (environment.verbose) {
        if (environment.edges[X][Y].shared_info->z_name_idx == -1)
          Rcout << "# See topEdgeElt[['z.name']]: NA\n";
        else
          Rcout
              << "# See topEdgeElt[['z.name']]: "
              << environment.nodes[top_info->zi_list[top_info->z_name_idx]].name
              << "\n";
      }
      // Update the information about the edge
      if (top_info->z_name_idx == -1) {
        if (environment.verbose) {
          Rcout << "Add edge to connected_list.\n";
        }
        // Move this edge from the list searchMore to noMore
        environment.connected_list.push_back(*it_max);
        environment.numNoMore++;
        unsettled_list.erase(it_max);
        environment.numSearchMore--;
        // Update the status of the edge
        top_info->connected = 1;
      }
    }

    printProgress(1.0 * (start_numSearchMore - environment.numSearchMore) /
                      (start_numSearchMore),
        loop_start_time, progress_percentile);
  }
  Rcerr << "\n";
  std::sort(begin(environment.connected_list), end(environment.connected_list));
  return true;
}

}  // namespace reconstruction
}  // namespace miic
