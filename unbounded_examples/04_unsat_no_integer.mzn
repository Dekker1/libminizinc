/***
  Real UNSAT: 3x = 7 has no integer solution.

  In range this is ordinary arithmetic.  Outside it the refutation comes from
  the classes alone: 3 * x is above the range when x is and below it when x
  is, while 7 is inside -- and equality between different classes is forced
  false.  So the answer holds for every integer, not just small ones.
***/
include "../unbounded.mzn";
SLV_MIN = -100; SLV_MAX = 100;   % pretend the solver is this small

Unbounded: x;
constraint unb_var(x);

constraint 3 * x = 7;

solve satisfy;
output ["x = " ++ show_unbounded(x) ++ "\n", unb_status(["x"], [x], unb_inexact) ++ "\n"];
