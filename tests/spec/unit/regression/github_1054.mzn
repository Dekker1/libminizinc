/***
!Test
solvers: [gecode]
options:
  -Werror: true
expected: !Error
  type: MiniZincError
***/

% Regression test for #1054: warnings raised by the parser must also be turned
% into errors by -Werror.

predicate foo = true;

constraint foo();
solve satisfy;
