/***
  Soundness check for unbounded.mzn.

  For every pair of concrete integers in a range reaching well past the
  solver's range on both sides, assert that each operation's relation admits
  the abstraction of the true result -- exactly the obligation the library
  rests on:

      for all a, b :  rel_op(alpha(a), alpha(b), alpha(a op b))

  Satisfiable means every relation held for every class combination.  UNSAT
  means one is wrong; narrow W or drop operators to find it.
***/
include "unbounded.mzn";

SLV_MIN = -40;                % deliberately lopsided
SLV_MAX = 25;

set of int: W = -60..60;      % reaches past the range on both sides

constraint forall (a, b in W) (
  unb_plus (alpha(a), alpha(b), alpha(a + b))
  /\ unb_minus(alpha(a), alpha(b), alpha(a - b))
  /\ unb_times(alpha(a), alpha(b), alpha(a * b))
  /\ unb_min  (alpha(a), alpha(b), alpha(min(a, b)))
  /\ unb_max  (alpha(a), alpha(b), alpha(max(a, b)))
  /\ (b != 0 -> unb_div(alpha(a), alpha(b), alpha(a div b))
             /\ unb_mod(alpha(a), alpha(b), alpha(a mod b)))
);

constraint forall (a in W) (
  unb_neg(alpha(a), alpha(-a))
  /\ unb_abs(alpha(a), alpha(abs(a)))
);

solve satisfy;
output ["sound over \(W) for \(SLV_MIN)..\(SLV_MAX)\n"];
