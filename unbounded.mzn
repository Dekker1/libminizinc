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

constraint assert(SLV_MIN < 0 /\ SLV_MAX > 0,
  "unbounded.mzn: SLV_MIN < 0 < SLV_MAX is required");

/* The symmetric core of the range, where negation is always representable.
   Equal to the whole range when the range is symmetric. */
int: R = min(SLV_MAX, -SLV_MIN);

/* Lower bound on the product of two below-range values, or SLV_MAX+1 when
   that product cannot be represented at all.  Guarded so the par
   multiplication itself cannot overflow. */
int: LL_MIN = if SLV_MIN - 1 < -SLV_MAX then SLV_MAX + 1
              else (SLV_MIN - 1) * (SLV_MIN - 1) endif;

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
  not (x.maybe_less /\ x.maybe_more)
  /\ (x.maybe_less -> x.v = SLV_MIN)
  /\ (x.maybe_more -> x.v = SLV_MAX)
  /\ (not in_range(x) -> unb_inexact);

/** @group unbounded.type Structural equality -- same class and same value. */
predicate unb_same(Unbounded: x, Unbounded: y) =
  x.v = y.v /\ x.maybe_less = y.maybe_less /\ x.maybe_more = y.maybe_more;

/***  @groupdef unbounded.build Construction  ***/

/** @group unbounded.build The abstraction map: the `Unbounded` denoting \a a. */
function Unbounded: alpha(int: a) =
  (v: if a < SLV_MIN then SLV_MIN elseif a > SLV_MAX then SLV_MAX else a endif,
   maybe_less: a < SLV_MIN,
   maybe_more: a > SLV_MAX);

/** @group unbounded.build Lift a variable that is already inside the range. */
function Unbounded: unb(var SLV_MIN..SLV_MAX: a) = (v: a, maybe_less: false, maybe_more: false);

/***  @groupdef unbounded.rel The relations  ***/

/* Operation tags, used to keep CSE from merging distinct operations. */
int: UNB_ADD = 1; int: UNB_SUB = 2; int: UNB_MUL = 3;
int: UNB_DIV = 4; int: UNB_MOD = 5; int: UNB_MIN = 6; int: UNB_MAX = 7;
int: UNB_NEG = 8; int: UNB_ABS = 9;

/* True when \a a is inside the symmetric core, so that -a is representable.
   Always true for a symmetric solver range. */
predicate in_core(var int: a) = a >= -R /\ a <= R;

/** @group unbounded.rel `z` is a sound abstraction of `x + y`.

  The in-range case needs no auxiliary variable: `x.v + y.v > SLV_MAX` becomes
  a reified linear constraint over x.v and y.v against a constant, which is
  still open even though both operands are confined to the range.
*/
predicate unb_plus(Unbounded: x, Unbounded: y, Unbounded: z) =
  (in_range(x) /\ in_range(y) ->
     (is_less(z) <-> x.v + y.v < SLV_MIN)
     /\ (is_more(z) <-> x.v + y.v > SLV_MAX)
     /\ (in_range(z) -> z.v = x.v + y.v))
  /\ (in_range(x) /\ is_more(y) -> not is_less(z) /\ (in_range(z) -> z.v - x.v > SLV_MAX))
  /\ (is_more(x) /\ in_range(y) -> not is_less(z) /\ (in_range(z) -> z.v - y.v > SLV_MAX))
  /\ (in_range(x) /\ is_less(y) -> not is_more(z) /\ (in_range(z) -> z.v - x.v < SLV_MIN))
  /\ (is_less(x) /\ in_range(y) -> not is_more(z) /\ (in_range(z) -> z.v - y.v < SLV_MIN))
  /\ (is_more(x) /\ is_more(y) -> is_more(z))
  /\ (is_less(x) /\ is_less(y) -> is_less(z));
  % (below, above) and (above, below) are left open: the sum can be anything.

