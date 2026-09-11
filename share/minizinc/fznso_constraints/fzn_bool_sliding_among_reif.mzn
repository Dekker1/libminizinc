% The constraint `bool_sliding_among_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `bool_sliding_among_reif` and
% the body below is dropped in favour of the propagator, or ship a file of this
% name with a decomposition of your own.

include "fzn_int_sliding_sum_reif.mzn";

predicate fzn_bool_sliding_among_reif(
  list of var bool: xs,
  var bool: value,
  int: window,
  int: lower_bound,
  int: upper_bound,
  var bool: b,
) =
  fzn_int_sliding_sum_reif(
    [bool2int(xi = value) | xi in xs],
    window,
    lower_bound,
    upper_bound,
    b,
  );
