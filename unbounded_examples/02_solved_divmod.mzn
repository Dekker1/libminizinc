/***
  A real solution, through division and remainder.

  x div 7 = 2 and x mod 7 = 3 pin x to 17, comfortably inside the range, so
  again no flag is raised and this is a genuine integer solution.
***/
include "../unbounded.mzn";
SLV_MIN = -100;
SLV_MAX = 100; % pretend the solver is this small

Unbounded: x;
constraint unb_var(x);

constraint x div 7 = 2;
constraint x mod 7 = 3;

solve satisfy;
output ["x = " ++ show_unbounded(x) ++ "\n", unb_status(["x"], [x], unb_inexact) ++ "\n"];
