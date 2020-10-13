/***
  @groupdef builtins.logic Logical operations

  Logical operations are the standard operators of Boolean logic.
*/

/** @group builtins.logic Return truth value of the negation of \a x */
function bool: op_not(bool: x) = not x;
/** @group builtins.logic Return truth value of the negation of \a x */
function var bool: op_not(var bool: x) ::promise_total =
  let {
    var bool: y;
    constraint bool_not(x, y);
  } in y;

/** @group builtins.compare Return if array \a x is equal to array \a y */
function bool: op_equals(array[$T] of int: x,array[$T] of int: y) =
    let {
        array[int] of int: xx = array1d(x);
        array[int] of int: yy = array1d(y);
    } in
    assert(index_sets_agree(x,y), "array index sets do not match",
      forall (i in index_set(xx)) (xx[i]=yy[i])
    );

/** @group builtins.compare Return if array \a x is equal to array \a y */
function var bool: op_equals(array[$T] of var int: x, array[$T] of var int: y) =
    let {
        array[int] of var int: xx = array1d(x);
        array[int] of var int: yy = array1d(y);
    } in
    assert(index_sets_agree(x,y), "array index sets do not match",
      forall (i in index_set(xx)) (xx[i]=yy[i])
    );

/** @group builtins.compare Return if array \a x is equal to array \a y */
function bool: op_equals(array[$T] of bool: x, array[$T] of bool: y) =
    let {
        array[int] of bool: xx = array1d(x);
        array[int] of bool: yy = array1d(y);
    } in
    assert(index_sets_agree(x,y), "array index sets do not match",
      forall (i in index_set(xx)) (xx[i]=yy[i])
    );

/** @group builtins.compare Return if array \a x is equal to array \a y */
function var bool: op_equals(array[$T] of var bool: x,array[$T] of var bool: y) =
    let {
        array[int] of var bool: xx = array1d(x);
        array[int] of var bool: yy = array1d(y);
    } in
    assert(index_sets_agree(x,y), "array index sets do not match",
      forall (i in index_set(xx)) (xx[i]=yy[i])
    );


/***
  @groupdef builtins.arithmetic Arithmetic Builtins

  These builtins implement arithmetic operations.
*/

predicate int_minus(var int:x, var int:y, var int:z);

/** @group builtins.arithmetic Return \a -x */
function int: op_minus(int: x) ::promise_total = -x;
/** @group builtins.arithmetic Return \a x - \a y */
function var int: op_minus(var int: x) ::promise_total =
  let {
    var int: y;
    constraint int_minus(0,x,y);
  } in y;

/** @group builtins.arithmetic Return \a x - \a y */
function int: op_minus(int: x, int: y) ::promise_total = x - y;
/** @group builtins.arithmetic Return \a x - \a y */
function var int: op_minus(var int: x, int: y) ::promise_total =
  if y = 0 then
    x
  else
    let {
      var int: z;
      constraint int_minus(x,y,z);
    } in z
  endif;
/** @group builtins.arithmetic Return \a x - \a y */
function var int: op_minus(var int: x,  var int: y) ::promise_total =
  let {
    var int: z;
    constraint int_minus(x, y, z);
  } in z;
/** @group builtins.arithmetic Return \a x + \a y */
function int: op_plus(int: x, int: y) = x + y;
/** @group builtins.arithmetic Return \a x + \a y */
function var int: op_plus(int: x, var int: y) ::promise_total =
  if x = 0 then
    y
  else
    let {
      var int: z;
      constraint int_plus(x, y, z);
    } in z
  endif;
/** @group builtins.arithmetic Return \a x + \a y */
function var int: op_plus(var int: x, int: y) = op_plus(y, x);
/** @group builtins.arithmetic Return \a x + \a y */
function var int: op_plus(var int: x, var int: y) ::promise_total =
  let {
    var int: z;
    constraint int_plus(x, y, z);
  } in z;

