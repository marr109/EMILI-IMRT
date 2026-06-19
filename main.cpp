
//  Created by Federico Pagnozzi on 28/11/14.
//  Copyright (c) 2014 Federico Pagnozzi. All rights reserved.
//  This file is distributed under the BSD 2-Clause License. See LICENSE.TXT
//  for details.

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <algorithm>
#include "generalParser.h"
#include "setup.h"
#include "imrt/imrt_builder.h"
#include "imrt/imrt.h"
#include "imrt/imrt_instance.h"
#ifdef WITH_OSQP
#include "imrt/imrt_bao.h"
#endif

int main(int argc, char *argv[])
{
    prs::emili_header();
    srand ( time(0) );
    clock_t time = clock();
    if (argc < 3 )
    {
        prs::info();
        return 1;
    }

    float pls = 0;
    emili::LocalSearch* ls;

    prs::GeneralParserE ps(argv, argc);
    prs::EmBaseBuilder emb(ps, ps.getTokenManager());
    prs::imrt::ImrtBuilder imrtb(ps, ps.getTokenManager());
    ps.addBuilder(&emb);
    ps.addBuilder(&imrtb);

    ls = ps.parseParams();

    if(ls != nullptr)
    {
        pls = ls->getSearchTime();
        emili::Solution* solution;
        std::cout << std::endl << "Searching..." << std::endl;
        if(pls > 0)
        {
            solution = ls->timedSearch(pls);
        }
        else
        {
            solution = ls->search();
        }
        if(!emili::get_print())
        {
            solution = ls->getBestSoFar();
            double time_elapsed = (double)(clock()-time)/CLOCKS_PER_SEC;
            double solval = solution->getSolutionValue();
            std::cout << "time : " << time_elapsed << std::endl;
            std::cout << "iteration counter : " << emili::iteration_counter() << std::endl;
            std::cerr << solution->getSolutionValue() << std::endl;
            std::cout << "Objective function value: " << std::fixed << solval << std::endl;
            std::cerr << std::fixed << solval << std::endl;
            std::cout << "Found solution: ";
            std::cout << solution->getSolutionRepresentation() << std::endl;
            std::cout << std::endl;

            // ── Clinical-style plan report (ICRU-83 metrics) ──────────────────
            emili::Problem* prob = &ls->getInitialSolution().getProblem();
#ifdef WITH_OSQP
            if (auto* bp = dynamic_cast<emili::imrt::BaoProblem*>(prob)) {
                auto* bs = dynamic_cast<emili::imrt::BaoSolution*>(solution);
                if (bs) {
                    std::vector<int> deg(bs->active_angles_.size());
                    for (size_t i = 0; i < deg.size(); ++i)
                        deg[i] = bp->getInstance().angles[bs->active_angles_[i]];
                    emili::imrt::reportPlan(bp->getInstance(), bs->intensities_,
                                            solval, deg, std::cout, "dvh.csv");
                }
            } else
#endif
            if (auto* ip = dynamic_cast<emili::imrt::ImrtProblem*>(prob)) {
                auto* is = dynamic_cast<emili::imrt::ImrtSolution*>(solution);
                if (is) {
                    const auto& angs = ip->getInstance().angles;
                    std::vector<int> deg(angs.begin(), angs.end());
                    emili::imrt::reportPlan(ip->getInstance(), is->getIntensities(),
                                            solval, deg, std::cout, "dvh.csv");
                }
            }
        }
        delete ls;
    }
}
