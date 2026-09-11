% `set_of_int_array_element` for a solver with no set variables.
%
% `std/nosets.mzn`'s `array_var_set_element`, but for the name, with the
% registry's offset rebased on the way in. It already names its result, so
% unlike the set operations beside it there was nothing to reshape.
%
% One thing did have to change. In FlatZinc the index of an element constraint
% is already known to be a position of the array, and `std` reads the bounds of
% its own auxiliary arrays straight off that index's domain. The registry takes
% the index unbounded, so those bounds are taken over the array's own positions
% here. Otherwise the auxiliary arrays reach past the end of `xs`, the access is
% undefined, and the whole constraint comes out false.

include "fznso_set_to_bools.mzn";

predicate fzn_set_of_int_array_element(
  list of var set of int: xs,
  int: offset,
  var int: index,
  var set of int: value,
) =
  fznso_nosets_element(index - offset + 1, array1d(xs), value);

predicate fznso_nosets_element(var int: idx, array [int] of var set of int: xs, var set of int: y)
  :: promise_total =
  let {
    % The positions `idx` can still take: an index outside them fails the
    % constraint, which is what an out-of-range index means here.
    set of int: POS = index_set(xs) intersect dom(idx);
    int: lp = min(POS);
    int: up = max(POS);
    constraint idx in {i | i in index_set(xs) where lb(xs[i]) subset ub(y) /\ lb(y) subset ub(xs[i])};
    set of int: sUB = array_union([ub(xs[i]) | i in POS]);
    set of int: sLB = array_intersect([lb(xs[i]) | i in POS]);
    constraint y subset sUB;
    constraint sLB subset y;
  } in forall (k in ub(y)) (
    set2bools(y)[k] ==
      if k in sUB then
        if k in sLB then
          true
        else
          array1d(
            lp..up,
            [if k in ub(xs[i]) then set2bools(xs[i])[k] else false endif | i in lp..up],
          )[idx]
        endif
      else
        false
      endif
  ) /\
    forall (
      k in sUB diff ub(y),
    ) (
      if k in sLB then
        false %% fail the constraint
      else
        not array1d(
          lp..up,
          [if k in ub(xs[i]) then set2bools(xs[i])[k] else false endif | i in lp..up],
        )[idx]
      endif
    );

