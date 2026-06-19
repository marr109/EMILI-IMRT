#ifndef IMRT_BAO_H
#define IMRT_BAO_H

#ifdef WITH_OSQP

#include "../emilibase.h"
#include "imrt_instance.h"
#include "imrt_fmo.h"
#include <fstream>
#include <string>
#include <vector>

namespace emili {
namespace imrt {

/*---------------------------------------------------------------------------*
 *                            BAO SOLUTION                                   *
 *                                                                           *
 * A BaoSolution represents a selection of K gantry angles (the outer BAO   *
 * decision) together with the OSQP-optimal beamlet intensities for that     *
 * selection (the inner FMO result).                                         *
 *---------------------------------------------------------------------------*/
class BaoSolution : public emili::Solution {
public:
    std::vector<int>    active_angles_; // K sorted angle indices (into inst.angles)
    std::vector<int>    angle_degrees_; // K actual degree values (for display only)
    std::vector<double> intensities_;   // n_beamlets optimal x* from FMO

    BaoSolution(const std::vector<int>& angles, int n_beamlets)
        : emili::Solution(1e30)
        , active_angles_(angles)
        , intensities_(n_beamlets, 0.0)
    {}

    virtual const void* getRawData() const override { return this; }
    virtual void setRawData(const void* data) override {
        if (data == this) return;
        const BaoSolution* o = static_cast<const BaoSolution*>(data);
        active_angles_ = o->active_angles_;
        angle_degrees_ = o->angle_degrees_;
        intensities_   = o->intensities_;
    }

    virtual emili::Solution* clone() override;
    virtual std::string getSolutionRepresentation() override;
    virtual bool isFeasible() override { return true; }
    virtual ~BaoSolution() {}
};


/*---------------------------------------------------------------------------*
 *                             BAO PROBLEM                                   *
 *                                                                           *
 * BaoProblem wraps ImrtFmoSolver.  Evaluating a BaoSolution means calling   *
 * OSQP to optimise beamlet intensities for the given angle subset, then     *
 * storing the result back into the solution.                                *
 *---------------------------------------------------------------------------*/
class BaoProblem : public emili::Problem {
    ImrtInstance   inst_;   // owned copy (ImrtFmoSolver holds a ref to this)
    ImrtFmoSolver  fmo_;
    int            K_;
    bool           verbose_;
    std::ofstream  csv_file_;
    int            eval_count_;

public:
    BaoProblem(ImrtInstance& inst, int K)
        : inst_(inst), fmo_(inst_), K_(K), verbose_(false), eval_count_(0) {}

    virtual double calcObjectiveFunctionValue(emili::Solution& s) override;
    virtual double evaluateSolution(emili::Solution& s) override;
    virtual int    problemSize() override { return inst_.n_angles; }

    int  K()                          const { return K_; }
    const ImrtInstance& getInstance() const { return inst_; }

    void setVerbose(bool v) { verbose_ = v; }
    bool isReady()          const { return fmo_.isReady(); }
    void openCsvLog(const std::string& path);
};


/*---------------------------------------------------------------------------*
 *                        INITIAL SOLUTIONS                                  *
 *---------------------------------------------------------------------------*/

/** Select the first K angles (indices 0, 1, …, K-1). */
class FirstKAnglesInit : public emili::InitialSolution {
    BaoProblem& bao_;
public:
    explicit FirstKAnglesInit(BaoProblem& p)
        : emili::InitialSolution(p), bao_(p) {}

    virtual emili::Solution* generateSolution()      override;
    virtual emili::Solution* generateEmptySolution() override;
};

/** Select K angles uniformly at random. */
class RandomKAnglesInit : public emili::InitialSolution {
    BaoProblem& bao_;
public:
    explicit RandomKAnglesInit(BaoProblem& p)
        : emili::InitialSolution(p), bao_(p) {}

    virtual emili::Solution* generateSolution()      override;
    virtual emili::Solution* generateEmptySolution() override;
};


/*---------------------------------------------------------------------------*
 *                          ANGLE-SWAP NEIGHBORHOOD                          *
 *                                                                           *
 * Remove one active angle and add one inactive angle.                       *
 * Size = K × (n_angles − K).                                               *
 *---------------------------------------------------------------------------*/
class AngleSwapNeighborhood : public emili::Neighborhood {
    BaoProblem& bao_;
    int         n_angles_;

    // State for iteration
    std::vector<int> base_angles_;     // active angles at begin()
    std::vector<int> inactive_list_;   // inactive angles at begin()
    int  cur_active_idx_;              // which active angle to remove
    int  cur_inactive_idx_;            // which inactive angle to add
    bool first_;

