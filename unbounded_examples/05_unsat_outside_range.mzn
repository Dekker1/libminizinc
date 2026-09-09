/***
  Real UNSAT, refuted while x is outside the solver's range.

  This is the case the whole construction exists for.  Nothing bounds x, so
  the solver is free to place it above or below the range -- and the
  refutation still goes through, because the remainder relation knows that
  `x mod 2` comes back into range with magnitude below 2 whatever x is, so it
  cannot be both 0 and 1.  A solver handed a clamped `var int` would report
  UNSAT too, but only for its clamped range; here it covers every integer.
***/
include "../unbounded.mzn";
SLV_MIN = -100; SLV_MAX = 100;   % pretend the solver is this small

Unbounded: x;
constraint unb_var(x);

constraint x mod 2 = 0;
constraint x mod 2 = 1;

solve satisfy;
output ["x = " ++ show_unbounded(x) ++ "\n", unb_status(["x"], [x], unb_inexact) ++ "\n"];
