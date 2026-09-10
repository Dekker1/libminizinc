/***
  @groupdef unbounded Unbounded integers

  A sound abstraction of an unbounded `var int` for solvers that can only
  represent a finite range of integers.

  A solver handed a `var int` with no domain either refuses it or silently
  clamps it to its own limits.  In the latter case an UNSAT answer only means
  "no solution exists inside the clamped range", which is not the question
  that was asked.

  This library replaces such a variable by an `Unbounded`: a value in
  `SLV_MIN..SLV_MAX` together with two flags saying the true value lies below
  or above that range.  Every operation posts a relation that is a *sound
  relaxation*: for all concrete `a`, `b`, the abstraction of `a op b`
  satisfies the relation applied to the abstractions of `a` and `b`.

  How to read an answer, from a single run:

    UNSAT                    no solution exists over the integers.  Not "none
                             in the solver's range" -- none at all.
    solution, no flag set    a real solution; every printed value is an actual
                             integer and means what it says.
    solution, some flag set  not a solution.  Some value had to leave the
                             solver's range, so a solution may exist for a
                             solver with wider bounds, or may not exist at all.
                             The abstraction did not decide.

  The flags are also raised by intermediate results, not just by declared
  variables: `7 * x = 1400` has no real solution in a range of +/-100 even
  though x itself would fit.  `unb_inexact` aggregates every such escape, so
  reading it is enough.

  The relations are as tight as the abstraction allows, so the solver cannot
  escape a constraint by declaring an operand out of range.  A few class
  combinations genuinely admit several results and only those are left open.

  Exactness note: every operation is exact when `SLV_MIN = -SLV_MAX`, which is
  the case for every solver in the tree.  When the range is lopsided, `*`,
  `div` and `mod` fall back to a sound but weaker relation for operands in the
  sliver beyond the symmetric core, because computing a threshold there would
  itself need an unrepresentable value.
***/

/***  @groupdef unbounded.setup Parameters  ***/

/** @group unbounded.setup Smallest integer the solver can represent. */
int: SLV_MIN;
/** @group unbounded.setup Largest integer the solver can represent. */
int: SLV_MAX;

constraint assert(SLV_MIN < 0 /\ SLV_MAX > 0, "unbounded.mzn: SLV_MIN < 0 < SLV_MAX is required");

/* The symmetric core of the range, where negation is always representable.
   Equal to the whole range when the range is symmetric. */
int: R = min(SLV_MAX, -SLV_MIN);

/* Lower bound on the product of two below-range values, or SLV_MAX+1 when
   that product cannot be represented at all.  Guarded so the par
   multiplication itself cannot overflow. */
int: LL_MIN = if SLV_MIN - 1 < -SLV_MAX then SLV_MAX + 1 else (SLV_MIN - 1) * (SLV_MIN - 1) endif;

/* Numerator for the quotient-magnitude bound when the dividend is below the
   range.  Capped so that dividing it can never leave the range. */
int: DIV_A_LESS = min(1 - SLV_MIN, 2 * SLV_MAX);

/* Largest magnitude the range holds, and the resulting bounds on a quotient
   whose divisor is above or below the range. */
int: DIV_ABS_MAX = max(SLV_MAX, -SLV_MIN);
int: DIV_BG = DIV_ABS_MAX div (SLV_MAX + 1);
int: DIV_BL = DIV_ABS_MAX div (1 - SLV_MIN);

/***  @groupdef unbounded.type The type  ***/

/** @group unbounded.type
  An integer that may lie outside what the solver can represent.

  Exactly one of three classes holds:
    * in range -- both flags false, the value is `v`;
    * below    -- `maybe_less`, the true value is `< SLV_MIN`, `v` pinned to `SLV_MIN`;
    * above    -- `maybe_more`, the true value is `> SLV_MAX`, `v` pinned to `SLV_MAX`.
*/
type Unbounded = record(var SLV_MIN..SLV_MAX: v, var bool: maybe_less, var bool: maybe_more);

