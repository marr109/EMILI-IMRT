#include <ampl/ampl.h>
#include <iostream>

int main()
{
    ampl::Environment env(AMPL_BIN_DIR);
    ampl::AMPL ampl(env);

    ampl.eval("var x >= 0; maximize obj: x; subject to c: x <= 10;");
    ampl.setOption("solver", "/Users/marrojasr/Documents/code/emili_imrt/ampl_gurobi/.venv/lib/python3.9/site-packages/ampl_module_gurobi/bin/gurobi");
    ampl.solve();

    std::cout << "solve_result: " << ampl.getValue("solve_result").str() << std::endl;
    std::cout << "x = " << ampl.getVariable("x").value() << std::endl;
    return 0;
}
