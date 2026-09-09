/***
  A real solution.

  Both variables, and every intermediate, stay inside the solver's range, so
  no flag is raised and the assignment reported here is an assignment of
  actual integers: x = 4, y = 3.  Nothing was approximated, and the answer
  means exactly what it would mean for a plain `var int` model.
***/
include "../unbounded.mzn";
SLV_MIN = -100; SLV_MAX = 100;   % pretend the solver is this small

Unbounded: x; Unbounded: y;
constraint unb_var(x) /\ unb_var(y);

constraint x - y = 1;
constraint 3 * x + 2 * y = 18;

solve satisfy;
output ["x = " ++ show_unbounded(x) ++ ", y = " ++ show_unbounded(y) ++ "\n",
        unb_status(["x", "y"], [x, y], unb_inexact) ++ "\n"];
