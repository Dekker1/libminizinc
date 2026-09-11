% The constraint `int_mdd_nondeterministic_reif`, as the FZnSO registry defines
% it.
%
% This file is a solver's override point: declare
% `int_mdd_nondeterministic_reif` and the body below is dropped in favour of the
% propagator, or ship a file of this name with a decomposition of your own.

% Unlike the unreified form, this does not use the node-and-edge encoding of
% `fzn_int_mdd_nondeterministic.mzn`. That encoding leaves the edge variables
% free, and a reification around it can be falsified by picking edges that break
% the path, so `b` came out unconstrained. Here one variable per node says
% whether the true node is reachable from it under `xs`, which `xs` alone fixes,
% and the root's variable is what `b` reifies.

predicate fzn_int_mdd_nondeterministic_reif(
  list of var int: xs,
  int: nodes,
  list of int: level,
  int: edges,
  list of int: from,
  list of set of int: label,
  list of int: to,
  var bool: b,
) =
  let {
    array [0..nodes] of var bool: accepts;
  } in accepts[0] /\ % the true node accepts whatever reaches it
    forall (n in 1..nodes) (
      accepts[n] <->
        exists (e in 1..edges where from[e] = n) (
          xs[level[n]] in label[e] /\ accepts[to[e]]
        )
    ) /\
    (b <-> accepts[1]);
