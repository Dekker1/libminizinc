%-----------------------------------------------------------------------------%
% 'all_different' constrains an array of objects to be all different.
%
% Linear version: equality encoding; see e.g. [Refalo, CP 2000]
%
% For a given d in dom(x), at most one i with x_i = d can exist.
%-----------------------------------------------------------------------------%

include "domain_encodings.mzn";

predicate fzn_all_different_int(array[int] of var int: x) =
    if length(x)<=1 then true
    else
    let {
        array[int] of var 0..1: x_eq_d = eq_encode(x)
    } in (
%        my_trace("  all_different_int: x[" ++ show(index_set(x)) ++ "]\n") /\
        assert(index_set(x_eq_d) == comb_index_set(index_set(x), lb_array(x)..ub_array(x)), "Index sets do not match up") /\
        forall(d in lb_array(x)..ub_array(x))(
          sum(i in index_set(x))(x_eq_d[get_index(index_set(x), lb_array(x)..ub_array(x), i, d)]) <= 1
        )
    )
    endif;


%-----------------------------------------------------------------------------%
