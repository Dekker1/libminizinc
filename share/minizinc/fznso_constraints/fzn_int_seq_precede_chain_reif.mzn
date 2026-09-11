% The constraint `int_seq_precede_chain_reif`, as the FZnSO registry defines it.
%
% This file is a solver's override point: declare `int_seq_precede_chain_reif` and the body
% below is dropped in favour of the propagator, or ship a file of this name with
% a decomposition of your own.

% The plain form's bookkeeping array is free, so a reification around its body
% is unsound. `fznso_precede.mzn` ties the same condition to a Boolean through an
% equivalence, outside the reification, and that Boolean is what `b` reifies.

include "fznso_precede.mzn";

predicate fzn_int_seq_precede_chain_reif(list of var int: xs, var bool: b) =
  let {
    var bool: ok;
    constraint fznso_int_seq_precede_holds(xs, ok);
  } in b <-> ok;
