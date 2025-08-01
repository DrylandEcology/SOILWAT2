# SOILWAT2 Parallelization

[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2

Note: this document is best viewed as part of the doxygen-built documentation
(there may be text artifacts if viewed as standalone-markdown).

<br>

# Parallelization Guide
## Goal/Idea
SOILWAT2 previously had two modes, SWTXT (text output) and SWNC/SWNETCDF (netCDF output with/without in/out value conversions)
NetCDF is the new primary in/output method which provides a lot more functionality than using text files
Currently, SWNC/SWNETCDF sequentially runs through the sites, reading, simulating, outputting in that order until every site has been simulated
Generally, this is quite fast when using it on smaller domain sizes, but our use case is with domains on the larger side (hundreds of thousands to millions of sites)
The only way to simulate this many sites in a reasonable amount of time is running these simulations in parallel
Using Open MPI, SOILWAT2 now has a new compilation mode - SWMPI - that will run sites in parallel given a set of cores and other compilation settings/values the user can provide (see the section constants for more information on the compilation settings/values)
A general idea of what needs to happen is:

## Current Known Limitations/Problems
### Will Not/Cannot Fix
- N_ITER_BEFORE_OUT > 1
- Attempting to write too many values in netCDF output function
    - It has been observed that writing, or storing too many values in cache before syncing the values to file, will result in a segmentation fault
    - This has a direct correlation to N_SUID_ASSIGN as we will attempt to write more values the bigger N_SUID_ASSIGN gets before syncing values to file
    - There is no evidence that the bound before a segmentation fault increases with more I/O processes
    - Though, for example, # I/O = 1, # comp = 127, N_SUID_ASSIGN = 100 may act fine, but the same setup with # I/O = 2 may throw a segmentation fault
    - Though this is not desirable, it is believed that this is purely an underlying problem with the libraries on Hovenweep
    - See current recommendations for the recommendations based on this

### Exiting Early
Timeout
- Add flag --signal=[{R|B}:]\<sig_num\>[\@sig_time] to the "sbatch" command
    - "sig_time" is the number of seconds before the end of process time
    - We will not be using the R or B option so examples would be
        - sbatch --signal=2@60 \<program\> (send SIGINT 60 before end of allotted time)
        - sbatch --signal=15@15 \<program\> (send SIGTERM 15 seconds before end of allotted time)
scancel
- Add flag --signal=\<signal_name or signal_id\>
    - scancel \<proc id\> --signal=2 (cancel proc id and send a SIGINT)
    - scencel \<proc id\> --signal=15 (cancel proc id and send a SIGTERM)

### References
- Resort to the SLURM documentation for "scancel" and "sbatch" for any further questions
    - sbatch: https://slurm.schedmd.com/sbatch.html
    - scancel: https://slurm.schedmd.com/scancel.html
### Constants
N_ITER_BEFORE_OUT (not used)
- Number of iterations of output gathered by an I/O process before
  outputting all values.
- An iteration is defined as the product of number of compute processes
  and N_SUID_ASSIGN number of outputs gathered.
- E.g., N_ITER_BEFORE_OUT = 3, N_SUID_ASSIGN = 4, n comp procs = 2
    - Iter 1: SUIDs 0-7
    - Iter 2: SUIDs 8-15
    - Iter 3: SUIDs 16-23
    - Write output values gathered in iter 1-3 (SUIDs 0-23)
- This constant defaults to **1** but can be overwritten by the user
  when compiling the program, i.e., ... CPPFLAGS="... -DN_ITER_BEFORE_OUT=[n iterations] ..." …

MAX_NODE_PROCS
 - Maximum number of processes that can be spawned per compute node or
   on a local CPU; in other words, specifies the number of available
   CPU cores on a compute node (HPC), or processor on a local/personal
   computer.
 - This constant defaults to **128** but can be overwritten by the user
   when compiling the program, i.e., ... CPPFLAGS="... -DMAX_NODES_PROCS=[n max nodes] ..." ...

SW_MPI_NIO
- Maximum number of I/O processes that will be assigned per compute node
- If the ratio of compute-to-I/O processes is less than 1, the program
  will auto-adjust so that at -most- half of the spawned processes in a
  compute node are I/O.
    - E.g., n processes = 10, SW_MPI_NIO = 7, program assigns 5 compute and 5 I/O processes.
- This constant defaults to **2** but can be overwritten by the user
  when compiling the program, i.e., ... CPPFLAGS="... -DSW_MPI_NIO=[n I/O processes] ..." ...

PROCS_PER_IO
- The maximum number of compute processes that can be assigned
  to an I/O process.
- Covers the conditions
    - MAX_NODE_PROCS < SW_MPI_NIO (MAX_NODE_PROCS / SW_MPI_NIO < 1)
    - SW_MPI_NIO > MAX_NODE_PROCS / 2.
- The above conditions can result in a lack of space results in a segmentation fault,
  so, if a case above is true, then we default this constant (PROCS_PER_IO) to
  SW_NODE_PROCS / 2.
    - This will result in unused storage, however should not be noticeable in any manner.
- If neither of the conditions are true, and MAX_NODE_PROCS and SW_MPI_NIO are reasonable values, then we simply divide the number of theoretical compute processes (MAX_NODE_PROCS - SW_MPI_NIO) by the number of I/O processes and add one to be safe  on ample storage for I/O processes to store compute ranks.

N_SUID_ASSIGN
Specifies the number of suids that are assigned to a compute process in an input-simulate-output cycle
For example, if N_SUID_ASSIGN = 10, # compute = 5 and # I/O = 1
The I/O process will read in 50 sites of data at once and distribute 10 sites to the 5 compute processes

## Notable Program Internals (for developers)
Abstracted view of execution
The following workflow is executed until all sites are simulated or the program is signalled to be shutdown

### Logging
- I/O processes will error for two reasons
- netCDF library error
- Input value setting/checking error
- A problem occurs with Open MPI
- I/O processes will report their error to their respective log file
Strings and NetCDFs
- In the normal version of netCDFs (serial), we can write strings (NC_STRING) and
    text (NC_CHAR).
- In parallel netCDF, we cannot create strings since the library wants fixed-length
    strings, so we must use text.
- This unfortunately does not simply mean creating a statically sized array of characters
    and having variable-length strings within, they must *all* be the same *size*

### Input
The new parallel mode has required a slightly different method of reading inputs
In concept, when an I/O process’ multiplication of # compute processes and N_SUID_ASSIGN > 1 (i.e., \<\# compute processes\> * N_SUID_ASSIGN), the I/O process needs to read in more than one site’s input to distribute to its compute processes
To properly do this without too much addition/rewriting of netCDF inputs, I/O processes will read in multiple sites at once in a long string of values, and set the values to an instance of run time input structs to distribute to the compute processes
More specifically, using a scenario of 1 I/O to 2 compute processes and 2 N_SUID_ASSIGN
Create four instances of run inputs
Read in a specific input key, e.g., model
Read in four sites of data
Loop through all relevant data within the read site data and set it into the respective input structure

#### Notes
The reason we need to loop over the inputs across all four sites is because the dimensions of the variable may not be ideal (e.g., site dimension last time=100, …, site=2, instead of site dimension first, site=2, time=100, etc.)
The code will keep track of these dimensions and loop over the site data and store it in the respective input structs
Plain netCDF (SWNC/SWNETCDF mode) inputs remain the same to read one site at a time

### Output
- Previous Problem(s)
    - Output using the current netCDF method (i.e., one site at a time) is not very dependable. That is, with contiguous writes, it has been proven that we cannot simply order the outputs [all site 1 data], [all site 2 data], ..., [all site n data]. This is *mainly* due to time being split of output files time domain (e.g., 20 years, and 10 years).

- Current Solution(s)
    - If we want to write contiguous site data across netCDFs, this data is rearranged into the
      following format:
        - [site 1, var 1, file 1], [site 1, var 1, file 2], ..., [site 1, var 2, file 1], ...
          [site 2, var 1, file 1], [site 2, var 1, file 2], ..., [site x, var y, file z]
    - The netCDF output function has been modified to use this format while also keeping
      functionality for the previous format (without MPI).

#### Notes
- In theory, if the "Inf" stride output years is used for output (i.e., only one netCDF
    per timestep), no rearranging is required.

## Recommendations
- Do not change the value of `N_ITER_BEFORE_OUT`
- Make sure the product of [n compute procs] * N_SUID_ASSIGN < [n active sites]
- The higher the # I/O processes used, the less N_SUID_ASSIGN should be or vice versa, where as N_SUID_ASSIGN grows, shrink the # of I/O processes
- Scale the number of I/O processes with the amount of input will be read. For example, if weather is not to be read, use less I/O processes, whereas if weather is read, use more I/O processes

## Performance Results

There is one type of performance test conducted - without weather inputs. When reading weather, the original hypothesis was that the weather inputs drastically decreased the performance gain since weather inputs require so much reading to take place (will be added at a later date). This section is split into two sections - methodology used to test performance and notable results without weather inputs.

### Methodology
- Two main performance test runs without weather occurred. The first batch of performance runs had the configuration combinations of
    - Domain sizes: 1x1, 5x5, 10x10, 25x25, and 50x50
    - Number of cores: 2, 10, 20, 50, and 128
        - If a domain size is too small for a certain number of cores, then the maximum number of cores they use is coined as “max cores”
    - Number of assigned suids per compute process: 1, 5, 10, 20, and 40
    - Number of I/O processes: 1, 2, and 3
        - If a number of I/O is too large for the total number of cores to split the core distribution at most 50/50, then the I/O size is not tested for the domain size

- The second method batch of non-weather-inclusive performance runs took place using a domain size of ~111k sites (excluding sites which error), a size no configuration will be able to engulf in one iteration of input-simulation-output. This batch uses the configuration combinations of
    - Domain size: ~111,000
    - Number of cores: 128
    - Number of suids per compute process: 25, 40, 50, 60, 75, 90, and 100
    - Number of I/O processes: 1, 2, 3, 5, 10, 20, 40, and 64

The metrics measured were
- Speedup - Classic parallel speedup equation - [sequential time] / [parallel time]
- Efficiency - Classic parallel efficiency equation - [speedup] / [# cores]
    - With more complexity than the usual parallel conversion, there are two ways this has been measured, for the most part using the equation above
        - Without suid consideration - Attempt to represent the pure parallel efficiency without the additional speedup of # assigned suids
[speed] / [# cores * # assigned suids]
        - With suid consideration - Take the numbers of speedup at face value and calculate the efficiency as is (depending on the setup, can easily be above 1)
    - Sequential/parallel compute/I/O time partition % (second batch only) - gives the idea of how much overall time is spent doing compute and I/O operations. This can help to get a sense of the correct balance of compute and I/O creation

An important thing to keep in mind is the idea that the number of assigned suids per compute process has been the main driver of the performance gain. As you will see in the results found when the number of assigned suids, pure parallelization would not be enough to give the required boost. This is a result of how the program is highly dependent on I/O operations, or in other words, is I/O-bound by nature due to reading/writing from netCDF files with relatively light computations during simulations.

## Results Without Weather

![Figure 1](Speedup_1_I_O-1_Suid_Per_Compute-2_Cores.png)

The speedup using 2 total processes, 1 I/O and 1 compute, with 1 suid assigned per compute process. This allows us to see how well the pure parallel version fares against the theoretical maximum speedup as the domain size increases. This graph shows with the use of these 2 total compute processes as the number of sites increases, it trends towards the maximum theoretical speedup (2x).

![Figure 2](Speedup_withI_O-1_Suid_Per_Compute-Max_Cores.png)

Speedup as the domain sizes increase with maximum number of sites where # sites >= # sites in domain. Every line represents a number of I/O processes used, with the number of assigned suids per compute process being 1. As shown above, as the domain size increases, the higher the number of I/O process, the better the program performs, and no configuration matters with lower domain sizes.

![Figure 3](Speedup_Relative_to_Assigned_Suids.png)

Speedup as the number of assigned suids per compute process increases. Multiple lines represent a different number of I/O processes. With a lower number of assigned suids, there is no obvious pattern of which number of I/O processes is best, with a range of speedup ~75x to ~160x with 25 assigned suids. On the other end, the higher the number assigned suids, 64 I/O is the most obvious speedup at ~425x, with other I/O sizes being more mixed in their results.

![Figure 4](Speedup_Relative_to_I_O.png)

Speedup as the number of I/O processes increases. Multiple lines represent a different number of assigned suids. Performance does not differ a lot between the number of assigned suids with a lower number of I/O processes. On the contrary, with 64 I/O processes, the more assigned suids, the better the performance gain.

![Figure 5](Partition_Timing-25_Assigned_Suids.png)

Partitioned timing into compute and I/O process with 25 assigned suids per compute process. The graph shows the expected increase in the compute times and decrease in I/O times as the number of I/O processes increase.

![Figure 6](Partition_Timing-100_Assigned_Suids.png)

Partitioned timing into compute and I/O process with 100 assigned suids per compute process. The graph mostly shows the expected increase in the compute times and decrease in I/O times as the number of I/O processes increase. The exception is with 64 I/O processes, a small increase in I/O timing compared to 40 I/O processes.
<br>
<hr>
Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
