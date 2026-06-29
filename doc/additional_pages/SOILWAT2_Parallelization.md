# SOILWAT2 Parallelization

[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2

Note: this document is best viewed as part of the doxygen-built documentation
(there may be text artifacts if viewed as standalone-markdown).

<br>

# Parallelization Guide for v8.5.0

SOILWAT2 now uses a time-before-space approach to organize
simulations across a spatial domain. A time-before-space approach
simulates a time step for all sites or gridcells within a domain before moving
on to the next time step.
This differs from previous versions which used a space-before-time approach,
i.e., all time steps for a site or gridcell were simulated before moving on
to the next site or gridcell.
This change resulted in a faster and more efficient parallelization and
addressed several shortcomings in the previous version.

### Design Overview
In the new approach, each process is assigned a rectangular chunk of the
simulation domain.
Previously (versions v8.4.0 & v8.3.0), processes worked on batches
of sites/gridcells (N_SUID_ASSIGN defined at compilation) for
reading data, simulation and output values.
The new approach greatly reduces the number of reads and writes during
a simulation because a process can now read and write from all assigned
sites/gridcells simultaneously.
Furthermore, this version leverages file stripe sizes,
Object Storage Targets (OSTs), and available memory
to further improve output performance.

### Main Changes
- Split all sites/gridcells across all processes;
  previously, the code worked on separate batches of
  N_SUID_ASSIGN active sites/gridcells
- Minimize read/writes by reading from the entire simulation domain
    - Read inputs and parameters at startup
    - Read daily weather forcing for the currently simulated year
    - Previously, each batch of N_SUID_ASSIGN active sites/gridcells resulted
      in separate reads and writes
- Improved organization and chunking of outputs in netCDF format
    - The temporal dimension is now changing the slowest
      (see [CF conventions](https://cfconventions.org/)); previously, the
      old time-before-space approach required time to change the fastest
    - Chunks now combine multiple spatial site/gridcells \<time chunk size\>xNxM;
      previoulsy, each site/gridcell was chunked with 1x1x<time chunk size>
    - The code identifies an ideal chunk size based on
        - Number of processes
        - File/directory stripe size
        - Upper limit of 16MB chunk size
        - Available program memory

### Recommendations
- Adjust the compute power (number of processes, memory) to the
  demands of the simulation project, e.g., too many processes for too few
  sites/gridcells can detoriate performance.
- Increasing number of OSTs can speed up output on parallel file systems.

### Current Known Limitations/Problems
- Insufficient file descriptors can lead to segmentation faults:
[issue #465](https://github.com/DrylandEcology/SOILWAT2/issues/465)
    - The solution is to increase the number of file descriptors on the system

### Fixed problems with this version
- Too large N_SUID_ASSIGN caused segmentation faults
[issue #469](https://github.com/DrylandEcology/SOILWAT2/issues/469)
    - **This constant was removed**

- Stalling on multiple nodes
[issue #470](https://github.com/DrylandEcology/SOILWAT2/issues/470)
    - **Multiple nodes can now be used**


### Performance Results

#### Methodology
- Performance tests were run for site-oriented and gridded domains
    - Test project domain
        - Site-based domain with ~12,000 sites
        - Gridded domain with ~112,000 active sites (~168,000 gridcells)
    - Number of cores
        - One node: 16, 32, 64, 128
        - Two nodes: 256
        - Three nodes: 384
    - Number of years: 30 (sites) and 46 (gridded)
    - Stripe size: 1MB and 4MB
    - Number of OSTs: 1, 2, 5 and -1/Inf (1 and -1/Inf only shown here)
    - Performance metrics
        - Speedup
        - Efficiency
        - Partition timings (input, compute and output) measured as
          average time over 365 days of simulation for the entire domain
- Comparison of speedup against v8.4.0 for 16, 32 and 64 processes
  (v8.4.0 stalled with more processes)
- Hardware: Dual 64-core AMD 7662 (2.0/3.3 GHz) "Rome" CPUs with 512 GB memory


#### Results

**Speedup**

![Combined Speedup Graph](Combined_Speedup.png)

The speedup for site-based domains was smaller than the speed-up for gridded
domains. Additionally, the speedup for the site-based domain test run plateaued
and even decreased at larger number of processes. This was likely because of
the small size of the site-based domain. The speedup of the gridded domain
continued increasing for all tested numbers of processes,
maxing out at around 114x speed up at three full nodes. This suggests that
additional speedup may be gained from even more processes for this test domain.

**Sites**
Percentage Time Partitioning | Absolute Time Partitioning
:-----------|-----------:
![site_partition_timing_perc](sites_partition_perc_timing.png) | ![site_partition_timing_abs](sites_partition_abs_timing.png)

For both 1MB striping + 1 OST and 4MB striping + Inf OSTs, time for the simulation computation decreased as expected with more
processes, eventually reaching less than a second walltime.
Time for inputs and outputs did not change as a function of the number of processes.
This resulted in increasing relative contributions of inputs and outputs to the
total time. When switching from 1MB striping + 1 OST to 4MB striping + Inf OSTs, output
is comparatively lower throughout all simulations.

**Gridded**
Percentage Time Partitioning | Absolute Time Partitioning
:-----------|-----------:
![gridded_partition_timing_perc](gridded_partition_perc_timing.png) | ![gridded_partition_timing_abs](gridded_partition_abs_timing.png)

The results were overall comparable to those for the site-base domain.
However, walltimes are larger for the gridded test domain because
it was much larger than the site-based domains.
Output for the gridded test domain was, similar to the site-based domain,
consistently faster with 4MB file striping and many OSTs.

**Efficiency**
![Combined Efficiency Graph](Combined_Efficiency.png)

Even though the parallelization approach of this version successfully
scaled simulation computations, input and output times did not scale
with number of processes.
Therefore, overall efficiency decreased with additional processes.
This result suggests that there is an optimal number of computational resources
for a given simulation project that may be smaller than all available processes.


**Comparison between v8.4.0 and v8.5.0**
![v840_vs_v850](TBS_performance_vs_SBT_performance.png)

The new version was about 3x faster than the previous version for the gridded
test domain.

### Deflating
SOILWAT2 provides the option to deflate an output file with 10 difference levels - 0 (off) and 1-9 - each level being more intense compression than the last. With larger domains, storage can be a concern with output file sizes. Deflation was tested on multiple levels on a ~810K total gridcell domain with no spinup and 10 years of simulation. Here are the results:

| Compression | Wall Time (sec) | Time for Output of 365 Days (sec) | Filesize Daily XYT - ET (GB) | Filesize Daily XYZT - VWC, 10 layers (GB)|
| :---------: | :-------------: | :-------------------------------: | :--------------------------: | :--------------------------------------: |
| None        | 915             | 66.148                            | 23                           | 222                                      |
| 1           | 826             | 57.680                            | 11                           | 103                                      |
| 3           | 835             | 58.005                            | 11                           | 102                                      |
| 4           | 834             | 58.280                            | 11                           | 102                                      |
| 5           | 844             | 59.064                            | 11                           | 101                                      |
| 9           | 1128            | 97.553                            | 11                           | 101                                      |

The key takeways for this domain is deflation level 1 was sufficient enough to provide a significant decrease in daily (which will also translate to the respective sizes of weekly, monthly and yearly outputs) output file sizes. Time was not significantly impacted from levels 1 to 3, but did technically speedup with little file size deflation. After level 3, total time increases with no advancement in deflation.

**Note:** When running these simulations, the HPC seemed to be relatively busy so there many be slight variation in these results for you, but the general idea should be the same.

**Recommendation:** When starting work with a new domain, it may be beneficial to compare the file sizes and time differences between deflation level 0, 1 and 2, and increasing if impactful improvements are made. Go with the deflation level that gives you the fastest program run while making notable total size decreases. For example, the results listed above suggest deflation level 1 is best used since it's the only level that significantly decreases the output sizes and total runtime. The most likely domains to benefit from deflation level 1 from 0 would be ones with a significant number of disabled (not simulated) sites.

<hr>
Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
