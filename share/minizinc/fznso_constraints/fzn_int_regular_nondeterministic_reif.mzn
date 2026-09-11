% The constraint `int_regular_nondeterministic_reif`, as the FZnSO registry
% defines it.
%
% This file is a solver's override point: declare
% `int_regular_nondeterministic_reif` and the body below is dropped in favour of
% the propagator, or ship a file of this name with a decomposition of your own.

% The plain form picks one state per position and asks that the path exist,
% which a reified form cannot do: the solver falsifies the body by picking a
% path that dies. Here each position carries the *set* of states the automaton
% can be in, one Boolean per state, and that set is fixed by `xs`. The string is
% accepted exactly when an accepting state survives to the end.

predicate fzn_int_regular_nondeterministic_reif(
  list of var int: xs,
  int: states,
  int: alphabet,
  int: alphabet_offset,
  list of set of int: transition,
  int: initial,
  set of int: accepting,
  var bool: b,
) =
  if length(xs) = 0 then
    b <-> initial in accepting
  else
    let {
      int: n = length(xs);
      set of int: symbols = alphabet_offset..alphabet_offset + alphabet - 1;
      array [1..states, symbols] of set of int: dd = array2d(1..states, symbols, transition);
      % `live[i, q]`: the automaton can be in state `q` after reading the first
      % `i - 1` symbols.
      array [1..n + 1, 1..states] of var bool: live;
      constraint forall (q in 1..states) (live[1, q] <-> q = initial);
      constraint
        forall (i in 1..n, q in 1..states) (
          live[i + 1, q] <->
            exists (p in 1..states, s in symbols where q in dd[p, s]) (
              live[i, p] /\ xs[i] = s
            )
        );
    } in b <->
      (
        forall (i in 1..n) (xs[i] in symbols) /\
          exists (q in accepting) (live[n + 1, q])
      )
  endif;