/** @group unbounded.type True when \a x is below the range. */
predicate is_less(Unbounded: x) = x.maybe_less;
/** @group unbounded.type True when \a x is above the range. */
predicate is_more(Unbounded: x) = x.maybe_more;
/** @group unbounded.type True when the value of \a x is `x.v`. */
predicate in_range(Unbounded: x) = not x.maybe_less /\ not x.maybe_more;

/** @group unbounded.type
  Set when any value in the model -- declared or intermediate -- left the
  solver's range.  While it is false, every reported value is a real integer.
  Only implied, never forced false, so it errs towards reporting an escape.
*/
var bool: unb_inexact;

/** @group unbounded.type
  The class invariant.  Post this on every `Unbounded` declared directly
  rather than produced by an operation:
      Unbounded: x;
      constraint unb_var(x);
  Every `Unbounded` passes through here, which is also where an escape raises
  `unb_inexact`.
*/
predicate unb_var(Unbounded: x) =
	not (x.maybe_less /\ x.maybe_more) /\
		(x.maybe_less -> x.v = SLV_MIN) /\
		(x.maybe_more -> x.v = SLV_MAX) /\
		(not in_range(x) -> unb_inexact);

/** @group unbounded.type Structural equality -- same class and same value. */
predicate unb_same(Unbounded: x, Unbounded: y) =
	x.v = y.v /\ x.maybe_less = y.maybe_less /\ x.maybe_more = y.maybe_more;

/***  @groupdef unbounded.build Construction  ***/

/** @group unbounded.build The abstraction map: the `Unbounded` denoting \a a. */
function Unbounded: alpha(int: a) = (
	v: if a < SLV_MIN then SLV_MIN elseif a > SLV_MAX then SLV_MAX else a endif,
	maybe_less: a < SLV_MIN,
	maybe_more: a > SLV_MAX,
);

/** @group unbounded.build Lift a variable that is already inside the range. */
function Unbounded: unb(var SLV_MIN..SLV_MAX: a) = (v: a, maybe_less: false, maybe_more: false);

/***  @groupdef unbounded.rel The relations  ***/

/* Operation tags, used to keep CSE from merging distinct operations. */
int: UNB_ADD = 1;
int: UNB_SUB = 2;
int: UNB_MUL = 3;
int: UNB_DIV = 4;
int: UNB_MOD = 5;
int: UNB_MIN = 6;
int: UNB_MAX = 7;
int: UNB_NEG = 8;
int: UNB_ABS = 9;

/* True when \a a is inside the symmetric core, so that -a is representable.
   Always true for a symmetric solver range. */
predicate in_core(var int: a) = a >= -R /\ a <= R;

/* ---- addition ------------------------------------------------------- */

/* Both operands in range.  No auxiliary variable appears: `a + b > SLV_MAX`
   is a reified linear constraint over a and b against a constant, which is
   still open even though both operands are confined to the range. */
predicate unb_plus_ii(var int: a, var int: b, Unbounded: z) =
	(is_less(z) <-> a + b < SLV_MIN) /\
		(is_more(z) <-> a + b > SLV_MAX) /\
		(in_range(z) -> z.v = a + b);

/* In-range \a a plus something above the range.  The sum exceeds SLV_MAX + a,
   so it is never below the range, and stays in range only when a is negative
   enough.  Written shifted, because SLV_MAX + a would not be representable. */
predicate unb_plus_above(var int: a, Unbounded: z) =
	not is_less(z) /\ (in_range(z) -> z.v - a > SLV_MAX);

/* In-range \a a plus something below the range: the mirror image. */
predicate unb_plus_below(var int: a, Unbounded: z) =
	not is_more(z) /\ (in_range(z) -> z.v - a < SLV_MIN);

/** @group unbounded.rel `z` is a sound abstraction of `x + y`.

                y below      y in range     y above
    x below     below        below          anything
    x in range  below        exact          above
    x above     anything     above          above

  "anything" means nothing is posted: the sum really can be any integer.
*/
predicate unb_plus(Unbounded: x, Unbounded: y, Unbounded: z) =
	if in_range(x) then
		if in_range(y) then
			unb_plus_ii(x.v, y.v, z)
		elseif is_more(y) then
			unb_plus_above(x.v, z)
		else
			unb_plus_below(x.v, z)
		endif
	elseif is_more(x) then
		if in_range(y) then
			unb_plus_above(y.v, z)
		elseif is_more(y) then
			is_more(z)
		else
			true
		endif
	else
		if in_range(y) then
			unb_plus_below(y.v, z)
		elseif is_less(y) then
			is_less(z)
		else
			true
		endif
	endif;

