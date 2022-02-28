include "fzn_arg_min_bool.mzn";

predicate fzn_arg_val_int(array[int] of var int: x, var int: v, var int: i) =
	fzn_minimum_arg_bool([x[j] == v | j in index_set(x)], i);