/** @group builtins.arithmetic Return \a x * \a y */
function int: op_times(int: x, int: y) = x * y;
function var int: op_times(int: x, var int: y) ::promise_total =
  if x = 1 then
    y
  else
    let {
      var int: z;
      constraint int_times(x, y, z);
    } in z
  endif;
function var int: op_times(var int: x, int: y) = op_times(y, x);
/** @group builtins.arithmetic Return \a x * \a y */
function var int: op_times(var int: x, var int: y) ::promise_total =
  let {
    var int: z;
    constraint int_times(x, y, z);
  } in z;

/* Negated versions of FlatZinc builtins */
predicate int_lin_le_neg(array[int] of int: as, array[int] of var int: bs, int: c) =
  int_lin_le([-a | a in as], bs, -c-1);
predicate int_lin_eq_neg(array[int] of int: as, array[int] of var int: bs, int: c) =
  int_lin_ne(as, bs, c);

predicate pre_int_lin_eq(array[int] of int: as, array[int] of var int: bs, int: c) = if length(as)=0 then c=0 else int_lin_eq(as,bs,c) endif;

predicate pre_int_lin_eq_neg(array[int] of int: as, array[int] of var int: bs, int: c) = if length(as)=0 then c!=0 else int_lin_ne(as,bs,c) endif;

predicate pre_int_lin_le(array[int] of int: as, array[int] of var int: bs, int: c) = if length(as)=0 then 0<=c else int_lin_le(as,bs,c) endif;

predicate pre_int_lin_le_neg(array[int] of int: as, array[int] of var int: bs, int: c) = if length(as)=0 then 0>c else int_lin_le_neg(as,bs,c) endif;


/** @group builtins.arithmetic Return result of integer division \a x / \a y */
function int: op_int_division(int: x, int: y) = x div y;
/** @group builtins.arithmetic Return result of integer division \a x / \a y */
function var int: op_int_division(var int: x, var int: y) =
  if mzn_in_root_context(y) \/ not (0 in dom(y)) then div_t(x,y) else
  let { constraint y != 0 } in div_mt(x,y) endif;

/** @group builtins.arithmetic Return result of integer division \a x / \a y */
function int: op_modulus(int: x, int: y) = x mod y;
/** @group builtins.arithmetic Return result of integer division \a x / \a y */
function var int: op_modulus(var int: x, var int: y) =
  if mzn_in_root_context(y) \/ not (0 in dom(y)) then mod_t(x,y) else
  let { constraint y != 0 } in mod_mt(x,y) endif;

/** @group builtins.arithmetic Return result of floating point division \a x / \a y */
function float: op_float_division(float: x, float: y) = x / y;
/** @group builtins.arithmetic Return result of floating point division \a x / \a y */
function var float: op_float_division(var float: x, var float: y) =
  if mzn_in_root_context(y) \/ lb(y) > 0.0 \/ ub(y) < 0.0 then fldiv_t(x,y) else
  let { constraint y != 0.0 } in fldiv_mt(x,y) endif;


function int: internal_max(array[$U] of int: x);
function int: internal_set_max(set of int: x);
function bool: internal_max(array[$U] of bool: x);
/** @group builtins.arithmetic Return maximum of \a x and \a y */
function $T: max($T: x, $T: y) =
  if x > y then
    x
  else
    y
  endif;
/** @group builtins.arithmetic Return maximum of elements in set \a x */
function int: max(set of int: x) = internal_set_max(x);
/** @group builtins.arithmetic Return maximum of elements in array \a x */
function int: max(array[$U] of int: x) = internal_max(array1d(x));
/** @group builtins.arithmetic Return maximum of elements in array \a x */
function bool: max(array[$U] of bool: x) = internal_max(array1d(x));

function int: internal_min(array[$U] of int: x);
function int: internal_set_min(set of int: x);
function bool: internal_min(array[$U] of bool: x);
/** @group builtins.arithmetic Return minimum of \a x and \a y */
function $T: min($T: x, $T: y) =
  if x < y then
    x
  else
    y
  endif;