    virtual emili::Solution* computeStep(emili::Solution* step)  override;
    virtual void reverseLastMove(emili::Solution* step)           override;

public:
    explicit AngleSwapNeighborhood(BaoProblem& p)
        : bao_(p), n_angles_(p.getInstance().n_angles)
        , cur_active_idx_(0), cur_inactive_idx_(0), first_(true)
    {}

    virtual emili::Neighborhood::NeighborhoodIterator
            begin(emili::Solution* base) override;
    virtual void reset() override;
    virtual emili::Solution* random(emili::Solution* s) override;
    virtual int size() override;
};

/*---------------------------------------------------------------------------*
 *                       ANGLE PERTURBATION (for ILS)                        *
 *                                                                           *
 * Randomly replaces p active angles with p inactive angles.                 *
 * Enables escaping local optima in an Iterated Local Search.                *
 *---------------------------------------------------------------------------*/
class RandomAnglesPerturbation : public emili::Perturbation {
    BaoProblem& bao_;
    int         p_;   // number of angles to swap out

public:
    RandomAnglesPerturbation(BaoProblem& bao, int p)
        : emili::Perturbation(), bao_(bao), p_(p) {}

    virtual emili::Solution* perturb(emili::Solution* current) override;
};


/*---------------------------------------------------------------------------*
 *                    BAO TABU MEMORY (angle-set based)                       *
 *                                                                           *
 * Stores K-element active angle sets in a circular buffer.                  *
 * Suitable for the BAO level where moves are angle swaps.                   *
 *---------------------------------------------------------------------------*/
class BaoTabuMemory : public emili::TabuMemory {
protected:
    std::vector<std::vector<int>> memory_;
    int head_, count_;
    int tenure_;   // local copy (base field is private)

public:
    explicit BaoTabuMemory(int tenure)
        : emili::TabuMemory(tenure)
        , memory_(tenure)
        , head_(0), count_(0), tenure_(tenure) {}

    virtual bool tabu_check(emili::Solution* s) override;
    virtual void forbid(emili::Solution* s)     override;
    virtual void reset() override { head_ = 0; count_ = 0; }

    int  getTenure() const { return tenure_; }
};


/*---------------------------------------------------------------------------*
 *                  ADAPTIVE BAO TABU MEMORY (reactive tenure)               *
 *                                                                           *
 * Extends BaoTabuMemory with dynamic tenure adjustment:                     *
 *   - Oscillation detected (revisit): tenure increases by 1 (up to max).   *
 *   - Quiet period (2×tenure steps without revisit): tenure decreases by 1. *
 *---------------------------------------------------------------------------*/
class AdaptiveBaoTabuMemory : public BaoTabuMemory {
    int tenure_min_, tenure_max_;
    int since_last_revisit_;

    void resizeTenure(int new_tenure);

public:
    AdaptiveBaoTabuMemory(int tenure_min, int tenure_max)
        : BaoTabuMemory((tenure_min + tenure_max) / 2)
        , tenure_min_(tenure_min), tenure_max_(tenure_max)
        , since_last_revisit_(0) {}

    virtual void forbid(emili::Solution* s) override;
};


/*---------------------------------------------------------------------------*
 *                    MULTI-SCALE SHAKE (for rVNS)                           *
 *                                                                           *
 * shake(s, k) performs (k+1) random angle swaps.                           *
 * Provides shaking strengths k = 0, 1, …, p_max-1 for emili::GVNS.        *
 *---------------------------------------------------------------------------*/
class MultiScaleAngleShake : public emili::Shake {
    BaoProblem& bao_;

public:
    MultiScaleAngleShake(BaoProblem& bao, int p_max)
        : emili::Shake(p_max), bao_(bao) {}

    virtual emili::Solution* shake(emili::Solution* s, int k) override;
};


/*---------------------------------------------------------------------------*
 *                  GREEDY ANGLES PERTURBATION (Iterated Greedy)             *
 *                                                                           *
 * Destroys D randomly chosen active angles, then reconstructs them          *
 * greedily: at each step, every inactive candidate is evaluated via OSQP   *
 * and the one minimising the FMO objective is added.                        *
 *---------------------------------------------------------------------------*/
class GreedyAnglesPerturbation : public emili::Perturbation {
    BaoProblem& bao_;
    int         D_;   // number of angles to destroy and rebuild

public:
    GreedyAnglesPerturbation(BaoProblem& bao, int D)
        : emili::Perturbation(), bao_(bao), D_(D) {}

    virtual emili::Solution* perturb(emili::Solution* current) override;
};

} // namespace imrt
} // namespace emili

#endif // WITH_OSQP
#endif // IMRT_BAO_H