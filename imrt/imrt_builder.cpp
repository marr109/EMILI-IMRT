#include "imrt_builder.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

// ── Token keywords ────────────────────────────────────────────────────────────
#define PROBLEM_IMRT    "imrt"
#define PROBLEM_BAO     "baoimrt"
#define INIT_ZERO       "izero"
#define INIT_UNIFORM    "iuniform"
#define INIT_RANDOM     "irandom"
#define INIT_FIRSTK     "ifirstk"
#define INIT_RANDOMK    "irandomk"
#define NEIGH_SHIFT     "nshift"
#define NEIGH_SWAP      "nswap"
#define NEIGH_ANGSWAP   "nangswap"
#define PERT_RANDOM     "prandom"
#define PERT_ANGSWAP    "prangswap"
#define ACC_IMPROVE     "aimprove"
#define TERM_MAXITER    "tmaxiter"
#define TERM_FEASIBLE   "tfeasible"
#define TABU_ALL_SOL    "Tabu_all_solution"
#define TABU_1_CAR      "Tabu_1_car"
#define TABU_1_POS      "Tabu_1_position"
#define OPT_NACTIVE        "nactive"
#define OPT_VERBOSE        "verbose"
#define OPT_CSV            "csv"
// ── New metaheuristic components ──────────────────────────────────────────────
#define PERT_GREEDY        "pgreedy"
#define TABU_BAO_FIXED     "TBao_fixed"
#define TABU_BAO_ADAPTIVE  "TBao_adaptive"
#define SHAKE_BANGSHAKE    "bangshake"

