% The constraint `graph_connected_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `graph_connected_reif` and the body
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

predicate fzn_graph_connected_reif(
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
    array [1..2 * length(edges)] of int: ufrom = from ++ to;
    array [1..2 * length(edges)] of int: uto = to ++ from;
    array [1..2 * length(edges)] of var bool: ues = es ++ es;
    % The least selected node, which is a function of the selection. Any
    % selected node reaches the rest exactly when this one does, because an
    % undirected walk runs both ways.
    array [NODES] of var bool: first;
    constraint
      forall (v in NODES) (
        first[v] <-> (ns[v] /\ forall (u in NODES where u < v) (not ns[u]))
      );
    array [NODES] of var bool: reached;
    constraint fznso_graph_reached(ufrom, uto, first, ues, reached);
  } in b <-> (exists (v in NODES) (ns[v]) /\ fznso_graph_kept(from, to, ns, es) /\
      forall (v in NODES) (ns[v] -> reached[v]));
