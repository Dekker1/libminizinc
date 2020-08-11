provider minizinc {
  /**
   * Fired when the garbage collection function has finished
   */
  probe gc__end();

  /**
   * Fired when the garbage collection function has started
   */
  probe gc__start();

  probe cse__find__start(uintptr_t envi, short nargs);

  probe cse__find__end(uintptr_t envi, int success);

  probe cse__insert__start(uintptr_t envi, short nargs);

  probe cse__insert__end(uintptr_t envi);

};
