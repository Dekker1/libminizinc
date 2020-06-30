#!/usr/bin/env dtrace -s

#pragma D option quiet

minizinc$target:::cse-keyalloc-start
{
  @count["CSE Keys Created"] = count();
  self->start_cse_key = timestamp;
}

minizinc$target:::cse-keyalloc-end
{
  @times["Total Key creation time (ns)"] = sum(timestamp - self->start_cse_key);
}

minizinc$target:::cse-insert-start
{
  @count["CSE Insertions"] = count();
  self->start_cse_insert = timestamp;
  @lquant["Number of Arguments (insert)"] = lquantize(arg1, 1, 6, 1);
}

minizinc$target:::cse-insert-end
{
  @quant["Time Inserting (ns)"] = quantize(timestamp - self->start_cse_insert);
  @times["Total Insertion time (ns)"] = sum(timestamp - self->start_cse_insert);
}

minizinc$target:::cse-lookup-start
{
  @count["CSE Lookups"] = count();
  self->start_cse_lookup = timestamp;
  @lquant["Number of Arguments (lookup)"] = lquantize(arg1, 1, 6, 1);
}

minizinc$target:::cse-lookup-end
{
  @quant["Time in Lookup (ns)"] = quantize(timestamp - self->start_cse_lookup);
  @times["Total Lookup time (ns)"] = sum(timestamp - self->start_cse_lookup);
  @success["CSE Hits"] = sum(arg1);
}