/* ---- subtraction ---------------------------------------------------- */

/* Both operands in range; the same shape as unb_plus_ii. */
predicate unb_minus_ii(var int: a, var int: b, Unbounded: z) =
	(is_less(z) <-> a - b < SLV_MIN) /\
		(is_more(z) <-> a - b > SLV_MAX) /\
		(in_range(z) -> z.v = a - b);

/* In-range \a a minus something above the range: falls short of a - SLV_MAX. */
predicate unb_minus_above(var int: a, Unbounded: z) =
	not is_more(z) /\ (in_range(z) -> z.v - a < -SLV_MAX);

/* In-range \a a minus something below the range: exceeds a - SLV_MIN. */
predicate unb_minus_below(var int: a, Unbounded: z) =
	not is_less(z) /\ (in_range(z) -> z.v - a > -SLV_MIN);

/* Something above the range minus in-range \a b: exceeds SLV_MAX - b. */
predicate unb_above_minus(var int: b, Unbounded: z) =
	not is_less(z) /\ (in_range(z) -> z.v + b > SLV_MAX);

/* Something below the range minus in-range \a b: falls short of SLV_MIN - b. */
predicate unb_below_minus(var int: b, Unbounded: z) =
	not is_more(z) /\ (in_range(z) -> z.v + b < SLV_MIN);

/** @group unbounded.rel `z` is a sound abstraction of `x - y`.

                y below      y in range     y above
    x below     anything     below          below
    x in range  above        exact          below
    x above     above        above          anything

  "anything" means nothing is posted: the difference really can be any
  integer.
*/
predicate unb_minus(Unbounded: x, Unbounded: y, Unbounded: z) =
	if in_range(x) then
		if in_range(y) then
			unb_minus_ii(x.v, y.v, z)
		elseif is_more(y) then
			unb_minus_above(x.v, z)
		else
			unb_minus_below(x.v, z)
		endif
	elseif is_more(x) then
		if in_range(y) then
			unb_above_minus(y.v, z)
		elseif is_less(y) then
			is_more(z)
		else
			true
		endif
	else
		if in_range(y) then
			unb_below_minus(y.v, z)
		elseif is_more(y) then
			is_less(z)
		else
			true
		endif
	endif;

/* ---- negation and absolute value ------------------------------------ */

/** @group unbounded.rel `z` is a sound abstraction of `-x`.

  For a lopsided range the negation of an in-range value can itself leave the
  range, which is why the in-range case still needs the class conditions.
*/
predicate unb_neg(Unbounded: x, Unbounded: z) =
	if in_range(x) then
		(is_less(z) <-> x.v > -SLV_MIN) /\
			(is_more(z) <-> x.v < -SLV_MAX) /\
			(in_range(z) -> z.v + x.v = 0)
	elseif is_more(x) then
		not is_more(z) /\ (in_range(z) -> z.v < -SLV_MAX)
	else
		not is_less(z) /\ (in_range(z) -> z.v > -SLV_MIN)
	endif;

/** @group unbounded.rel `z` is a sound abstraction of `abs(x)`.

  `abs(x.v)` is never written: for a lopsided range it can exceed SLV_MAX and
  would need an unrepresentable auxiliary.  Splitting on the sign of x.v keeps
  every term in range.
*/
predicate unb_abs(Unbounded: x, Unbounded: z) =
	if in_range(x) then
		if x.v >= 0 then
			in_range(z) /\ z.v = x.v
		else
			(is_more(z) <-> x.v < -SLV_MAX) /\ not is_less(z) /\ (in_range(z) -> z.v + x.v = 0)
		endif
	elseif is_more(x) then
		is_more(z)
	else
		not is_less(z) /\ (in_range(z) -> z.v > -SLV_MIN)
	endif;

