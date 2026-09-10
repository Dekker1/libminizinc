/***
  Not a solution, and this time none exists at all.

  2x = 2y + 1 asks for an even number to equal an odd one, so it is UNSAT over
  the integers.  The abstraction cannot see that: once both sides are merely
  "above the range", equality between them is left open on purpose, because
  concrete values in that class can be equal or unequal.

  Compare with 06.  Both come back satisfiable with a flag raised, and from
  the output the two are indistinguishable -- yet 06 has a solution and this
  one does not.  That is the honest reading: a raised flag means the
  abstraction did not decide, never that a solution exists.  Only a clean
  solution and UNSAT carry information.
***/
include "../unbounded.mzn";
SLV_MIN = -100;
SLV_MAX = 100; % pretend the solver is this small

Unbounded: x;
Unbounded: y;
constraint unb_var(x) /\ unb_var(y);

constraint 2 * x = 2 * y + 1;

solve satisfy;
output [
	"x = " ++ show_unbounded(x) ++ ", y = " ++ show_unbounded(y) ++ "\n",
	unb_status(["x", "y"], [x, y], unb_inexact) ++ "\n",
];
