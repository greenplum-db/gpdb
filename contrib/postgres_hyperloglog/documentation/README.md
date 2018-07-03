# Design Specifics

The "HyperLogLog in Practice: Algorithmic Engineering of a State of The Art Cardinality Estimation Algorithm", published by Stefan Heulem, Marc Nunkesse and Alexander Hall describes several improvements to the original algorithm created by Flajolet. The improvements focus on correcting and improving the algorithm's performance on lower cardinalities.

I have also made several modifications to help with the amount of space taken when stored to disc by implementing compression on the counters.

A few things to note about the following analysis:

  * For the sake of simplicity assume all estimators are using 2^14 buckets of 6-bits in width.
  * Sigma values for each cardinality are generated from 500 random samples
  * Average compression size is based off of 400 random samples

## Low Cardinality Improvements
The two low cardinality improvements introduced in the "HyperLogLog in Practice" paper are the utilization of sparse encoding and error correction.

### Sparse Encoding
The first improvement is applicable to very low cardinalities. When a counter isn't populated with many entries most of the bins are entirely empty and thus most of the counter is 0's. So instead of storing the rho values in their proper bins one can store the value `bin_index | rho` (the 14 binbits and 6 row bits concatenated together) in a list until a certain cutoff is reached (discussed in the compression section) where it is deemed no longer beneficial. This allows for the counter to be smaller both in memory and on disc.

However, this also opens up another improvement in accuracy. The concatenation of the bin_index and rho is only 20-bits long (14 + 6) so a large part of this number is 0-bits. To take advantage of this extra space a larger bin_index (25-bits in this case) is used which improves the accuracy of linear counting dramatically because the potential number of counters is 2^25 not 2^14. This encoding method is described in greater detail in the "HyperLogLog in Practice" paper.

![Sparse to Dense](documentation/sparse_to_dense_final.png?raw=true)

The improved accuracy can be seen until the counter switches from sparse encoding to dense (at 1020).

### Error Correction
Flajolet's original algorithm actually uses two algorithms depending on the estimated cardinality to provide an estimate. First an estimate is created using the hyperloglog algorithm if this estimate is less than 2.5 * number_of_bins (40,960) then a new estimate is computed using the linear counting algorithm. This helps to compensate for the hyperloglog algorithm's over-estimation bias for low cardinalities.

![Flajolet's Original Algorithm](documentation/original_algorithm.png?raw=true)

However you can clearly see that a bias still exists when the algorithm switch occurs. It doesn't reach acceptable accuracy until 5 * number_of_bins (81,920). This is where some of the improvements described in the "HyperLogLog in Practice" paper are focused. Luckily hyperloglog's bias is very predictable and consistent. Using a list of empircally calculated average errors for ~200 cardinalities in this range much of this bias can be corrected. The error correction is so accurate that it actually becomes beneficial to switch to hyperloglog from linear-counting at a lower cardinality (11,500 instead of 40,960). There are several ways to to use these average errors to correct the bias. The first method tried was simply an average of the two average errors for the the cardinalities closest to the estimate.

![2-point average](documentation/2-point_average.png?raw=true)

This method tends to fluctuate depending on where the estimated cardinality is in relation to nearest average error points. The paper suggests using nearest neighbour for the 6 nearest points (i.e. average of the 6 closest average error points).

![6-point average](documentation/6-point_average.png?raw=true)

This produces better results on higher cardinalities however it performs much worse in the lower cardinalities. So the next attempt was using a linear interpolation of the nearest 2 average error points (i.e. draw a line between them and find where you estimate lies on the line).

![2-point interpolation](documentation/2-point_interpolation.png?raw=true)

This smooths the fluctuations as the cardinality ranges in between average error points. Unfortunately the higher cardinalities are slightly less accurate than the nearest 6 neighbour estimation. So in order to improve on this simple linear regression on the nearest 6 error average points was used.

![6-point interpolation](documentation/final_choice.png?raw=true)

This produces both good results in the high and low end of the correction range and thus was the final choice for this implementation.

## Compression
The method of compression is different depending on the encoding the counter is using at the time. This is because each type of counter/encoding has different properties which allow certain methods to be applied to one and not the other. For example the order of the sparse encoded list is unimportant while the order of the buckets in the dense encoding dictates the index for each bucket so it must be maintained. Another thing to note is there will be many duplicate values in dense encoding while there will be relatively few if any in sparse encoding. These differences lead to different optimal handling of each.

### Sparse Compression
The sparse data is essentially a list of unsigned 32-bit integers. Since the order is unimportant because the index value is part of the 32-bit integer itself sorting the list and converting it to a list of delta's (e.g [3,2,5] => [2,+1,+2]) is lossless. This will reduce the average size of each entry significantly. Since there are still very few to no repeated values something like lz compression wouldn't be helpful. However we do have a list of integers whose length is quite limited so a type of variable length encoding would help eliminate the 0-bits and reduce the overall size. There are two main types of variable length encoding. The simplest uses a continuation bit-flag to indicate that the next byte is part of that number. However another method operates on groups of 4 integers using a leading byte to indicate the number of bytes per each integer in the group. This method is called group varint encoding.

![Sparse Compression on Disc](documentation/sparse_compression_size.png?raw=true)

Group varint regularly outperforms continuation bit encoding in terms of compression ratios. However its biggest benefit is in decode speed which is cited by its developers as being ~400M numbers/second as opposed to continuation bit encodings ~180M numbers/second.

### Dense Compression
The order of the bins must be maintained as its representitive of the index of each. However each bin has only 64 possible values. This is a good example of when something like lz compression would do very well. However since the counter is bit-packed in memory (each bin is only 6-bits long so they aren't aligned with 8-bit byte lines) lz compression won't properly detect that only 64 values are being used in each bin since it reads per byte. In order to acheive best results it was necessary to unpack the bins so now each bin is its own 8-bit unsigned integer and compress the unpacked structure.

![LZ compression](documentation/lz_compression.png?raw=true)

It can be seen that the counter size levels off around 9.5KB which is a reasonable improvement over the storing the raw bit-packed structure which is always ~12KB.

### When To Switch From Sparse To Dense
Another important aspect to consider is the threshold where the counter will switch from sparse to dense encoding. The selected value used here is 1020. This value was chosen because it allows for the initial counter memory allocation to fit perfectly in the 4KB memory block (next size up is 8KB) which reduces very costly memory allocation overhead.

```
counter_size = struct_overhead_size + sparse_array_size
counter_size = 16-bytes + number_of_4_byte_entries * 4
4096 = 16 + 1020*4
```

The next highest cutoff that would fully use the memory block is 2044

```
8192 = 16 + 2044*4
```

If you extrapolate the fairly linear compression ratio of the group varint encoding you get an estimated compressed counter of ~5300 bytes with 2044 entries. When the switch from sparse to dense encoding occurs at 2045 entries the counter is now ~3000 bytes. This gyration in counter size (5300 -> 3000 as opposed to 2700 -> 2150) is much greater and leads to less predictable counter sizes to someone who isn't entirely familiar with the internals. This coupled with 8KB memory allocation being much more costly than 4KB memory allocation led us to choose 1020 as the threshold.