/** @group unbounded.rel `z` is a sound abstraction of `x - y`. */
predicate unb_minus(Unbounded: x, Unbounded: y, Unbounded: z) =
  (in_range(x) /\ in_range(y) ->
     (is_less(z) <-> x.v - y.v < SLV_MIN)
     /\ (is_more(z) <-> x.v - y.v > SLV_MAX)
     /\ (in_range(z) -> z.v = x.v - y.v))
  /\ (in_range(x) /\ is_more(y) -> not is_more(z) /\ (in_range(z) -> z.v - x.v < -SLV_MAX))
  /\ (in_range(x) /\ is_less(y) -> not is_less(z) /\ (in_range(z) -> z.v - x.v > -SLV_MIN))
  /\ (is_more(x) /\ in_range(y) -> not is_less(z) /\ (in_range(z) -> z.v + y.v > SLV_MAX))
  /\ (is_less(x) /\ in_range(y) -> not is_more(z) /\ (in_range(z) -> z.v + y.v < SLV_MIN))
  /\ (is_more(x) /\ is_less(y) -> is_more(z))
  /\ (is_less(x) /\ is_more(y) -> is_less(z));
  % (above, above) and (below, below) are left open.

/** @group unbounded.rel `z` is a sound abstraction of `-x`. */
predicate unb_neg(Unbounded: x, Unbounded: z) =
  (in_range(x) ->
     (is_less(z) <-> x.v > -SLV_MIN)
     /\ (is_more(z) <-> x.v < -SLV_MAX)
     /\ (in_range(z) -> z.v + x.v = 0))
  /\ (is_more(x) -> not is_more(z) /\ (in_range(z) -> z.v < -SLV_MAX))
  /\ (is_less(x) -> not is_less(z) /\ (in_range(z) -> z.v > -SLV_MIN));

/** @group unbounded.rel `z` is a sound abstraction of `abs(x)`.

  `abs(x.v)` is avoided: for a lopsided range it can exceed SLV_MAX, so it
  would need an unrepresentable auxiliary.  Splitting on the sign of x.v keeps
  every constraint linear in variables that are already in range.
*/
predicate unb_abs(Unbounded: x, Unbounded: z) =
  (in_range(x) /\ x.v >= 0 -> in_range(z) /\ z.v = x.v)
  /\ (in_range(x) /\ x.v < 0 ->
        (is_more(z) <-> x.v < -SLV_MAX)
        /\ not is_less(z)
        /\ (in_range(z) -> z.v + x.v = 0))
  /\ (is_more(x) -> is_more(z))
  /\ (is_less(x) -> not is_less(z) /\ (in_range(z) -> z.v > -SLV_MIN));

/* Both operands in range.  The product is the one place a wide auxiliary
   would appear, so the class is decided by dividing the range endpoints by
   the operand instead.  Every value below stays inside SLV_MIN..SLV_MAX:
   |SLV_MAX div d| <= SLV_MAX and |SLV_MIN div d| <= -SLV_MIN for d >= 1, and
   clamping into the symmetric core keeps the negation representable. */
predicate unb_times_ii(var int: a, var int: b, Unbounded: z) =
  let {
    var -R..R: ac = max(-R, min(R, a)),
    var -R..R: bc = max(-R, min(R, b)),
    var 1..R: d = max(1, max(bc, -bc)),
    var 0..SLV_MAX: hi = SLV_MAX div d,
    var SLV_MIN..0: lo = SLV_MIN div d,
    var -R..R: am = if bc < 0 then -ac else ac endif,
    var bool: core = a = ac /\ b = bc,
    var bool: pos = (a > 0 /\ b > 0) \/ (a < 0 /\ b < 0),
  } in
    (core -> (in_range(z) <-> (am >= lo /\ am <= hi)))
    /\ (in_range(z) -> z.v = a * b)
    /\ (is_more(z) <-> (not in_range(z) /\ pos))
    /\ (is_less(z) <-> (not in_range(z) /\ not pos));

/* One operand in range (`a`), the other (`w`) outside it. */
predicate unb_times_io(var int: a, Unbounded: w, Unbounded: z) =
  (a = 0 -> in_range(z) /\ z.v = 0)
  /\ (a > 0 /\ is_more(w) -> is_more(z))
  /\ (a > 0 /\ is_less(w) -> is_less(z))
  /\ (a < 0 /\ is_more(w) -> not is_more(z) /\ (in_range(z) -> z.v < -SLV_MAX))
  /\ (a < 0 /\ is_less(w) -> not is_less(z) /\ (in_range(z) -> z.v > -SLV_MIN));

