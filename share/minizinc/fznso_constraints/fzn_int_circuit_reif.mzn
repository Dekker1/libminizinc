% The constraint `int_circuit_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `int_circuit_reif` and the
% body below is dropped in favour of the propagator, or ship a file of this name
% with a decomposition of your own.

% A FlatZinc array is one-based, so the index the caller numbers `xs` from is
% passed explicitly rather than left implicit in an `index_set` the solver never
% receives.
%
% The plain form orders the nodes along the cycle with a free array, which a
% reified form cannot use. Here the successor each node names is read as an arc,
% and `fznso_graph_reach.mzn` walks those arcs: one cycle covering every node is
% exactly a permutation in which the first node reaches all of them.

include "fznso_graph_reach.mzn";
include "fzn_int_all_different.mzn";
include "fzn_int_all_different_reif.mzn";

predicate fzn_int_circuit_reif(list of var int: xs, int: offset, var bool: b) =
  let {
    set of int: NODES = offset..offset + length(xs) - 1;
    array [NODES] of var int: x = array1d(NODES, xs);
    % The successor relation as arcs, one per ordered pair. Each is fixed by
    % `x`, which is what makes the walk below reifiable.
    array [1..length(xs) * length(xs)] of int: cfrom = [u | u in NODES, v in NODES];
    array [1..length(xs) * length(xs)] of int: cto = [v | u in NODES, v in NODES];
    array [1..length(xs) * length(xs)] of var bool: ces = [x[u] = v | u in NODES, v in NODES];
    array [NODES] of var bool: reached;
    constraint
      fznso_graph_reached(
        cfrom,
        cto,
        array1d(NODES, [v = min(NODES) | v in NODES]),
        ces,
        reached,
      );
    var bool: distinct;
    constraint fzn_int_all_different_reif(xs, distinct);
  } in b <->
    (
      distinct /\ forall (v in NODES) (x[v] in NODES) /\
        forall (v in NODES) (reached[v]) /\
        % A single node is a circuit only through its own self loop, and more
        % than one node has no self loop at all.
        (length(xs) = 1 \/ forall (v in NODES) (x[v] != v))
    );
