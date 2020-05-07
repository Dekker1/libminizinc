/***
  @groupdef builtins.compare Comparison Builtins

  These builtins implement comparison operations.
*/

/** @group builtins.compare Return if \a x is equal to \a y */
function bool: op_equals($T: x, $T: y) = y = x;
/** @group builtins.compare Return if \a x is equal to \a y */
function var bool: op_equals(var bool: x, var bool: y) ::promise_total =
  let {
    var bool: b;
    constraint bool_eq_reif(x, y, b);
  } in b;
/** @group builtins.compare Return if \a x is equal to \a y */
function var bool: op_equals(var int: x, var int: y) ::promise_total =
  let {
    var bool: b;
    constraint int_eq_reif(x, y, b);
  } in b;

/** @group builtins.compare Return if \a x is less than or equal to \a y */
function bool: op_lessorequals($T: x, $T: y) = x <= y;
/** @group builtins.compare Return if \a x is less than or equal to \a y */
function var bool: op_lessorequals(var bool: x, var bool: y) ::promise_total =
  let {
    var bool: b;
    constraint bool_clause([y],[b,x]);
    constraint bool_clause([b,x],[]);
    constraint bool_clause([b],[y]);
  } in b;
/** @group builtins.compare Return if \a x is less than or equal to \a y */
function var bool: op_lessorequals(var int: x, var int: y) ::promise_total =
  let {
    var bool: b;
    constraint int_lin_le_reif([1,-1], [x,y], 0, b);
  } in b;

/** @group builtins.compare Return if \a x is less than or equal to \a y */
function bool: op_less($T: x, $T: y) = x <= y;
/** @group builtins.compare Return if \a x is less than or equal to \a y */
function var bool: op_less(var bool: x, var bool: y) ::promise_total =
  let {
    var bool: b;
    constraint bool_clause([],[b,x]);
    constraint bool_clause([y],[b]);
    constraint bool_clause([b,x],[y]);
  } in b;
/** @group builtins.compare Return if \a x is less than or equal to \a y */
function var bool: op_less(var int: x, var int: y) ::promise_total =
  let {
    var bool: b;
    constraint int_lin_le_reif([1,-1], [x,y], 0, b);
  } in b;

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

/***
  @groupdef builtins.arithmetic Arithmetic Builtins

  These builtins implement arithmetic operations.
*/

/** @group builtins.arithmetic Return \a -x */
function int: op_minus(int: x) = -x;
/** @group builtins.arithmetic Return \a x - \a y */
function var int: op_minus(var int: x) ::promise_total =
  let {
    var -ub(x)..-lb(x): y;
    constraint int_lin_eq([1,-1], [x,y], 0);
  } in y;

/** @group builtins.arithmetic Return \a x - \a y */
function int: op_minus(int: x, int: y) = x - y;
/** @group builtins.arithmetic Return \a x - \a y */
function var int: op_minus(var int: x,  var int: y) ::promise_total =
  let {
    var lb(x)-ub(y)..ub(x)-lb(y): z;
    constraint int_lin_eq([1,-1,-1], [x,y,z], 0);
  } in z;

/** @group builtins.arithmetic Return \a x + \a y */
function int: op_plus(int: x, int: y) = x + y;
/** @group builtins.arithmetic Return \a x + \a y */
function var int: op_plus(var int: x, var int: y) ::promise_total =
  let {
    var lb(x)+lb(y)..ub(x)+ub(y): z;
    constraint int_lin_eq([1,1,-1], [x,y,z], 0);
  } in z;

/** @group builtins.arithmetic Return \a x * \a y */
function int: op_times(int: x, int: y) = x * y;
/** @group builtins.arithmetic Return \a x * \a y */
function var int: op_times(int: x, var int: y) ::promise_total =
  let {
    var fix(x)*lb(y)..fix(x)*ub(y): z;
    constraint int_lin_eq([fix(x),-1], [y,z], 0);
  } in z;
/** @group builtins.arithmetic Return \a x * \a y */
function var int: op_times(var int: x, int: y) = op_times(y, x);
/** @group builtins.arithmetic Return \a x * \a y */
function var int: op_times(var int: x, var int: y) ::promise_total =
  let {
    var lb(x)*lb(y)..ub(x)*ub(y): z;
    constraint int_times(x, y, z);
  } in z;

/** @group builtins.arithmetic Return result of integer division \a x / \a y */
function int: op_int_division(int: x, int: y) = x div y;
/** @group builtins.arithmetic Return result of integer division \a x / \a y */
function var int: op_int_division(var int: x, var int: y) =
  if mzn_in_root_context(y) \/ not (0 in dom(y)) then div_t(x,y) else
  let { constraint y != 0 } in div_mt(x,y) endif;

/** @group builtins.arithmetic Return result of floating point division \a x / \a y */
function float: op_float_division(float: x, float: y) = x / y;
/** @group builtins.arithmetic Return result of floating point division \a x / \a y */
function var float: op_float_division(var float: x, var float: y) =
  if mzn_in_root_context(y) \/ lb(y) > 0.0 \/ ub(y) < 0.0 then fldiv_t(x,y) else
  let { constraint y != 0.0 } in fldiv_mt(x,y) endif;


function int: internal_max(array[$U] of int: x);
function bool: internal_max(array[$U] of bool: x);
/** @group builtins.arithmetic Return maximum of \a x and \a y */
function $T: max($T: x, $T: y) =
  if x > y then
    x
  else
    y
  endif;
/** @group builtins.arithmetic Return maximum of elements in set \a x */
function int: max(set of int: x) = internal_max(set2array(x));
/** @group builtins.arithmetic Return maximum of elements in array \a x */
function int: max(array[$U] of int: x) = internal_max(array1d(x));
/** @group builtins.arithmetic Return maximum of elements in array \a x */
function bool: max(array[$U] of bool: x) = internal_max(array1d(x));

function int: internal_min(array[$U] of int: x);
function bool: internal_min(array[$U] of bool: x);
/** @group builtins.arithmetic Return minimum of \a x and \a y */
function $T: min($T: x, $T: y) =
  if x < y then
    x
  else
    y
  endif;
function int: min(set of int: x) = internal_min(set2array(x));
function int: min(array[$U] of int: x) = internal_min(array1d(x));
function bool: min(array[$U] of bool: x) = internal_min(array1d(x));

% ---------------
/** @group flatzinc.set Constrains \a x \( \in \) \a S */
function var bool: op_in(var int: x, var set of int: S) ::promise_total =
  let {
    var bool: b;
    constraint set_in_reif(x, S, b);
  } in b;

/** @group flatzinc.set Constrains \a x \( \in \) \a S */
function var bool: op_in(var int: x, set of int: S) ::promise_total =
  if dom(x) subset S then
    true
  else
    let {
      var bool: b;
      constraint set_in_reif(x, S, b);
    } in b
  endif;

/** @group flatzinc.set Constrains \a x \( \in \) \a S */
function bool: op_in(int: x, set of int: S) =
  exists(i in S) (i == x);

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