/* ---- multiplication -------------------------------------------------- */

/* Both operands in range.  The product is the one place a wide auxiliary
   would appear, so the class is decided by dividing a range endpoint by the
   operand rather than by forming the product.  Everything below stays inside
   SLV_MIN..SLV_MAX: |SLV_MAX div d| <= SLV_MAX and |SLV_MIN div d| <= -SLV_MIN
   for d >= 1, and clamping into the symmetric core keeps -ac representable. */
predicate unb_times_ii(var int: a, var int: b, Unbounded: z) =
	let {
		% both operands clamped into the core
		var -R..R: ac = max(-R, min(R, a));
		var -R..R: bc = max(-R, min(R, b));
		var bool: core = a = ac /\ b = bc; % true unless the range is lopsided
		var 1..R: d = max(1, max(bc, -bc)); % |b|, or 1 when b is zero
		var 0..SLV_MAX: hi = SLV_MAX div d; % largest a whose product fits
		var SLV_MIN..0: lo = SLV_MIN div d; % smallest such a
		var -R..R: am = if bc < 0 then -ac else ac endif; % fold b's sign into a
		var bool: pos = (a > 0 /\ b > 0) \/ (a < 0 /\ b < 0);
	} in (core -> (in_range(z) <-> (am >= lo /\ am <= hi))) /\
		(in_range(z) -> z.v = a * b) /\
		(is_more(z) <-> (not in_range(z) /\ pos)) /\
		(is_less(z) <-> (not in_range(z) /\ not pos));

/* In-range \a a times something outside the range.  A zero operand wins
   outright; otherwise the product is at least as large in magnitude as the
   out-of-range operand, and the signs decide the direction. */
predicate unb_times_io(var int: a, Unbounded: w, Unbounded: z) =
	if a = 0 then
		in_range(z) /\ z.v = 0
	elseif a > 0 then
		if is_more(w) then is_more(z) else is_less(z) endif
	else
		if is_more(w) then
			not is_more(z) /\ (in_range(z) -> z.v < -SLV_MAX)
		else
			not is_less(z) /\ (in_range(z) -> z.v > -SLV_MIN)
		endif
	endif;

/** @group unbounded.rel `z` is a sound abstraction of `x * y`.

                y below      y in range     y above
    x below     positive     by sign        below
    x in range  by sign      exact          by sign
    x above     below        by sign        above

  "by sign" means the class follows the signs of the two operands.
*/
predicate unb_times(Unbounded: x, Unbounded: y, Unbounded: z) =
	if in_range(x) then
		if in_range(y) then unb_times_ii(x.v, y.v, z) else unb_times_io(x.v, y, z) endif
	elseif in_range(y) then
		unb_times_io(y.v, x, z)
	elseif is_more(x) /\ is_more(y) then
		is_more(z)
	elseif is_less(x) /\ is_less(y) then
		% both below the range, so the product is positive and at least LL_MIN
		not is_less(z) /\ (in_range(z) -> z.v >= LL_MIN)
	else
		% one below and one above the range, so the product is far negative
		is_less(z)
	endif;

/* ---- division -------------------------------------------------------- */

/* Both operands in range.  |a div b| <= |a|, so the quotient can only leave
   the range from the sliver of a lopsided one. */
predicate unb_div_ii(var int: a, var int: b, Unbounded: z) =
	(in_range(z) -> z.v = a div b) /\
		(in_core(a) -> in_range(z)) /\
		(is_more(z) -> (a > 0 /\ b > 0) \/ (a < 0 /\ b < 0)) /\
		(is_less(z) -> (a > 0 /\ b < 0) \/ (a < 0 /\ b > 0));

/* In-range dividend \a a, divisor outside the range.  The quotient is bounded
   by \a bound and takes the sign of a, flipped when the divisor is below the
   range.  It is only guaranteed back in range when the bound fits, which for
   a lopsided range it need not. */
predicate unb_div_i_out(var int: a, int: bound, bool: negative_divisor, Unbounded: z) =
	(bound <= R -> in_range(z)) /\
		(
			if (a >= 0) = negative_divisor then
				not is_more(z) /\ (in_range(z) -> z.v <= 0 /\ z.v >= -bound)
			else
				not is_less(z) /\ (in_range(z) -> z.v >= 0 /\ z.v <= bound)
			endif
		);

