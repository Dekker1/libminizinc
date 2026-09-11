% The constraint `graph_acyclic_directed_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `graph_acyclic_directed_reif` and the body
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

predicate fzn_graph_acyclic_directed_reif(
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
    % A walk seeded at each node's own successors: the node lies on a cycle
    % exactly when that walk comes back to it.
    array [NODES, NODES] of var bool: onwards;
    constraint
      forall (v in NODES) (
        fznso_graph_reached(
          from,
          to,
          array1d(
            NODES,
            [
              exists (e in index_set(from) where from[e] = v /\ to[e] = u) (es[e]) |
                u in NODES,
            ],
          ),
          es,
          array1d(NODES, [onwards[v, u] | u in NODES]),
        )
      );
  } in b <-> (fznso_graph_kept(from, to, ns, es) /\ forall (v in NODES) (not onwards[v, v]));
