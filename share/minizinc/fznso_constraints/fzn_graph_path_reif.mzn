% The constraint `graph_path_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `graph_path_reif` and the body
% below is dropped in favour of the propagator, or ship a file of this name with
% a decomposition of your own.

% The nodes are numbered by `nodes`'s index set, which a FlatZinc array cannot
% carry, so the index it is numbered from travels alongside as a plain `int`.
% `from` and `to` hold node numbers in that same numbering.
%
% The plain form orients each selected edge and hands the result to the directed
% path, and that orientation is a choice the solver makes, which a reified form
% cannot allow. Here the undirected walk fixes reachability, and the degrees say
% the rest: two edges at every selected node, one at the source and the sink.

include "fznso_graph_reach.mzn";

predicate fzn_graph_path_reif(
  list of int: from,
  list of int: to,
  int: node_offset,
  var int: source,
  var int: sink,
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
    % The least selected node, which the selection alone decides. An undirected
    % walk runs both ways, so any selected node reaches the rest exactly when
    % this one does.
    array [NODES] of var bool: first;
    constraint
      forall (v in NODES) (
        first[v] <-> (ns[v] /\ forall (u in NODES where u < v) (not ns[u]))
      );
    array [NODES] of var bool: reached;
    constraint fznso_graph_reached(ufrom, uto, first, ues, reached);
  } in b <->
    (
      fznso_graph_kept(from, to, ns, es) /\
        forall (v in NODES) (ns[v] -> reached[v]) /\
        forall (v in NODES) (
          ns[v] ->
            sum (e in index_set(from) where from[e] = v \/ to[e] = v) (bool2int(es[e])) =
              2 - bool2int(source = v) - bool2int(sink = v)
        )
    );