/* Out-of-range dividend, in-range divisor \a b; the caller handles |b| <= 1.
   The sign of
   The quotient's sign follows from the classes; its magnitude is at least
   |x| div |b|, but only once |x| is known to exceed |b| -- for a lopsided
   range x may sit just past one end while b sits further past the other. */
predicate unb_div_out_i(Unbounded: x, var int: b, Unbounded: z) =
	let {
		var -R..R: bc = max(-R, min(R, b));
		var 0..R: d = max(bc, -bc); % |b|, clamped into the core
		var 2..max(2, R): d2 = max(2, d); % kept >= 2 so the bound fits
		var 0..R: qm = min(R, if is_more(x) then (SLV_MAX + 1) div d2 else DIV_A_LESS div d2 endif);
		var bool: qpos = (is_more(x) /\ b > 0) \/ (is_less(x) /\ b < 0);
		var bool: x_dominates =
			in_core(b) /\ d >= 2 /\ ((is_more(x) /\ b >= -SLV_MAX) \/ (is_less(x) /\ b <= -SLV_MIN));
	} in if qpos then
		not is_less(z) /\ (in_range(z) /\ x_dominates -> z.v >= qm)
	else
		not is_more(z) /\ (in_range(z) /\ x_dominates -> z.v <= -qm)
	endif;

/** @group unbounded.rel
  `z` is a sound abstraction of `x div y` (truncating).  Division by zero is
  left open rather than made false.
*/
predicate unb_div(
	Unbounded: x,
	Unbounded: y,
	Unbounded: z,
) =
	if in_range(y) /\ y.v = 0 then
		true % undefined, so nothing is claimed
	elseif in_range(x) /\ in_range(y) then
		unb_div_ii(x.v, y.v, z)
	elseif in_range(x) then
		if is_more(y) then
			unb_div_i_out(x.v, DIV_BG, false, z)
		else
			unb_div_i_out(x.v, DIV_BL, true, z)
		endif
	elseif in_range(y) then
		if y.v = 1 then
			unb_same(x, z)
		elseif y.v = -1 then
			unb_neg(x, z)
		else
			unb_div_out_i(x, y.v, z)
		endif
	else
		% both outside the range: the magnitude is unknown, the sign is not
		let {
			var bool: qpos = is_less(x) <-> is_less(y);
		} in if qpos then
			not is_less(z) /\ (in_range(z) -> z.v >= 0)
		else
			not is_more(z) /\ (in_range(z) -> z.v <= 0)
		endif
	endif;

/* ---- remainder ------------------------------------------------------- */

/* Both operands in range.  |a mod b| < |b|, so the remainder can only leave
   the range from the sliver of a lopsided one. */
predicate unb_mod_ii(var int: a, var int: b, Unbounded: z) =
	(in_range(z) -> z.v = a mod b) /\
		(in_core(b) -> in_range(z)) /\
		(is_more(z) -> a > 0) /\
		(is_less(z) -> a < 0);

/* In-range dividend \a a, divisor outside the range.  The remainder lies
   between 0 and a inclusive, so it is always back in range, and it equals a
   outright once |a| is below the magnitude the class of y guarantees. */
predicate unb_mod_i_out(var int: a, Unbounded: y, Unbounded: z) =
	in_range(z) /\
		(if a >= 0 then z.v >= 0 /\ z.v <= a else z.v <= 0 /\ z.v >= a endif) /\
		(is_more(y) /\ a >= -SLV_MAX -> z.v = a) /\
		(is_less(y) /\ a <= -SLV_MIN -> z.v = a);

/* Out-of-range dividend, in-range divisor \a b.  The remainder takes the sign
   of the dividend and is smaller in magnitude than b.  |b| is not written
   directly: for a lopsided range -b can exceed SLV_MAX, so the two signs of b
   are handled separately to keep every term in range. */
