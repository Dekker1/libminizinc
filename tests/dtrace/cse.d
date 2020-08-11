#!/usr/bin/env dtrace -s

#pragma D option quiet

minizinc$target:::cse-insert-start
{
  @count["CSE Insertions"] = count();
  @lquant["Number of Arguments (insert)"] = lquantize(arg1, 1, 6, 1);
  self->start_cse_insert = timestamp;
}

minizinc$target:::cse-insert-end
{
  @quant["Time Inserting (ns)"] = quantize(timestamp - self->start_cse_insert);
  @times["Total Insertion time (ns)"] = sum(timestamp - self->start_cse_insert);
}

minizinc$target:::cse-find-start
{
  @count["CSE Lookups"] = count();
  @lquant["Number of Arguments (lookup)"] = lquantize(arg1, 1, 6, 1);
  self->start_cse_lookup = timestamp;
}

minizinc$target:::cse-find-end
{
  @quant["Time in Lookup (ns)"] = quantize(timestamp - self->start_cse_lookup);
  @times["Total Lookup time (ns)"] = sum(timestamp - self->start_cse_lookup);
  @success["CSE Hits"] = sum(arg1);
}