/** @group unbounded.rel `z` is a sound abstraction of `x * y`. */
predicate unb_times(Unbounded: x, Unbounded: y, Unbounded: z) =
  (in_range(x) /\ in_range(y) -> unb_times_ii(x.v, y.v, z))
  /\ (in_range(x) /\ not in_range(y) -> unb_times_io(x.v, y, z))
  /\ (not in_range(x) /\ in_range(y) -> unb_times_io(y.v, x, z))
  /\ (is_more(x) /\ is_more(y) -> is_more(z))
  /\ (is_less(x) /\ is_more(y) -> is_less(z))
  /\ (is_more(x) /\ is_less(y) -> is_less(z))
  /\ (is_less(x) /\ is_less(y) -> not is_less(z) /\ (in_range(z) -> z.v >= LL_MIN));

/** @group unbounded.rel
  `z` is a sound abstraction of `x div y` (truncating).  Division by zero is
  left open rather than made false.
*/
predicate unb_div(Unbounded: x, Unbounded: y, Unbounded: z) =
  (in_range(x) /\ in_range(y) /\ y.v != 0 ->
     % |x.v div y.v| <= |x.v|, so the quotient can only escape from the
     % sliver of a lopsided range
     (in_range(z) -> z.v = x.v div y.v)
     /\ (in_core(x.v) -> in_range(z))
     /\ (is_more(z) -> (x.v > 0 /\ y.v > 0) \/ (x.v < 0 /\ y.v < 0))
     /\ (is_less(z) -> (x.v > 0 /\ y.v < 0) \/ (x.v < 0 /\ y.v > 0)))
  /\ (in_range(x) /\ is_more(y) ->
        % |quotient| <= |x.v| div |y|, and y is above the range.  The quotient
        % is only guaranteed back in range when that bound fits, which for a
        % lopsided range it need not.
        (DIV_BG <= R -> in_range(z))
        /\ (x.v >= 0 -> not is_less(z) /\ (in_range(z) -> z.v >= 0 /\ z.v <= DIV_BG))
        /\ (x.v <= 0 -> not is_more(z) /\ (in_range(z) -> z.v <= 0 /\ z.v >= -DIV_BG)))
  /\ (in_range(x) /\ is_less(y) ->
        % y is below the range, so the quotient flips sign
        (DIV_BL <= R -> in_range(z))
        /\ (x.v >= 0 -> not is_more(z) /\ (in_range(z) -> z.v <= 0 /\ z.v >= -DIV_BL))
        /\ (x.v <= 0 -> not is_less(z) /\ (in_range(z) -> z.v >= 0 /\ z.v <= DIV_BL)))
  /\ (not in_range(x) /\ in_range(y) /\ y.v = 1 -> unb_same(x, z))
  /\ (not in_range(x) /\ in_range(y) /\ y.v = -1 -> unb_neg(x, z))
  /\ (not in_range(x) /\ in_range(y) /\ y.v != 0 ->
        % the sign of the quotient follows from the classes alone.  Its
        % magnitude is at least |x| div |y.v|, but only once |x| is known to
        % exceed |y.v| -- for a lopsided range x may sit just past one end
        % while y.v sits further past the other.
        let { var -R..R: yc = max(-R, min(R, y.v)),
              var 0..R: dy = max(yc, -yc),
              var 2..max(2, R): dy2 = max(2, dy),
              var 0..R: qm = min(R, if is_more(x) then (SLV_MAX + 1) div dy2
                                                  else DIV_A_LESS div dy2 endif),
              var bool: qpos = (is_more(x) /\ y.v > 0) \/ (is_less(x) /\ y.v < 0),
              var bool: big = in_core(y.v) /\ dy >= 2
                              /\ ((is_more(x) /\ y.v >= -SLV_MAX)
                                   \/ (is_less(x) /\ y.v <= -SLV_MIN)) } in
          (qpos -> not is_less(z) /\ (in_range(z) /\ big -> z.v >= qm))
          /\ (not qpos -> not is_more(z) /\ (in_range(z) /\ big -> z.v <= -qm)))
  /\ (not in_range(x) /\ not in_range(y) ->
        let { var bool: qpos = is_less(x) <-> is_less(y) } in
          (qpos -> not is_less(z) /\ (in_range(z) -> z.v >= 0))
          /\ (not qpos -> not is_more(z) /\ (in_range(z) -> z.v <= 0)));