predicate unb_mod_out_i(Unbounded: x, var int: b, Unbounded: z) =
	(b >= -SLV_MAX /\ b <= -SLV_MIN -> in_range(z)) /\
		(
			if is_more(x) then
				not is_less(z) /\ (in_range(z) -> z.v >= 0 /\ (b > 0 -> z.v < b) /\ (b < 0 -> z.v + b < 0))
			else
				not is_more(z) /\ (in_range(z) -> z.v <= 0 /\ (b > 0 -> z.v + b > 0) /\ (b < 0 -> z.v > b))
			endif
		);

/** @group unbounded.rel
  `z` is a sound abstraction of `x mod y`.  Modulo zero is left open.
*/
predicate unb_mod(
	Unbounded: x,
	Unbounded: y,
	Unbounded: z,
) =
	if in_range(y) /\ y.v = 0 then
		true % undefined, so nothing is claimed
	elseif in_range(x) /\ in_range(y) then
		unb_mod_ii(x.v, y.v, z)
	elseif in_range(x) then
		unb_mod_i_out(x.v, y, z)
	elseif in_range(y) then
		unb_mod_out_i(x, y.v, z)
	else
		% both outside the range: only the sign of the dividend carries over
		if is_more(x) then
			not is_less(z) /\ (in_range(z) -> z.v >= 0)
		else
			not is_more(z) /\ (in_range(z) -> z.v <= 0)
		endif
	endif;

/* ---- minimum and maximum --------------------------------------------- */

/** @group unbounded.rel `z` is a sound abstraction of `min(x, y)`.  Always exact. */
predicate unb_min(Unbounded: x, Unbounded: y, Unbounded: z) =
	if is_less(x) \/ is_less(y) then
		is_less(z)
	elseif in_range(x) /\ in_range(y) then
		in_range(z) /\ z.v = min(x.v, y.v)
	elseif in_range(x) then
		unb_same(x, z) % y is above
	elseif in_range(y) then
		unb_same(y, z) % x is above
	else
		is_more(z)
	endif;

/** @group unbounded.rel `z` is a sound abstraction of `max(x, y)`.  Always exact. */
predicate unb_max(Unbounded: x, Unbounded: y, Unbounded: z) =
	if is_more(x) \/ is_more(y) then
		is_more(z)
	elseif in_range(x) /\ in_range(y) then
		in_range(z) /\ z.v = max(x.v, y.v)
	elseif in_range(x) then
		unb_same(x, z) % y is below
	elseif in_range(y) then
		unb_same(y, z) % x is below
	else
		is_less(z)
	endif;

/* ---- dispatch --------------------------------------------------------- */

/** @group unbounded.rel The relation for the binary operation named by \a tag. */
predicate unb_rel(int: tag, Unbounded: x, Unbounded: y, Unbounded: z) =
	if tag = UNB_ADD then
		unb_plus(x, y, z)
	elseif tag = UNB_SUB then
		unb_minus(x, y, z)
	elseif tag = UNB_MUL then
		unb_times(x, y, z)
	elseif tag = UNB_DIV then
		unb_div(x, y, z)
	elseif tag = UNB_MOD then
		unb_mod(x, y, z)
	elseif tag = UNB_MIN then
		unb_min(x, y, z)
	else
		unb_max(x, y, z)
	endif;

/** @group unbounded.rel The relation for the unary operation named by \a tag. */
predicate unb_rel1(int: tag, Unbounded: x, Unbounded: z) =
	if tag = UNB_NEG then unb_neg(x, z) else unb_abs(x, z) endif;

/***  @groupdef unbounded.ops Operations  ***/

/* Allocate the result of an operation.  `tag` is part of the call so that
   common subexpression elimination keeps distinct operations apart. */
function Unbounded: unb_bin(int: tag, Unbounded: x, Unbounded: y) :: promise_total =
	let {
		var SLV_MIN..SLV_MAX: rv;
		var bool: rl;
		var bool: rm;
		Unbounded: z = (v: rv, maybe_less: rl, maybe_more: rm);
		constraint unb_var(z);
		constraint unb_rel(tag, x, y, z);
	} in z;

