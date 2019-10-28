/*
% FlatZinc built-in redefinitions for linear solvers.
%
% AUTHORS
% Sebastian Brand
% Gleb Belov
*/

%-----------------------------------------------------------------------------%
%
% Logic operations
% Use indicators for reifs (CPLEX)?  Seems weak.
%
%-----------------------------------------------------------------------------%

predicate bool_not(var bool: p, var bool: q) =
  bool_xor(p, q);

predicate bool_and(var bool: p, var bool: q, var bool: r) =
  array_bool_and( [p, q], r )

predicate bool_or(var bool: p, var bool: q, var bool: r) =
  array_bool_or( [p, q], r )

predicate bool_xor(var bool: p, var bool: q) =
  p + q = 1;

predicate bool_xor(var bool: x, var bool: y, var bool: z) =
  x <= y + z /\
  y <= x + z /\
  z <= x + y /\
  x + y + z <= 2

predicate bool_eq_reif(var bool: p, var bool: q, var bool: r) =
%%    trace(" bool_eq_reif: \(p), \(q), \(r) \n") /\
  if is_fixed(r) then   % frequent case
    if fix(r) = true then p = q else bool_not(p,q) endif
  elseif is_fixed(q) then
    if fix(q) = true then p = r
    else bool_not(p,r) endif
  elseif is_fixed(p) then
    if fix(p) = true then q = r
    else bool_not(q, r) endif
  else
    p + q <= r + 1 /\
    p + r <= q + 1 /\
    q + r <= p + 1 /\
    p + q + p >= 1
  endif;

predicate bool_ne_reif(var bool: p, var bool: q, var bool: r) =
  bool_xor(p, q, r);

predicate bool_le(var bool: p, var bool: q) =
    p <= q;

predicate bool_le_reif(var bool: x, var bool: y, var bool: z) =
  1 - x + y >= z /\
  1 - x <= z /\ y <= z % strong
  %% /\ 1 - x + y <= z * 2      not needed

predicate bool_lt(var bool: p, var bool: q) =
  not p /\ q;

predicate bool_lt_reif(var bool: p, var bool: q, var bool: r) =
  (not p /\ q) <-> r;

%-----------------------------------------------------------------------------%

%% Reified disjunction
predicate array_bool_or(array[int] of var bool: a, var bool: b) =
  if exists( i in index_set( a ) )( is_fixed(a[i]) /\ fix(a[i]) ) then
    b
  elseif is_fixed(b) then   % frequent case
    if fix(b) then
      sum(i in index_set(a)) (a[i]) >= 1       %% >=1 seems better for MIPDomains... 5.4.19
    else
      forall(i in index_set(a)) (not a[i])
    endif
  else
    sum(a) >= b /\
    forall(i in index_set(a)) (b >= a[i])        % strong
    %  sum(c) <= x * length(a)                     % weak
  endif;

%% Reified conjunction
predicate array_bool_and(array[int] of var bool: a, var bool: b) =
  if exists( i in index_set( a ) )( is_fixed(a[i]) /\ not fix(a[i]) ) then
    not b
  elseif is_fixed(b) then            % frequent case
    if fix(b) then
      forall(i in index_set(a))(a[i])
    else
      sum(a) <= length(a)-1
    endif
  else
    length(a) - sum(a) >=  1 - b /\
    forall(i in index_set(a)) (b <= a[i])        % strong
    % length(a) - sum(c) <= (1 - x) * length(a);     % weak
  endif;

% predicate array_bool_xor(array[int] of var bool: a) = .. sum(a) is odd ..
predicate array_bool_xor(array[int] of var bool: a) =
  let {
    var 0..(length(a)-1) div 2: m;
  } in sum(a) == 1 + 2 * m;

predicate bool_clause(array[int] of var bool: p, array[int] of var bool: n) =
  sum(p) - sum(n) + length(n) >= 1;

predicate bool_lin_eq(array[int] of int: c, array[int] of var bool: x, var int: d) :: promise_total =
  sum(i in index_set(x))(c[i]*x[i]) == d;

predicate bool_lin_eq_reif(array[int] of int: c, array[int] of var bool: x,
                          int: d, var bool: b) =                                %% No var int d, sorry
  aux_int_eq_iff_1(sum(i in index_set(x))( c[i]*x[i]), d, b);

    