/** @group unbounded.rel
  `z` is a sound abstraction of `x mod y`.  Modulo zero is left open.
*/
predicate unb_mod(Unbounded: x, Unbounded: y, Unbounded: z) =
  (in_range(x) /\ in_range(y) /\ y.v != 0 ->
     % |x.v mod y.v| < |y.v|, so the remainder can only escape from the
     % sliver of a lopsided range
     (in_range(z) -> z.v = x.v mod y.v)
     /\ (in_core(y.v) -> in_range(z))
     /\ (is_more(z) -> x.v > 0)
     /\ (is_less(z) -> x.v < 0))
  /\ (in_range(x) /\ not in_range(y) ->
        % |x.v mod y| <= |x.v| and takes x.v's sign, and equals x.v outright
        % once |x.v| is below the magnitude the class of y guarantees
        in_range(z)
        /\ (x.v >= 0 -> z.v >= 0 /\ z.v <= x.v)
        /\ (x.v <= 0 -> z.v <= 0 /\ z.v >= x.v)
        /\ (is_more(y) /\ x.v >= -SLV_MAX -> z.v = x.v)
        /\ (is_less(y) /\ x.v <= -SLV_MIN -> z.v = x.v))
  /\ (not in_range(x) /\ in_range(y) /\ y.v != 0 ->
        % the remainder takes the sign of the dividend and is smaller in
        % magnitude than the divisor.  |y.v| is not written directly: for a
        % lopsided range -y.v can exceed SLV_MAX, so the two signs of y.v are
        % handled separately to keep every term in range.
        (y.v >= -SLV_MAX /\ y.v <= -SLV_MIN -> in_range(z))
        /\ (is_more(x) -> not is_less(z)
              /\ (in_range(z) -> z.v >= 0
                    /\ (y.v > 0 -> z.v < y.v) /\ (y.v < 0 -> z.v + y.v < 0)))
        /\ (is_less(x) -> not is_more(z)
              /\ (in_range(z) -> z.v <= 0
                    /\ (y.v > 0 -> z.v + y.v > 0) /\ (y.v < 0 -> z.v > y.v))))
  /\ (not in_range(x) /\ not in_range(y) ->
        (is_more(x) -> not is_less(z) /\ (in_range(z) -> z.v >= 0))
        /\ (is_less(x) -> not is_more(z) /\ (in_range(z) -> z.v <= 0)));

/** @group unbounded.rel `z` is a sound abstraction of `min(x, y)`.  Always exact. */
predicate unb_min(Unbounded: x, Unbounded: y, Unbounded: z) =
  (is_less(x) \/ is_less(y) -> is_less(z))
  /\ (in_range(x) /\ in_range(y) -> in_range(z) /\ z.v = min(x.v, y.v))
  /\ (in_range(x) /\ is_more(y) -> unb_same(x, z))
  /\ (is_more(x) /\ in_range(y) -> unb_same(y, z))
  /\ (is_more(x) /\ is_more(y) -> is_more(z));

/** @group unbounded.rel `z` is a sound abstraction of `max(x, y)`.  Always exact. */
predicate unb_max(Unbounded: x, Unbounded: y, Unbounded: z) =
  (is_more(x) \/ is_more(y) -> is_more(z))
  /\ (in_range(x) /\ in_range(y) -> in_range(z) /\ z.v = max(x.v, y.v))
  /\ (in_range(x) /\ is_less(y) -> unb_same(x, z))
  /\ (is_less(x) /\ in_range(y) -> unb_same(y, z))
  /\ (is_less(x) /\ is_less(y) -> is_less(z));

/** @group unbounded.rel The relation for binary operation \a op, selected by tag. */
predicate unb_rel(int: op, Unbounded: x, Unbounded: y, Unbounded: z) =
  if     op = UNB_ADD then unb_plus(x, y, z)
  elseif op = UNB_SUB then unb_minus(x, y, z)
  elseif op = UNB_MUL then unb_times(x, y, z)
  elseif op = UNB_DIV then unb_div(x, y, z)
  elseif op = UNB_MOD then unb_mod(x, y, z)
  elseif op = UNB_MIN then unb_min(x, y, z)
  else                     unb_max(x, y, z) endif;