function Unbounded: unb_un(int: tag, Unbounded: x) :: promise_total =
	let {
		var SLV_MIN..SLV_MAX: rv;
		var bool: rl;
		var bool: rm;
		Unbounded: z = (v: rv, maybe_less: rl, maybe_more: rm);
		constraint unb_var(z);
		constraint unb_rel1(tag, x, z);
	} in z;

/** @group unbounded.ops Sum of \a x and \a y. */
function Unbounded: '+'(Unbounded: x, Unbounded: y) = unb_bin(UNB_ADD, x, y);
/** @group unbounded.ops Difference of \a x and \a y. */
function Unbounded: '-'(Unbounded: x, Unbounded: y) = unb_bin(UNB_SUB, x, y);
/** @group unbounded.ops Product of \a x and \a y. */
function Unbounded: '*'(Unbounded: x, Unbounded: y) = unb_bin(UNB_MUL, x, y);
/** @group unbounded.ops Truncating quotient of \a x and \a y. */
function Unbounded: 'div'(Unbounded: x, Unbounded: y) = unb_bin(UNB_DIV, x, y);
/** @group unbounded.ops Remainder of \a x divided by \a y. */
function Unbounded: 'mod'(Unbounded: x, Unbounded: y) = unb_bin(UNB_MOD, x, y);
/** @group unbounded.ops Smaller of \a x and \a y. */
function Unbounded: min(Unbounded: x, Unbounded: y) = unb_bin(UNB_MIN, x, y);
/** @group unbounded.ops Larger of \a x and \a y. */
function Unbounded: max(Unbounded: x, Unbounded: y) = unb_bin(UNB_MAX, x, y);
/** @group unbounded.ops Negation of \a x.  MiniZinc has no user-defined unary operators. */
function Unbounded: neg(Unbounded: x) = unb_un(UNB_NEG, x);
/** @group unbounded.ops Absolute value of \a x. */
function Unbounded: abs(Unbounded: x) = unb_un(UNB_ABS, x);

function Unbounded: unb_fold(int: tag, array [int] of Unbounded: xs, int: i) =
	if i = max(index_set(xs)) then xs[i] else unb_bin(tag, xs[i], unb_fold(tag, xs, i + 1)) endif;

/** @group unbounded.ops Sum of \a xs. */
function Unbounded: sum(array [int] of Unbounded: xs) =
	if length(xs) = 0 then alpha(0) else unb_fold(UNB_ADD, xs, min(index_set(xs))) endif;
/** @group unbounded.ops Product of \a xs. */
function Unbounded: product(array [int] of Unbounded: xs) =
	if length(xs) = 0 then alpha(1) else unb_fold(UNB_MUL, xs, min(index_set(xs))) endif;

/***  @groupdef unbounded.cmp Comparison  ***/

/* Strictly less.

              y below      y in range     y above
    x below   either       true           true
    x in rng  false        exact          true
    x above   false        false          either

   The two "either" cells are left open on purpose: two values that are both
   below the range can be ordered any way round. */
predicate unb_lt(
	Unbounded: x,
	Unbounded: y,
)
	:: promise_total =
	let {
		var bool: r;
		constraint
			if in_range(x) then
				if in_range(y) then r <-> x.v < y.v elseif is_more(y) then r else not r endif
			elseif is_less(x) then
				if is_less(y) then true else r endif
			else
				if is_more(y) then true else not r endif
			endif;
	} in r;

/* Equal.  Exact in range, false across different classes, and open when both
   sides are on the same side of the range. */
predicate unb_eq(
	Unbounded: x,
	Unbounded: y,
)
	:: promise_total =
	let {
		var bool: r;
		constraint
			if in_range(x) then
				if in_range(y) then r <-> x.v = y.v else not r endif
			elseif is_less(x) then
				if is_less(y) then true else not r endif
			else
				if is_more(y) then true else not r endif
			endif;
	} in r;

