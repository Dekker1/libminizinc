/***
  Exactness check for unbounded.mzn, run over a tiny solver range so the whole
  abstract value space can be enumerated.

  Soundness alone is not enough: a relation that admits everything is sound
  and useless, because the solver could then satisfy any model by declaring an
  operand out of range.  For every pair of abstract operands this computes the
  exact set of abstract results concrete integers can produce, and requires
  the relation to admit that set and nothing more.

      reachable subset of admitted   is soundness
      admitted subset of reachable   is tightness

  Run it with a symmetric range for full exactness.  With a lopsided range the
  soundness half still holds, but `*`, `div` and `mod` are knowingly weaker
  for operands in the sliver beyond the symmetric core -- pass
  -D TIGHT=false to check soundness only.
***/
include "unbounded.mzn";

SLV_MIN = -7; SLV_MAX = 7;
opt bool: TIGHT;
bool: tight = TIGHT default true;

int: NA = SLV_MAX - SLV_MIN + 3;             % below, then SLV_MIN..SLV_MAX, then above
int: K = (SLV_MAX + 1) * (1 - SLV_MIN);      % far enough out that every result is reachable

function int: aix(int: a) =
  if a < SLV_MIN then 1 elseif a > SLV_MAX then NA else a - SLV_MIN + 2 endif;
function Unbounded: aval(int: i) =
  if i = 1 then alpha(SLV_MIN - 1) elseif i = NA then alpha(SLV_MAX + 1)
  else alpha(i + SLV_MIN - 2) endif;

% Concrete representatives of each abstract value.  In-range values are their
% own single representative; the two out-of-range classes need many.
array[1..NA] of set of int: cls =
  [ if i = 1 then SLV_MIN - K..SLV_MIN - 1
    elseif i = NA then SLV_MAX + 1..SLV_MAX + K
    else {i + SLV_MIN - 2} endif | i in 1..NA ];

function int: papply(int: tag, int: a, int: b) =
  if     tag = UNB_ADD then a + b
  elseif tag = UNB_SUB then a - b
  elseif tag = UNB_MUL then a * b
  elseif tag = UNB_DIV then a div b
  elseif tag = UNB_MOD then a mod b
  elseif tag = UNB_MIN then min(a, b)
  else                     max(a, b) endif;

set of int: BINOPS = {UNB_ADD, UNB_SUB, UNB_MUL, UNB_DIV, UNB_MOD, UNB_MIN, UNB_MAX};

constraint forall (tag in BINOPS, ix, iy in 1..NA
                   where not (tag in {UNB_DIV, UNB_MOD} /\ iy = aix(0))) (
  let { set of int: reach = { aix(papply(tag, a, b)) | a in cls[ix], b in cls[iy] } } in
  forall (iz in 1..NA) (
    if iz in reach then unb_rel(tag, aval(ix), aval(iy), aval(iz))
    elseif tight   then not (unb_rel(tag, aval(ix), aval(iy), aval(iz)))
    else true endif
  )
);

constraint forall (tag in {UNB_NEG, UNB_ABS}, ix in 1..NA) (
  let { set of int: reach = { aix(if tag = UNB_NEG then -a else abs(a) endif) | a in cls[ix] } } in
  forall (iz in 1..NA) (
    if iz in reach then unb_rel1(tag, aval(ix), aval(iz))
    elseif tight   then not (unb_rel1(tag, aval(ix), aval(iz)))
    else true endif
  )
);

% Comparisons.  Where the concrete order admits only one answer the relation
% must force it; where it admits both it must force neither.
constraint forall (ix, iy in 1..NA) (
  let { set of int: rlt = { bool2int(a < b) | a in cls[ix], b in cls[iy] },
        set of int: req = { bool2int(a = b) | a in cls[ix], b in cls[iy] } } in
     (if     rlt = {1} then     unb_lt(aval(ix), aval(iy))
      elseif rlt = {0} then not unb_lt(aval(ix), aval(iy))
      else true endif)
  /\ (if     req = {1} then     unb_eq(aval(ix), aval(iy))
      elseif req = {0} then not unb_eq(aval(ix), aval(iy))
      else true endif)
);

% The cells where both answers are reachable must leave both open.  Separate
% pairs throughout, so nothing is shared through common subexpressions.
array[1..8] of Unbounded: fr;
constraint forall (i in 1..8) (unb_var(fr[i]));
constraint forall (i in 1..4) (is_less(fr[i])) /\ forall (i in 5..8) (is_more(fr[i]));
constraint fr[1] < fr[2] /\ not (fr[3] < fr[4]);
constraint fr[5] < fr[6] /\ not (fr[7] < fr[8]);

solve satisfy;
output [(if tight then "exact" else "sound" endif)
     ++ " over the full abstract space for \(SLV_MIN)..\(SLV_MAX)\n"];
