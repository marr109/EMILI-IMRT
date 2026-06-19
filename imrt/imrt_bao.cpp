#include "imrt_bao.h"

#ifdef WITH_OSQP

#include "../emilibase.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace emili {
namespace imrt {

/*---------------------------------------------------------------------------*
 * BaoSolution
 *---------------------------------------------------------------------------*/

emili::Solution* BaoSolution::clone()
{
    BaoSolution* s = new BaoSolution(active_angles_, (int)intensities_.size());
    s->angle_degrees_ = angle_degrees_;
    s->intensities_   = intensities_;
    s->solution_value = solution_value;
    return s;
}

std::string BaoSolution::getSolutionRepresentation()
{
    std::ostringstream oss;
    const std::vector<int>& deg = angle_degrees_.empty() ? active_angles_ : angle_degrees_;
    oss << "angles=[";
    for (int i = 0; i < (int)deg.size(); ++i) {
        if (i) oss << ",";
        oss << deg[i];
    }
    oss << (angle_degrees_.empty() ? "(idx)" : "deg");
    oss << "] f=" << std::fixed << std::setprecision(2) << solution_value;
    return oss.str();
}

/*---------------------------------------------------------------------------*
 * BaoProblem
 *---------------------------------------------------------------------------*/

void BaoProblem::openCsvLog(const std::string& path)
{
    csv_file_.open(path);
    if (csv_file_.is_open()) {
        csv_file_ << "eval,angles_deg,objective\n";
        std::cout << "  CSV log: " << path << "\n";
    } else {
        std::cerr << "[BAO] Could not open CSV log: " << path << "\n";
    }
}

double BaoProblem::calcObjectiveFunctionValue(emili::Solution& s)
{
    BaoSolution& bs = static_cast<BaoSolution&>(s);
    auto res = fmo_.solve(bs.active_angles_);
    bs.intensities_ = std::move(res.first);
    return res.second;
}

double BaoProblem::evaluateSolution(emili::Solution& s)
{
    double f = calcObjectiveFunctionValue(s);
    s.setSolutionValue(f);

    BaoSolution& bs = static_cast<BaoSolution&>(s);
    // Keep degree labels updated for display
    bs.angle_degrees_.resize(bs.active_angles_.size());
    for (int i = 0; i < (int)bs.active_angles_.size(); ++i)
        bs.angle_degrees_[i] = inst_.angles[bs.active_angles_[i]];

    if (verbose_) {
        std::cout << "  BAO eval: angles=[";
        for (int i = 0; i < (int)bs.angle_degrees_.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << bs.angle_degrees_[i];
        }
        std::cout << "deg] -> f=" << std::fixed << std::setprecision(2) << f << "\n";
    }

    if (csv_file_.is_open()) {
        csv_file_ << ++eval_count_ << ",\"";
        for (int i = 0; i < (int)bs.angle_degrees_.size(); ++i) {
            if (i) csv_file_ << ";";
            csv_file_ << bs.angle_degrees_[i];
        }
        csv_file_ << "\"," << std::fixed << std::setprecision(6) << f << "\n";
        csv_file_.flush();
    }

    return f;
}

/*---------------------------------------------------------------------------*
 * Initial solutions
 *---------------------------------------------------------------------------*/

emili::Solution* FirstKAnglesInit::generateEmptySolution()
{
    std::vector<int> angles(bao_.K());
    for (int i = 0; i < bao_.K(); ++i) angles[i] = i;
    return new BaoSolution(angles, bao_.getInstance().n_dimlets);
}

emili::Solution* FirstKAnglesInit::generateSolution()
{
    emili::Solution* s = generateEmptySolution();
    bao_.evaluateSolution(*s);
    return s;
}

emili::Solution* RandomKAnglesInit::generateEmptySolution()
{
    int n = bao_.getInstance().n_angles;
    int K = bao_.K();
    std::vector<int> perm(n);
    for (int i = 0; i < n; ++i) perm[i] = i;
    // Fisher-Yates shuffle for first K
    for (int i = 0; i < K; ++i) {
        int j = i + emili::generateRandomNumber() % (n - i);
        std::swap(perm[i], perm[j]);
    }
    std::vector<int> angles(perm.begin(), perm.begin() + K);
    std::sort(angles.begin(), angles.end());
    return new BaoSolution(angles, bao_.getInstance().n_dimlets);
}

emili::Solution* RandomKAnglesInit::generateSolution()
{
    emili::Solution* s = generateEmptySolution();
    bao_.evaluateSolution(*s);
    return s;
}

/*---------------------------------------------------------------------------*
 * AngleSwapNeighborhood
 *---------------------------------------------------------------------------*/

int AngleSwapNeighborhood::size()
{
    int K  = bao_.K();
    int na = bao_.getInstance().n_angles;
    return K * (na - K);
}

emili::Neighborhood::NeighborhoodIterator
AngleSwapNeighborhood::begin(emili::Solution* base)
{
    BaoSolution* bs = static_cast<BaoSolution*>(base);

    // Save base state
    base_angles_ = bs->active_angles_;

    // Build inactive list
    std::vector<bool> active_flag(n_angles_, false);
    for (int ai : base_angles_) active_flag[ai] = true;
    inactive_list_.clear();
    for (int a = 0; a < n_angles_; ++a)
        if (!active_flag[a]) inactive_list_.push_back(a);

    cur_active_idx_   = 0;
    cur_inactive_idx_ = 0;
    first_            = true;

    return emili::Neighborhood::NeighborhoodIterator(this, base);
}

void AngleSwapNeighborhood::reset()
{
    cur_active_idx_   = 0;
    cur_inactive_idx_ = 0;
    first_            = true;
}

emili::Solution* AngleSwapNeighborhood::computeStep(emili::Solution* step)
{
    if (!first_) {
        ++cur_inactive_idx_;
        if (cur_inactive_idx_ >= (int)inactive_list_.size()) {
            ++cur_active_idx_;
            cur_inactive_idx_ = 0;
        }
    }
    first_ = false;

    if (cur_active_idx_ >= (int)base_angles_.size()) return nullptr;
    if (inactive_list_.empty())                       return nullptr;

    // Apply swap: replace base_angles_[cur_active_idx_] with inactive_list_[cur_inactive_idx_]
    BaoSolution* bs = static_cast<BaoSolution*>(step);
    bs->active_angles_ = base_angles_;
    bs->active_angles_[cur_active_idx_] = inactive_list_[cur_inactive_idx_];
    std::sort(bs->active_angles_.begin(), bs->active_angles_.end());

    bao_.evaluateSolution(*bs);
    return bs;
}

void AngleSwapNeighborhood::reverseLastMove(emili::Solution* step)
{
    // Restore the active angle set to the base state.
    // The solution value is reset by the iterator itself (to base_value).
    // Intensities are stale but will be overwritten by the next computeStep.
    BaoSolution* bs = static_cast<BaoSolution*>(step);
    bs->active_angles_ = base_angles_;
}

emili::Solution* AngleSwapNeighborhood::random(emili::Solution* s)
{
    BaoSolution* bs = static_cast<BaoSolution*>(s);
    if (inactive_list_.empty()) return s->clone();

    // Pick a random active angle to remove and a random inactive to add
    int ai = emili::generateRandomNumber() % bs->active_angles_.size();
    int ii = emili::generateRandomNumber() % inactive_list_.size();

    BaoSolution* nb = static_cast<BaoSolution*>(s->clone());
    nb->active_angles_[ai] = inactive_list_[ii];
    std::sort(nb->active_angles_.begin(), nb->active_angles_.end());
    bao_.evaluateSolution(*nb);
    return nb;
}

/*---------------------------------------------------------------------------*
 * RandomAnglesPerturbation
 *---------------------------------------------------------------------------*/

emili::Solution* RandomAnglesPerturbation::perturb(emili::Solution* current)
{
    BaoSolution* bs = static_cast<BaoSolution*>(current->clone());
    const int n = bao_.getInstance().n_angles;

    // Build inactive list
    std::vector<bool> active_flag(n, false);
    for (int ai : bs->active_angles_) active_flag[ai] = true;
    std::vector<int> inactive;
    inactive.reserve(n - (int)bs->active_angles_.size());
    for (int a = 0; a < n; ++a)
        if (!active_flag[a]) inactive.push_back(a);

    int swaps = std::min(p_, std::min((int)bs->active_angles_.size(),
                                      (int)inactive.size()));

    // Partial Fisher-Yates on both lists to pick 'swaps' random pairs
    for (int i = 0; i < swaps; ++i) {
        int ai = i + (int)(emili::generateRandomNumber() % (bs->active_angles_.size() - i));
        std::swap(bs->active_angles_[i], bs->active_angles_[ai]);
        int ii = i + (int)(emili::generateRandomNumber() % (inactive.size() - i));
        std::swap(inactive[i], inactive[ii]);
    }
    for (int i = 0; i < swaps; ++i)
        bs->active_angles_[i] = inactive[i];

    std::sort(bs->active_angles_.begin(), bs->active_angles_.end());
    bao_.evaluateSolution(*bs);
    return bs;
}

/*---------------------------------------------------------------------------*
 * BaoTabuMemory
 *---------------------------------------------------------------------------*/

bool BaoTabuMemory::tabu_check(emili::Solution* s)
{
    const BaoSolution* bs = static_cast<const BaoSolution*>(s);
    for (int i = 0; i < count_; ++i) {
        int idx = (head_ - 1 - i + tenure_) % tenure_;
        if (memory_[idx] == bs->active_angles_)
            return true;
    }
    return false;
}

void BaoTabuMemory::forbid(emili::Solution* s)
{
    const BaoSolution* bs = static_cast<const BaoSolution*>(s);
    memory_[head_] = bs->active_angles_;
    head_ = (head_ + 1) % tenure_;
    if (count_ < tenure_) ++count_;
}

/*---------------------------------------------------------------------------*
 * AdaptiveBaoTabuMemory
 *---------------------------------------------------------------------------*/

void AdaptiveBaoTabuMemory::resizeTenure(int new_tenure)
{
    std::vector<std::vector<int>> new_mem(new_tenure);
    int entries = std::min(count_, new_tenure);
    for (int i = 0; i < entries; ++i) {
        int old_idx = (head_ - 1 - i + tenure_) % tenure_;
        int new_idx = (new_tenure - 1 - i + new_tenure) % new_tenure;
        new_mem[new_idx] = memory_[old_idx];
    }
    memory_  = std::move(new_mem);
    head_    = entries % new_tenure;
    count_   = entries;
    tenure_  = new_tenure;
    setTabuTenure(new_tenure);
}

void AdaptiveBaoTabuMemory::forbid(emili::Solution* s)
{
    if (tabu_check(s)) {
        // Oscillation: revisit detected → increase tenure
        if (tenure_ < tenure_max_) resizeTenure(tenure_ + 1);
        since_last_revisit_ = 0;
    } else {
        ++since_last_revisit_;
        // Quiet period: no revisit for 2×tenure steps → decrease tenure
        if (since_last_revisit_ >= 2 * tenure_ && tenure_ > tenure_min_) {
            resizeTenure(tenure_ - 1);
            since_last_revisit_ = 0;
        }
    }
    BaoTabuMemory::forbid(s);
}

/*---------------------------------------------------------------------------*
 * MultiScaleAngleShake
 *---------------------------------------------------------------------------*/

emili::Solution* MultiScaleAngleShake::shake(emili::Solution* s, int k)
{
    // k = 0 → 1 swap, k = 1 → 2 swaps, …, k = Kmax-1 → Kmax swaps
    RandomAnglesPerturbation pert(bao_, k + 1);
    return pert.perturb(s);
}

/*---------------------------------------------------------------------------*
 * GreedyAnglesPerturbation
 *---------------------------------------------------------------------------*/

emili::Solution* GreedyAnglesPerturbation::perturb(emili::Solution* current)
{
    BaoSolution* bs = static_cast<BaoSolution*>(current->clone());
    const int n = bao_.getInstance().n_angles;
    const int K = bao_.K();

    // Build inactive list
    std::vector<bool> active_flag(n, false);
    for (int ai : bs->active_angles_) active_flag[ai] = true;
    std::vector<int> inactive;
    inactive.reserve(n - K);
    for (int a = 0; a < n; ++a)
        if (!active_flag[a]) inactive.push_back(a);

    int D = std::min(D_, std::min(K, (int)inactive.size()));

    // Destruction: remove D random active angles (Fisher-Yates on prefix)
    for (int i = 0; i < D; ++i) {
        int ai = i + emili::generateRandomNumber() % (K - i);
        std::swap(bs->active_angles_[i], bs->active_angles_[ai]);
        inactive.push_back(bs->active_angles_[i]);
    }
    bs->active_angles_.erase(bs->active_angles_.begin(),
                              bs->active_angles_.begin() + D);

    // Greedy construction: add D angles one by one, choosing best FMO each step
    for (int step = 0; step < D; ++step) {
        double best_f   = 1e30;
        int    best_pos = 0;

        for (int j = 0; j < (int)inactive.size(); ++j) {
            // Temporarily add inactive[j] and evaluate
            bs->active_angles_.push_back(inactive[j]);
            std::sort(bs->active_angles_.begin(), bs->active_angles_.end());

            double f = bao_.calcObjectiveFunctionValue(*bs);
            bs->setSolutionValue(f);

            if (f < best_f) { best_f = f; best_pos = j; }

            // Undo: remove the trial angle
            bs->active_angles_.erase(
                std::find(bs->active_angles_.begin(),
                          bs->active_angles_.end(), inactive[j]));
        }

        // Commit the best angle found in this step
        bs->active_angles_.push_back(inactive[best_pos]);
        std::sort(bs->active_angles_.begin(), bs->active_angles_.end());
        bao_.evaluateSolution(*bs);
        inactive.erase(inactive.begin() + best_pos);
    }

    return bs;
}

} // namespace imrt
} // namespace emili

#endif // WITH_OSQP