namespace prs {
namespace imrt {

/*---------------------------------------------------------------------------*
 * Helpers
 *---------------------------------------------------------------------------*/

emili::imrt::ImrtProblem* ImrtBuilder::castProblem()
{
    return static_cast<emili::imrt::ImrtProblem*>(gp.getInstance());
}

#ifdef WITH_OSQP
emili::imrt::BaoProblem* ImrtBuilder::castBaoProblem()
{
    return dynamic_cast<emili::imrt::BaoProblem*>(gp.getInstance());
}

bool ImrtBuilder::isBaoProblem()
{
    return castBaoProblem() != nullptr;
}
#endif

/*---------------------------------------------------------------------------*
 * Problem identification
 *---------------------------------------------------------------------------*/

bool ImrtBuilder::isCompatibleWith(char* problem_definition)
{
    if (strcmp(problem_definition, PROBLEM_IMRT) == 0) return true;
#ifdef WITH_OSQP
    if (strcmp(problem_definition, PROBLEM_BAO)  == 0) return true;
#endif
    return false;
}

bool ImrtBuilder::canOpenInstance(char* problem_definition)
{
    return isCompatibleWith(problem_definition);
}

/*---------------------------------------------------------------------------*
 * openInstance — dispatches on "imrt" vs "baoimrt"
 *
 * Token layout when called:
 *   tokens[0]            = executable path  (skipped by parseParams)
 *   tokens[1]            = instance directory
 *   tokens[currentToken] = problem keyword  ("imrt" or "baoimrt") ← peek here
 *   tokens[currentToken+1..] = optional K (BAO), then algorithm tokens
 *---------------------------------------------------------------------------*/

emili::Problem* ImrtBuilder::openInstance()
{
    // The problem keyword is the CURRENT token (not yet consumed).
    // peek() returns tokens[currentToken] without advancing.
    bool is_bao = false;
    int  K      = 0;

#ifdef WITH_OSQP
    {
        char* kw = tm.peek();
        if (kw && strcmp(kw, PROBLEM_BAO) == 0)
            is_bao = true;
    }
#endif

    // Instance directory lives at absolute token index 1 (argv[1]).
    char* dir = tm.tokenAt(1);
    tm.nextToken();   // consume the problem keyword ("imrt" or "baoimrt")
    prs::check(dir, "IMRT: expected path to instance directory");

#ifdef WITH_OSQP
    if (is_bao)
        K = tm.getInteger();   // consume K (number of gantry angles to select)
#endif

    emili::imrt::ImrtInstance inst;
    if (!inst.loadFromDirectory(dir)) {
        std::cerr << "[IMRT] Could not load instance from: " << dir << "\n";
        exit(-1);
    }

    prs::printTabPlusOne("directory", dir);
    prs::printTabPlusOne("angles",    inst.n_angles);
    prs::printTabPlusOne("dimlets",   inst.n_dimlets);
    prs::printTabPlusOne("boxets",    inst.n_boxets_total);
    prs::printTabPlusOne("organs",    inst.organs.size());

#ifdef WITH_OSQP
    if (is_bao) {
        if (K <= 0 || K > inst.n_angles) {
            std::cerr << "[BAO] K=" << K << " invalido para "
                      << inst.n_angles << " angulos\n";
            exit(-1);
        }
        prs::printTab("BAO problem (OSQP-exact FMO + busqueda de angulos)");
        prs::printTabPlusOne("K (angulos activos)", K);

        emili::imrt::BaoProblem* prob = new emili::imrt::BaoProblem(inst, K);
        if (!prob->isReady()) {
            std::cerr << "[BAO] OSQP setup failed\n";
            exit(-1);
        }

        if (tm.checkToken(OPT_VERBOSE)) {
            prob->setVerbose(true);
            prs::printTab("modo verbose BAO activado");
        }
        if (tm.checkToken(OPT_CSV)) {
            char* path = tm.nextToken();
            if (path) {
                prob->openCsvLog(std::string(path));
                prs::printTabPlusOne("CSV log", path);
            }
        }
        return prob;
    }
#endif

    // Classic FMO problem
    prs::printTab("IMRT problem loaded");
    emili::imrt::ImrtProblem* prob = new emili::imrt::ImrtProblem(inst);

    if (tm.checkToken(OPT_NACTIVE)) {
        int k = tm.getInteger();
        prob->setActiveAngles(k);
        prs::printTabPlusOne("angulos activos", k);
    }
    if (tm.checkToken(OPT_VERBOSE)) {
        prob->setVerboseFmo(true);
        prs::printTab("modo verbose FMO activado");
    }
    return prob;
}

/*---------------------------------------------------------------------------*
 * buildInitialSolution
 *---------------------------------------------------------------------------*/

emili::InitialSolution* ImrtBuilder::buildInitialSolution()
{
    prs::incrementTabLevel();
    emili::InitialSolution* init = nullptr;

#ifdef WITH_OSQP
    if (isBaoProblem()) {
        emili::imrt::BaoProblem* prob = castBaoProblem();
        if (tm.checkToken(INIT_FIRSTK)) {
            prs::printTab("BAO initial solution: first K angles");
            init = new emili::imrt::FirstKAnglesInit(*prob);
        }
        else if (tm.checkToken(INIT_RANDOMK)) {
            prs::printTab("BAO initial solution: random K angles");
            init = new emili::imrt::RandomKAnglesInit(*prob);
        }
        prs::decrementTabLevel();
        return init;
    }
#endif

    emili::imrt::ImrtProblem* prob = castProblem();

    if (tm.checkToken(INIT_ZERO)) {
        prs::printTab("initial solution: zero");
        init = new emili::imrt::ZeroInitialSolution(*prob);
    }
    else if (tm.checkToken(INIT_UNIFORM)) {
        double x = tm.getDecimal();
        prs::printTab("initial solution: uniform");
        prs::printTabPlusOne("intensity", x);
        init = new emili::imrt::UniformInitialSolution(*prob, x);
    }
    else if (tm.checkToken(INIT_RANDOM)) {
        double max = tm.getDecimal();
        prs::printTab("initial solution: random");
        prs::printTabPlusOne("max_intensity", max);
        init = new emili::imrt::RandomInitialSolution(*prob, max);
    }

    prs::decrementTabLevel();
    return init;
}

/*---------------------------------------------------------------------------*
 * buildNeighborhood
 *---------------------------------------------------------------------------*/

emili::Neighborhood* ImrtBuilder::buildNeighborhood()
{
    prs::incrementTabLevel();
    emili::Neighborhood* neigh = nullptr;

#ifdef WITH_OSQP
    if (isBaoProblem()) {
        emili::imrt::BaoProblem* prob = castBaoProblem();
        if (tm.checkToken(NEIGH_ANGSWAP)) {
            prs::printTab("BAO neighborhood: angle swap");
            neigh = new emili::imrt::AngleSwapNeighborhood(*prob);
        }
        prs::decrementTabLevel();
        return neigh;
    }
#endif

    emili::imrt::ImrtProblem* prob = castProblem();

    if (tm.checkToken(NEIGH_SHIFT)) {
        double delta = tm.getDecimal();
        prs::printTab("neighborhood: single beamlet shift");
        prs::printTabPlusOne("delta", delta);
        neigh = new emili::imrt::SingleBeamletShift(*prob, delta);
    }
    else if (tm.checkToken(NEIGH_SWAP)) {
        prs::printTab("neighborhood: beamlet swap");
        neigh = new emili::imrt::BeamletSwap(*prob);
    }

    prs::decrementTabLevel();
    return neigh;
}

/*---------------------------------------------------------------------------*
 * buildPerturbation
 *---------------------------------------------------------------------------*/

emili::Perturbation* ImrtBuilder::buildPerturbation()
{
    prs::incrementTabLevel();
    emili::Perturbation* pert = nullptr;

#ifdef WITH_OSQP
    if (isBaoProblem()) {
        emili::imrt::BaoProblem* prob = castBaoProblem();
        if (tm.checkToken(PERT_ANGSWAP)) {
            int p = tm.getInteger();
            prs::printTab("BAO perturbation: random angle swap");
            prs::printTabPlusOne("swaps", p);
            pert = new emili::imrt::RandomAnglesPerturbation(*prob, p);
        }
        else if (tm.checkToken(PERT_GREEDY)) {
            int D = tm.getInteger();
            prs::printTab("BAO perturbation: greedy construction (Iterated Greedy)");
            prs::printTabPlusOne("D (destroy/rebuild)", D);
            pert = new emili::imrt::GreedyAnglesPerturbation(*prob, D);
        }
        prs::decrementTabLevel();
        return pert;
    }
#endif

    emili::imrt::ImrtProblem* prob = castProblem();

    if (tm.checkToken(PERT_RANDOM)) {
        int    k   = tm.getInteger();
        double max = tm.getDecimal();
        prs::printTab("perturbation: random beamlet");
        prs::printTabPlusOne("k", k);
        prs::printTabPlusOne("max_intensity", max);
        pert = new emili::imrt::RandomBeamletPerturbation(*prob, k, max);
    }

    prs::decrementTabLevel();
    return pert;
}

/*---------------------------------------------------------------------------*
 * buildAcceptance
 *---------------------------------------------------------------------------*/

emili::Acceptance* ImrtBuilder::buildAcceptance()
{
    prs::incrementTabLevel();
    emili::Acceptance* acc = nullptr;

    if (tm.checkToken(ACC_IMPROVE)) {
        prs::printTab("acceptance: improve");
        acc = new emili::imrt::ImrtImproveAccept();
    }

    prs::decrementTabLevel();
    return acc;
}

/*---------------------------------------------------------------------------*
 * buildTermination
 *---------------------------------------------------------------------------*/

emili::Termination* ImrtBuilder::buildTermination()
{
    prs::incrementTabLevel();
    emili::Termination* term = nullptr;

    if (tm.checkToken(TERM_MAXITER)) {
        int n = tm.getInteger();
        prs::printTab("termination: max iterations");
        prs::printTabPlusOne("max", n);
        term = new emili::imrt::ImrtMaxIterations(n);
    }
    else if (tm.checkToken(TERM_FEASIBLE)) {
        prs::printTab("termination: feasible solution found");
        term = new emili::imrt::ImrtFeasibleTermination();
    }

    prs::decrementTabLevel();
    return term;
}

/*---------------------------------------------------------------------------*
 * buildTabuTenure
 *---------------------------------------------------------------------------*/

emili::TabuMemory* ImrtBuilder::buildTabuTenure()
{
    prs::incrementTabLevel();
    emili::TabuMemory* mem = nullptr;

    if (tm.checkToken(TABU_ALL_SOL) ||
        tm.checkToken(TABU_1_CAR)   ||
        tm.checkToken(TABU_1_POS))
    {
        int tenure = tm.getInteger();
        prs::printTab("tabu memory: solution vector (circular buffer)");
        prs::printTabPlusOne("tenure", tenure);
        mem = new emili::imrt::ImrtTabuMemory(tenure);
    }
#ifdef WITH_OSQP
    else if (tm.checkToken(TABU_BAO_FIXED)) {
        int tenure = tm.getInteger();
        prs::printTab("BAO tabu memory: fixed tenure on angle sets");
        prs::printTabPlusOne("tenure", tenure);
        mem = new emili::imrt::BaoTabuMemory(tenure);
    }
    else if (tm.checkToken(TABU_BAO_ADAPTIVE)) {
        int tmin = tm.getInteger();
        int tmax = tm.getInteger();
        prs::printTab("BAO tabu memory: adaptive tenure on angle sets");
        prs::printTabPlusOne("tenure_min", tmin);
        prs::printTabPlusOne("tenure_max", tmax);
        mem = new emili::imrt::AdaptiveBaoTabuMemory(tmin, tmax);
    }
#endif

    prs::decrementTabLevel();
    return mem;
}

/*---------------------------------------------------------------------------*
 * buildShake — rVNS multi-scale shake for BAO
 *---------------------------------------------------------------------------*/

emili::Shake* ImrtBuilder::buildShake()
{
    prs::incrementTabLevel();
    emili::Shake* sh = nullptr;

#ifdef WITH_OSQP
    if (isBaoProblem()) {
        emili::imrt::BaoProblem* prob = castBaoProblem();
        if (tm.checkToken(SHAKE_BANGSHAKE)) {
            int p_max = tm.getInteger();
            prs::printTab("BAO shake: multi-scale angle swap (rVNS)");
            prs::printTabPlusOne("p_max", p_max);
            sh = new emili::imrt::MultiScaleAngleShake(*prob, p_max);
        }
        prs::decrementTabLevel();
        return sh;
    }
#endif

    prs::decrementTabLevel();
    return sh;
}

} // namespace imrt
} // namespace prs