/** @group unbounded.cmp \a x is strictly less than \a y. */
function var bool: '<'(Unbounded: x, Unbounded: y) = unb_lt(x, y);
/** @group unbounded.cmp \a x is strictly greater than \a y. */
function var bool: '>'(Unbounded: x, Unbounded: y) = unb_lt(y, x);
/** @group unbounded.cmp \a x is at most \a y. */
function var bool: '<='(Unbounded: x, Unbounded: y) = not unb_lt(y, x);
/** @group unbounded.cmp \a x is at least \a y. */
function var bool: '>='(Unbounded: x, Unbounded: y) = not unb_lt(x, y);
/** @group unbounded.cmp \a x equals \a y. */
function var bool: '='(Unbounded: x, Unbounded: y) = unb_eq(x, y);
/** @group unbounded.cmp \a x differs from \a y. */
function var bool: '!='(Unbounded: x, Unbounded: y) = not unb_eq(x, y);

/***  @groupdef unbounded.mixed Mixed with plain integers  ***/

function Unbounded: '+'(Unbounded: x, int: y) = x + alpha(y);
function Unbounded: '+'(int: x, Unbounded: y) = alpha(x) + y;
function Unbounded: '-'(Unbounded: x, int: y) = x - alpha(y);
function Unbounded: '-'(int: x, Unbounded: y) = alpha(x) - y;
function Unbounded: '*'(Unbounded: x, int: y) = x * alpha(y);
function Unbounded: '*'(int: x, Unbounded: y) = alpha(x) * y;
function Unbounded: 'div'(Unbounded: x, int: y) = x div alpha(y);
function Unbounded: 'div'(int: x, Unbounded: y) = alpha(x) div y;
function Unbounded: 'mod'(Unbounded: x, int: y) = x mod alpha(y);
function Unbounded: 'mod'(int: x, Unbounded: y) = alpha(x) mod y;
function var bool: '<'(Unbounded: x, int: y) = unb_lt(x, alpha(y));
function var bool: '<'(int: x, Unbounded: y) = unb_lt(alpha(x), y);
function var bool: '>'(Unbounded: x, int: y) = unb_lt(alpha(y), x);
function var bool: '>'(int: x, Unbounded: y) = unb_lt(y, alpha(x));
function var bool: '<='(Unbounded: x, int: y) = not unb_lt(alpha(y), x);
function var bool: '<='(int: x, Unbounded: y) = not unb_lt(y, alpha(x));
function var bool: '>='(Unbounded: x, int: y) = not unb_lt(x, alpha(y));
function var bool: '>='(int: x, Unbounded: y) = not unb_lt(alpha(x), y);
function var bool: '='(Unbounded: x, int: y) = unb_eq(x, alpha(y));
function var bool: '='(int: x, Unbounded: y) = unb_eq(alpha(x), y);
function var bool: '!='(Unbounded: x, int: y) = not unb_eq(x, alpha(y));
function var bool: '!='(int: x, Unbounded: y) = not unb_eq(alpha(x), y);

/***  @groupdef unbounded.out Output  ***/

/** @group unbounded.out
  The verdict for a solution.  Pass the global `unb_inexact` as \a inexact --
  it must come in as an argument because an output item may only call
  functions of its own parameters.
*/
function string: unb_status(
	array [int] of string: names,
	array [int] of Unbounded: xs,
	var bool: inexact,
) =
	assert(
		index_set(names) = index_set(xs),
		"unb_status: one name per variable",
		let {
			array [int] of string: escaped = [
				names[i] ++ (if fix(xs[i].maybe_less) then " < \(SLV_MIN)" else " > \(SLV_MAX)" endif) |
					i in index_set(xs) where fix(xs[i].maybe_less) \/ fix(xs[i].maybe_more),
			];
		} in if not fix(inexact) then
			"SOLVED: nothing left the solver's range, so this is a real solution"
		elseif length(escaped) = 0 then
			"NOT A SOLUTION: an intermediate result left the solver's range"
		else
			"NOT A SOLUTION: " ++
				join(", ", escaped) ++
				" -- a solution may exist for a solver with wider bounds"
		endif,
	);

/** @group unbounded.out Render \a x, showing the direction when it left the range. */
function string: show_unbounded(Unbounded: x) =
	if fix(x.maybe_less) then
		"<\(SLV_MIN)"
	elseif fix(x.maybe_more) then
		">\(SLV_MAX)"
	else
		show(fix(x.v))
	endif;
