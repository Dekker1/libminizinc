include "fzn_arg_min_float.mzn";

predicate minimum_arg_float(array[int] of var float: x, var int: i) =
    fzn_minimum_arg_float(x, i);

predicate minimum_arg_float_reif(array[int] of var float: x, var int: i, var bool: b) =
     b <-> i=arg_min(x);
