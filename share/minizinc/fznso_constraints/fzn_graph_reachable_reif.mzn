% The constraint `graph_reachable_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `graph_reachable_reif` and the
% body below is dropped in favour of the propagator, or ship a file of this name
% with a decomposition of your own.

% The reified form of the directed constraint is what this reduces to, so a
% solver that declares that one gets its propagator here too. The decomposition
% of the directed form introduces a distance and a parent per node, which cannot
% be reified, and the file for its reified form says so.

include "fzn_graph_reachable_directed.mzn";
include "fzn_graph_reachable_directed_reif.mzn";

predicate fzn_graph_reachable_reif(
  list of int: from,
  list of int: to,
  int: node_offset,
  var int: root,
  list of var bool: nodes,
  list of var bool: edges,
  var bool: b,
) =
  /* duplicate the edges so that we can use directed graph reachability */
  b <->
    fzn_graph_reachable_directed(
      from ++ to,
      to ++ from,
      node_offset,
      root,
      nodes,
      edges ++ edges,
    );
