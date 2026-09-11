% The precedence conditions as functions of the decisions, for the reified
% forms.
%
% Each plain form states its bookkeeping with implications over an array the
% solver is otherwise free to choose: `seen` for the value forms, `H` for the
% sequence forms. A reification around such a body is unsound, because the
% solver falsifies it by choosing the array badly. Here the same arrays are
% fixed by the decisions, through an equivalence rather than an implication, so
% the condition is a function of `xs` alone and reifies soundly. That is why the
% standard library's `_reif` files for these constraints abort.

% `ok` holds exactly when `t` appears only once `s` has appeared earlier.
predicate fznso_int_value_precede_holds(
  int: s,
  int: t,
  array [int] of var int: xs,
  var bool: ok,
) =
  if length(xs) = 0 then
    ok
  else
    let {
      int: imin = min(index_set(xs));
      int: imax = max(index_set(xs));
      array [imin..imax + 1] of var bool: seen;
      constraint not seen[imin];
      constraint forall (i in imin..imax) (seen[i + 1] <-> (seen[i] \/ xs[i] = s));
    } in ok <-> forall (i in imin..imax) (not seen[i] -> xs[i] != t)
  endif;

% As above over sets, where a position counts as holding `s` when it holds `s`
% without `t`, and the plain form's own reading of the condition.
predicate fznso_set_value_precede_holds(
  int: s,
  int: t,
  array [int] of var set of int: xs,
  var bool: ok,
) =
  if length(xs) = 0 then
    ok
  else
    let {
      int: imin = min(index_set(xs));
      int: imax = max(index_set(xs));
      array [imin..imax + 1] of var bool: seen;
      constraint not seen[imin];
      constraint
        forall (i in imin..imax) (
          seen[i + 1] <-> (seen[i] \/ (s in xs[i] /\ not (t in xs[i])))
        );
    } in ok <->
      forall (i in imin..imax) (not seen[i] -> (s in xs[i] \/ not (t in xs[i])))
  endif;

% A value above one appears only once the value below it has appeared.
predicate fznso_int_seq_precede_holds(array [int] of var int: xs, var bool: ok) =
  if length(xs) = 0 then
    ok
  else
    let {
      int: l = lb_array(xs);
      int: u = ub_array(xs);
      int: f = min(index_set(xs));
      % `0` stands for "nothing seen yet", so it has to fit whatever the values
      % are.
      array [index_set(xs)] of var min(0, l)..u: H;
      constraint H[f] = max(xs[f], 0);
      constraint forall (i in index_set(xs) diff {f}) (H[i] = max(xs[i], H[i - 1]));
    } in ok <->
      (H[f] <= 1 /\ forall (i in index_set(xs) diff {f}) (H[i] <= H[i - 1] + 1))
  endif;

% As above, where a set's greatest member is what the step is read from.
predicate fznso_set_seq_precede_holds(
  array [int] of var set of int: xs,
  var bool: ok,
) =
  if length(xs) = 0 then
    ok
  else
    let {
      set of int: S = ub_array(xs);
      int: l = min(S);
      int: u = max(S);
      int: f = min(index_set(xs));
      array [index_set(xs)] of var min(0, l)..u: H;
      constraint H[f] = max(xs[f] union {0});
      constraint
        forall (i in index_set(xs) diff {f}) (
          H[i] = max(max(xs[i] union {0}), H[i - 1])
        );
    } in ok <->
      (H[f] <= 1 /\ forall (i in index_set(xs) diff {f}) (H[i] <= H[i - 1] + 1))
  endif;
