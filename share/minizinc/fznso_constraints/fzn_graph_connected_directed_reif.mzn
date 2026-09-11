% The constraint `graph_connected_directed_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `graph_connected_directed_reif` and the body
% below is dropped in favour of the propagator, or ship a file of this name with
% a decomposition of your own.

% The nodes are numbered by `nodes`'s index set, which a FlatZinc array cannot
% carry, so the index it is numbered from travels alongside as a plain `int`.
% `from` and `to` hold node numbers in that same numbering.
%
% The plain form walks the subgraph with a distance and a parent per node, which
% a reified form cannot use: those are free, so the solver falsifies the body by
% choosing them badly. `fznso_graph_reach.mzn` states reachability as a function
% of the selected edges instead, and that is what `b` reifies.

include "fznso_graph_reach.mzn";

predicate fzn_graph_connected_directed_reif(
  list of int: from,
  list of int: to,
  int: node_offset,
  list of var bool: nodes,
  list of var bool: edges,
  var bool: b,
) =
  let {
    set of int: NODES = node_offset..node_offset + length(nodes) - 1;
    array [NODES] of var bool: ns = array1d(NODES, nodes);
    array [1..length(edges)] of var bool: es = array1d(1..length(edges), edges);
    % One walk per candidate root, because the constraint asks only that
    % *some* selected node reaches the rest, and which one is not a function of
    % the selection. A node per node per layer, paid only where a model reifies
    % this constraint.
    array [NODES, NODES] of var bool: reach;
    constraint
      forall (r in NODES) (
        fznso_graph_reached(
          from,
          to,
          array1d(NODES, [v = r | v in NODES]),
          es,
          array1d(NODES, [reach[r, v] | v in NODES]),
        )
      );
  } in b <-> (fznso_graph_kept(from, to, ns, es) /\
      exists (r in NODES) (ns[r] /\ forall (v in NODES) (ns[v] -> reach[r, v])));
