include "fzn_arg_val_int.mzn";

/** @group globals
	Returns the index of the value \a y in the array \a x.
	When breaking ties the least index is returned.
*/
function var $$E: arg_val(array [$$E] of var $$V: x, var $$V: v) =
	let { constraint length(x) > 0; } in arg_val_total(x, v);

function var $$E: arg_val_total(array [$$E] of var $$V: x, var $$V: v) :: promise_total =
	if length(x) = 0 then
		min(index_set(x))
	else
		let {
			var min(index_set(x)) .. max(index_set(x)): i;
			constraint fzn_arg_val_int(x, v, i);
		} in i
	endif;
