% The constraint `int_subcircuit_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `int_subcircuit_reif` and the
% body below is dropped in favour of the propagator, or ship a file of this name
% with a decomposition of your own.

% As `int_circuit_reif`, except that a node may sit out as a self loop. The
% nodes that move have to form one cycle, so the walk starts at the first of
% them, which the successors alone decide.

include "fznso_graph_reach.mzn";
include "fzn_int_all_different.mzn";
include "fzn_int_all_different_reif.mzn";

predicate fzn_int_subcircuit_reif(list of var int: xs, int: offset, var bool: b) =
  let {
    set of int: NODES = offset..offset + length(xs) - 1;
    array [NODES] of var int: x = array1d(NODES, xs);
    array [NODES] of var bool: moves = array1d(NODES, [x[v] != v | v in NODES]);
    array [NODES] of var bool: first;
    constraint
      forall (v in NODES) (
        first[v] <-> (moves[v] /\ forall (u in NODES where u < v) (not moves[u]))
      );
    array [1..length(xs) * length(xs)] of int: cfrom = [u | u in NODES, v in NODES];
    array [1..length(xs) * length(xs)] of int: cto = [v | u in NODES, v in NODES];
    % Only the arcs out of a node that moves, so a self loop is not a walk.
    array [1..length(xs) * length(xs)] of var bool: ces = [
      x[u] = v /\ moves[u] |
        u in NODES,
        v in NODES,
    ];
    array [NODES] of var bool: reached;
    constraint fznso_graph_reached(cfrom, cto, first, ces, reached);
    var bool: distinct;
    constraint fzn_int_all_different_reif(xs, distinct);
  } in b <->
    (
      distinct /\ forall (v in NODES) (x[v] in NODES) /\
        forall (v in NODES) (moves[v] -> reached[v])
    );