function int: min(set of int: x) = internal_set_min(x);
function int: min(array[$U] of int: x) = internal_min(array1d(x));
function bool: min(array[$U] of bool: x) = internal_min(array1d(x));

predicate int_sum(array[int] of var int: xs, var int: x);
/** @group builtins.arithmetic Return sum of elements in array \a x */
function var int: sum_cc(array[$T] of var int: x) ::promise_total =
  let {
    array[int] of var int: xs = array1d(x);
    var int: res;
    constraint int_sum(xs, res);
  } in res;

function int: abs(int: x) =
  if x<0 then
    -x
  else
    x
  endif;

% ---------------

/***
  @groupdef builtins.ifthenelse Conditionals

  These functions implement conditional (if-then-else-endif) constraints.
*/

/** @group builtins.ifthenelse Conditional constraint \(\{\a c[i]\land\not\exists \a c[1..i-1]\ \rightarrow\ y=x[i] \}\)

  This constraint is generated by the compiler for if-then-else expressions.
  The last entry in the \a c array is always the constant true, corresponding
  to the else case.
*/
function var int: if_then_else(array[int] of var bool: c, array[int] of int: x) ::promise_total =
  let {
    var dom_array(x): y;
    constraint fzn_if_then_else_int(c, x, y);
  } in y;

/** @group builtins.ifthenelse Conditional constraint \(\{\a c[i]\land\not\exists \a c[1..i-1]\ \rightarrow\ y=x[i] \}\)

  This constraint is generated by the compiler for if-then-else expressions.
  The last entry in the \a c array is always the constant true, corresponding
  to the else case.
*/
function var int: if_then_else(array[int] of var bool: c, array[int] of var int: x) ::promise_total =
  let {
    var dom_array(x): y;
    constraint fzn_if_then_else_var_int(c, x, y);
  } in y;

/** @group builtins.ifthenelse Conditional constraint \(\{\a c[i]\land\not\exists \a c[1..i-1]\ \rightarrow\ y=x[i] \}\)

  This constraint is generated by the compiler for if-then-else expressions.
  The last entry in the \a c array is always the constant true, corresponding
  to the else case.
*/
function var bool: if_then_else(array[int] of var bool: c, array[int] of bool: x) ::promise_total =
  let {
    var bool: y;
    constraint fzn_if_then_else_bool(c, x, y);
  } in y;

/** @group builtins.ifthenelse Conditional constraint \(\{\a c[i]\land\not\exists \a c[1..i-1]\ \rightarrow\ y=x[i] \}\)

  This constraint is generated by the compiler for if-then-else expressions.
  The last entry in the \a c array is always the constant true, corresponding
  to the else case.
*/
function var bool: if_then_else(array[int] of var bool: c, array[int] of var bool: x) ::promise_total =
  let {
    var bool: y;
    constraint fzn_if_then_else_var_bool(c, x, y);
  } in y;

/** @group builtins.ifthenelse Conditional partiality constraint

  This constraint is generated by the compiler for if-then-else expressions
  with potentially undefined cases.
  The last entry in the \a c array is always the constant true, corresponding
  to the else case.
  The \a d[i] variable represents whether case
  \p i is defined. Constrains that if \b is defined, then the selected case must be defined, and
  if the selected case is undefined, then \a b must be undefined.
*/
function var bool: if_then_else_partiality(array[int] of var bool: c, array[int] of var bool: d) ::promise_total =
  let {
    var bool: b;
    constraint fzn_if_then_else_partiality(c, d, b);
  } in b;



% Include solver-specific redefinitions for any FlatZinc built-ins.
%
include "fzn_if_then_else_int.mzn";
include "fzn_if_then_else_var_int.mzn";
include "fzn_if_then_else_bool.mzn";
include "fzn_if_then_else_var_bool.mzn";
include "fzn_if_then_else_partiality.mzn";
% Include interpreter definitions always to be included
include "mznasm_builtins.mzn";
