/***
  Not a solution, though one does exist.

  7x = 1400 is solved by x = 200, which a range of +/-100 cannot hold.  The
  model comes back satisfiable, but with a flag raised, so the answer carries
  no information: a solution may exist for a solver with wider bounds -- here
  one does.  A solver given a plain `var int` clamped to this range would have
  answered UNSAT, which would have been wrong.

  Note that 7 * x leaves the range even though x itself would fit, which is
  why the verdict tracks intermediate results and not just declared variables.
***/
include "../unbounded.mzn";
SLV_MIN = -100;
SLV_MAX = 100; % pretend the solver is this small

Unbounded: x;
constraint unb_var(x);

constraint 7 * x = 1400;

solve satisfy;
output ["x = " ++ show_unbounded(x) ++ "\n", unb_status(["x"], [x], unb_inexact) ++ "\n"];
