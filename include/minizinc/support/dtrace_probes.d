provider minizinc {
  /**
   * Fired when the garbage collection function has finished
   */
  probe gc__end();
  /**
   * Fired when the garbage collection function has started
   */
  probe gc__start();
};
