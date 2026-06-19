#ifndef IMRT_BUILDER_H
#define IMRT_BUILDER_H

#include "../generalParser.h"
#include "imrt.h"
#ifdef WITH_OSQP
#include "imrt_bao.h"
#endif

namespace prs {
namespace imrt {

/**
 * ImrtBuilder — parses command-line tokens and constructs IMRT components.
 *
 * ── FMO problem (classic metaheuristic on beamlet intensities) ─────────────
 *   Token: imrt  <instance_dir>  [nactive <K>]  [verbose]
 *
 * ── BAO problem (OSQP-exact FMO + combinatorial angle search) ─────────────
 *   Token: baoimrt  <K>  <instance_dir>  [verbose]
 *   (requires WITH_OSQP=ON at cmake time)
 *
 * ── FMO initial solutions ─────────────────────────────────────────────────
 *   izero                  all intensities = 0
 *   iuniform  <x>          all intensities = x
 *   irandom   <max>        random intensities in [0, max]
 *
 * ── BAO initial solutions ─────────────────────────────────────────────────
 *   ifirstk                first K angles (0,1,...,K-1)
 *   irandomk               K randomly chosen angles
 *
 * ── FMO neighborhoods ─────────────────────────────────────────────────────
 *   nshift   <delta>       SingleBeamletShift
 *   nswap                  BeamletSwap
 *
 * ── BAO neighborhoods ─────────────────────────────────────────────────────
 *   nangswap               AngleSwapNeighborhood (swap one active ↔ inactive)
 *
 * ── Perturbations ─────────────────────────────────────────────────────────
 *   prandom  <k> <max>     RandomBeamletPerturbation
 *
 * ── Acceptance ────────────────────────────────────────────────────────────
 *   aimprove               ImrtImproveAccept
 *
 * ── Termination ───────────────────────────────────────────────────────────
 *   tmaxiter  <n>          ImrtMaxIterations / iterations on angle swaps
 *   tfeasible              ImrtFeasibleTermination
 */
class ImrtBuilder : public Builder {
public:
    ImrtBuilder(GeneralParserE& gp, TokenManager& tm)
        : Builder(gp, tm) {}

    virtual bool isCompatibleWith(char* problem_definition) override;
    virtual bool canOpenInstance(char* problem_definition) override;
    virtual emili::Problem* openInstance() override;

    virtual emili::InitialSolution*    buildInitialSolution()    override;
    virtual emili::Neighborhood*       buildNeighborhood()       override;
    virtual emili::Perturbation*       buildPerturbation()       override;
    virtual emili::Acceptance*         buildAcceptance()         override;
    virtual emili::Termination*        buildTermination()        override;
    virtual emili::TabuMemory*         buildTabuTenure()         override;
    virtual emili::Shake*              buildShake()              override;

private:
    emili::imrt::ImrtProblem* castProblem();
#ifdef WITH_OSQP
    emili::imrt::BaoProblem*  castBaoProblem();
    bool isBaoProblem();
#endif
};

} // namespace imrt
} // namespace prs

#endif // IMRT_BUILDER_H
