% Reachability as a function of the selected edges, for the reified graph
% constraints.
%
% The plain forms introduce a distance and a parent per node, which a reified
% form cannot use: those variables are free, so the solver falsifies the body by
% choosing them badly. The walk below is a fixed point instead, one layer per
% node, and each layer is fixed by the seed and the selected edges alone. That
% is what makes `b <-> ...` sound here, and it is why the standard library's
% `_reif` files for these constraints abort rather than reify the plain body.
%
% The cost is one Boolean per node per layer. A reified graph constraint is rare
% and small where it appears, and a wrong answer costs more than the columns.

predicate fznso_graph_reached(
  array [int] of int: from,
  array [int] of int: to,
  array [int] of var bool: seed,
  array [int] of var bool: es,
  array [int] of var bool: reached,
) =
  let {
    set of int: NODES = index_set(seed);
    int: steps = card(NODES) - 1;
    array [0..steps, NODES] of var bool: walk;
  } in forall (v in NODES) (walk[0, v] <-> seed[v]) /\
    forall (k in 1..steps, v in NODES) (
      walk[k, v] <->
        (
          walk[k - 1, v] \/
            exists (e in index_set(from) where to[e] = v) (es[e] /\ walk[k - 1, from[e]])
        )
    ) /\
    forall (v in NODES) (reached[v] <-> walk[steps, v]);

% The selected edges keep their endpoints, which every graph constraint here
% asks for. Stated inline rather than through `fzn_graph_subgraph`, so that a
% reified body needs no reified form of it.
predicate fznso_graph_kept(
  array [int] of int: from,
  array [int] of int: to,
  array [int] of var bool: ns,
  array [int] of var bool: es,
) = forall (e in index_set(from)) (es[e] -> (ns[from[e]] /\ ns[to[e]]));
