/***
!Test
solvers: [gecode]
expected: !Result
  status: SATISFIED
  solution: !Solution
    _output_item: "false true true"
***/

% Regression test for GitHub issue #1057. A function whose body compares an
% optional expression is copied into the par "output model" when its result is
% needed for output. Only the types in the copied body were made par, without
% re-resolving the declarations of the operators in it. The var version of `=`
% is a builtin without a body, so the copy kept no declaration at all and
% evaluation fell through to the plain bool comparison, which hit the absent
% literal and raised "unexpected absent literal". Making the body par has to
% re-resolve `=` to the par optional version, which has a body.

var bool: initial;
var bool: always;
constraint not initial /\ always;

predicate is_covered(bool: ev) = if initial then true else <> endif = ev;
predicate is_present(bool: ev) = if always then true else <> endif = ev;
predicate is_absent(opt bool: ev) = if initial then true else <> endif = ev;

var bool: a = is_covered(false);
var bool: b = is_present(true);
var bool: c = is_absent(<>);

output "\(a) \(b) \(c)";
