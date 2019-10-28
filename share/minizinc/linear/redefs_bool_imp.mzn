include "redefs_lin_halfreifs.mzn";

% SIMPLE BOOLEAN LOGIC
% TODO: why not check "is_fixed(r)" everywhere??

predicate bool_eq_imp(var bool: x, var bool: y, var bool: z) =
  if is_fixed(z) then
    if fix(z) then x = y else true endif
  else
    x + z <= y + 1 /\ y + z <= x + 1
  endif;

predicate bool_ne_imp(var bool: p, var bool: q, var bool: r) =
  bool_xor_imp(p, q, r);

predicate bool_le_imp(var bool: x, var bool: y, var bool: z) =
  1 - x + y >= z;

predicate bool_lt_imp(var bool: p, var bool: q, var bool: r) =
  bool_and_imp(not p, q, r);

predicate bool_or_imp(var bool: p, var bool: q, var bool: r) =
  array_bool_or_imp([p,q], r);

predicate bool_and_imp(var bool: p, var bool: q, var bool: r) =
  array_bool_and_imp([p,q], r);

predicate bool_xor_imp(var bool: x, var bool: y, var bool: z) =
  x + y >= z /\ x + y + z <= 2;

% BOOLEAN ARRAY OPERATIONS
predicate array_bool_or_imp(array[int] of var bool: a, var bool: b) =
  if forall( i in index_set( a ) )( is_fixed(a[i]) /\ not fix(a[i]) ) then
    not b
  elseif exists( i in index_set( a ) )( is_fixed(a[i]) /\ fix(a[i]) ) then
    true
  else
    let {
      array[index_set(a)] of var bool: a1;
    } in forall(i in index_set(a)) (a1[i] -> a[i]) /\ sum(a1) = b
  endif;

predicate array_bool_and_imp(array[int] of var bool: a, var bool: b) =
  if is_fixed(b) then
    if fix(b) then
      forall(i in index_set(a))( a[i] )
    else
      true
    endif
  elseif forall( i in index_set( a ) )( is_fixed(a[i]) /\ fix(a[i]) ) then
    true
  else
    forall(i in index_set(a)) ( a[i] >= b )
  endif;

%% No var int d, sorry TODO: why not???
predicate bool_lin_eq_imp(array[int] of int: c, array[int] of var bool: x,
                          int: d, var bool: b) =
  aux_int_eq_if_1(sum(i in index_set(x))(c[i]*x[i]), d, b);