/** @group unbounded.rel The relation for unary operation \a op, selected by tag. */
predicate unb_rel1(int: op, Unbounded: x, Unbounded: z) =
  if op = UNB_NEG then unb_neg(x, z) else unb_abs(x, z) endif;

/***  @groupdef unbounded.ops Operations  ***/

/* Allocate the result of an operation.  `op` is part of the call so that
   common subexpression elimination keeps distinct operations apart. */
function Unbounded: unb_bin(int: op, Unbounded: x, Unbounded: y) :: promise_total =
  let {
    var SLV_MIN..SLV_MAX: rv; var bool: rl; var bool: rm;
    Unbounded: z = (v: rv, maybe_less: rl, maybe_more: rm);
    constraint unb_var(z);
    constraint unb_rel(op, x, y, z);
  } in z;

function Unbounded: unb_un(int: op, Unbounded: x) :: promise_total =
  let {
    var SLV_MIN..SLV_MAX: rv; var bool: rl; var bool: rm;
    Unbounded: z = (v: rv, maybe_less: rl, maybe_more: rm);
    constraint unb_var(z);
    constraint unb_rel1(op, x, z);
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

function Unbounded: unb_fold(int: op, array[int] of Unbounded: xs, int: i) =
  if i = max(index_set(xs)) then xs[i]
  else unb_bin(op, xs[i], unb_fold(op, xs, i + 1)) endif;

/** @group unbounded.ops Sum of \a xs. */
function Unbounded: sum(array[int] of Unbounded: xs) =
  if length(xs) = 0 then alpha(0) else unb_fold(UNB_ADD, xs, min(index_set(xs))) endif;
/** @group unbounded.ops Product of \a xs. */
function Unbounded: product(array[int] of Unbounded: xs) =
  if length(xs) = 0 then alpha(1) else unb_fold(UNB_MUL, xs, min(index_set(xs))) endif;

/***  @groupdef unbounded.cmp Comparison  ***/

/* Strictly less.  Free in (below, below) and (above, above), where the
   concrete order genuinely admits both answers. */
predicate unb_lt(Unbounded: x, Unbounded: y) :: promise_total =
  let {
    var bool: r;
    constraint in_range(x) /\ in_range(y) -> (r <-> x.v < y.v);
    constraint is_less(x) /\ not is_less(y) -> r;
    constraint in_range(x) /\ is_more(y) -> r;
    constraint is_more(x) /\ not is_more(y) -> not r;
    constraint in_range(x) /\ is_less(y) -> not r;
  } in r;

/* Equal.  Free in (below, below) and (above, above). */
predicate unb_eq(Unbounded: x, Unbounded: y) :: promise_total =
  let {
    var bool: r;
    constraint in_range(x) /\ in_range(y) -> (r <-> x.v = y.v);
    constraint not (x.maybe_less = y.maybe_less /\ x.maybe_more = y.maybe_more) -> not r;
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
function string: unb_status(array[int] of string: names, array[int] of Unbounded: xs,
                            var bool: inexact) =
  assert(index_set(names) = index_set(xs), "unb_status: one name per variable",
  let {
    array[int] of string: escaped =
      [ names[i] ++ (if fix(xs[i].maybe_less) then " < \(SLV_MIN)" else " > \(SLV_MAX)" endif)
      | i in index_set(xs) where fix(xs[i].maybe_less) \/ fix(xs[i].maybe_more) ];
  } in if not fix(inexact)
       then "SOLVED: nothing left the solver's range, so this is a real solution"
       elseif length(escaped) = 0
       then "NOT A SOLUTION: an intermediate result left the solver's range"
       else "NOT A SOLUTION: " ++ join(", ", escaped)
            ++ " -- a solution may exist for a solver with wider bounds"
       endif);

/** @group unbounded.out Render \a x, showing the direction when it left the range. */
function string: show_unbounded(Unbounded: x) =
  if fix(x.maybe_less) then "<\(SLV_MIN)"
  elseif fix(x.maybe_more) then ">\(SLV_MAX)"
  else show(fix(x.v)) endif;
