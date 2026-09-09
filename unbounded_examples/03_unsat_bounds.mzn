/***
  Real UNSAT: contradictory bounds.

  UNSAT here is a statement about the integers, not about the solver's range.
  Every class of x is refuted in turn: in range by ordinary arithmetic; below
  it because `x > 10` compares a below-range value against an in-range one and
  is forced false; above it because `x < 5` is forced false the same way.  No
  magnitude of x, however large, satisfies this model.
***/
include "../unbounded.mzn";
SLV_MIN = -100; SLV_MAX = 100;   % pretend the solver is this small

Unbounded: x;
constraint unb_var(x);

constraint x < 5;
constraint x > 10;

solve satisfy;
output ["x = " ++ show_unbounded(x) ++ "\n", unb_status(["x"], [x], unb_inexact) ++ "\n"];
