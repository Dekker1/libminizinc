/** @group flatzinc.ann Mark declaration as being stored in register \a r */
ann: global_register(int: r);

/** @group flatzinc.bool Constrains \( \a r \leftrightarrow \bigwedge_i \a as[i]\) */
predicate array_bool_and(array[int] of var bool: as, var bool: r);
/** @group flatzinc.bool Constrains \( \a r \leftrightarrow \bigvee_i \a as[i]\) */
predicate array_bool_or(array[int] of var bool: as, var bool: r);
/** @group flatzinc.bool Constrains \( \bigvee_i \a as[i] \lor \bigvee_j \lnot \a bs[j] \) */
predicate bool_clause(array[int] of var bool: as, array[int] of var bool: bs);
/** @group flatzinc.two Reified clause constraint.
  Constrains \( \a b \leftrightarrow \bigvee_i \a as[i] \lor \bigvee_j \lnot \a bs[j] \)
 */
predicate bool_clause_reif(array[int] of var bool: as, array[int] of var bool: bs, var bool: b);
/** @group flatzinc.int Constrains \( \a c = \sum_i \a as[i]*\a bs[i] \) */
predicate int_lin_eq(array[int] of int: as, array[int] of var int: bs, int: c);
