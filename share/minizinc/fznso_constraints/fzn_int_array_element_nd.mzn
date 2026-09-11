% The constraint `int_array_element_nd`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `int_array_element_nd` and the body
% below is dropped in favour of the propagator, or ship a file of this name with
% a decomposition of your own.

% `xs` holds the array flattened row-major, so the position the registry defines
% is the one computed here. Each index is held to its own dimension's range
% first: without that, an index past the end of one dimension would simply name
% a cell of the next, which the flat position alone cannot tell apart.

include "fzn_int_array_element.mzn";

predicate fzn_int_array_element_nd(
  list of var int: xs,
  list of int: offsets,
  list of int: sizes,
  list of var int: indices,
  var int: value,
) =
  let {
    set of int: DIMS = index_set(indices);
    int: last = max(DIMS);
  } in forall (d in DIMS) (indices[d] in offsets[d]..offsets[d] + sizes[d] - 1) /\
    fzn_int_array_element(
      xs,
      1,
      1 +
        sum (d in DIMS) (
          (indices[d] - offsets[d]) * product (e in d + 1..last) (sizes[e])
        ),
      value,
    );
