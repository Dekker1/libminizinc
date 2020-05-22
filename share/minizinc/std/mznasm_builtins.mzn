/** @group flatzinc.ann Mark declaration as being stored in register \a r */
ann: global_register(int: r);

/** @group flatzinc.bool Constrains \( \a r \leftrightarrow \bigwedge_i \a as[i]\) */
predicate array_bool_and(array[int] of var bool: as, var bool: r);
/** @group flatzinc.bool Constrains \( \a r \leftrightarrow \bigvee_i \a as[i]\) */
predicate array_bool_or(array[int] of var bool: as, var bool: r);
