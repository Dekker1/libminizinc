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
    constraint bool_clause([], [x,y]) /\ bool_clause([x,y], []);
  } in y;

/***
  @groupdef builtins.arithmetic Arithmetic Builtins

  These builtins implement arithmetic operations.
*/

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
% ---------------
/** @group flatzinc.set Constrains \a x \( \in \) \a S */
function var bool: set_in(var int: x, var set of int: S) ::promise_total =
  let {
    var bool: b;
    constraint set_in_reif(x, S, b);
  } in b;
