# SOILWAT2 Parallelization

[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2

Note: this document is best viewed as part of the doxygen-built documentation
(there may be text artifacts if viewed as standalone-markdown).

<br>

# Parallelization Guide for v8.2.0 & v8.3.0 & v8.4.0

## v8.3.0

### Goal/Idea
SOILWAT2 has three modes, SWTXT (text output), SWNC/SWNETCDF (netCDF output with/without in/out value conversions), and SWMPI (run simulations in parallel starting in v8.2.0). This version of the parallelization is regarded as an awkward step from the initial parallel version and the next version of the program, which could provide further parallel benefits. This means, there are still limitations to some things like how many processes to spawn. In version v8.2.0, we introduced the idea of splitting spawned processes into two categories - compute & I/O. This created complicated code which can be hard to maintain down the road. Now, the goal is to simplify the code and design to get rid of these designations and replace it with processes that do all input, simulations, and output. Overall functionality stays mostly the same. This v8.3.0 section will contain updates to the previous section (v8.2.0), so if something is not explicitly mentioned, it functions the same as mentioned in said section without any I/O & compute processes.

### Overview of Design
As mentioned in the last section, v8.2.0 has compute & I/O processes. These processes communicate with each other to share input/output. However, this turned out to be complicated code which may be hard to maintain in later versions. So, to solve this, the program now only spawns processes that will do all actions, i.e., input, simulations, and output on their own. This greatly decreases the amount of communication processes need to do throughout a program run.

#### Main Changes
- User no longer needs to worry about how to divide spawned processes into I/O and compute
- Each process creates their own logfile following the format "rank_[rank]_<log file name>", e.g., rank_0_logfile.log
- Each process now reports their own log information to their logfile instead of sending it to an I/O process
- Removed multiple constants
    - N_ITER_BEFORE_OUT
    - MAX_NODE_PROCS
    - PROCS_PER_IO

### Recommendations
- Make sure the product of [n compute procs] * N_SUID_ASSIGN < [n active sites]
- Use as high of N_SUID_ASSIGN value as desired up until 150
- If used on an HPC, use at most 64 cores
- If using a site-oriented domain with gridded input files, try to make coordinates as consistent as possible. Chances are, using an index from site to gridded domains will increase the number of reads necessary to read all data. The worst-case scenario is having latitudes/longitudes that are random, making indices within an index file sporatic, practically eliminating the chance of reducing of the number of reads
    - In general, when using random coordinates or indexing from site to gridded domains, try running a small sample of the program with two different values of N_SUID_ASSIGN, as a smaller value of this constant could provide better performance
    - With that said, it stands that if you do not need to use an index file no matter the consistency of latitude/longitude values, you have the best chance at reducing the time reading inputs due to less but bigger contiguous site reads

### Current Known Limitations/Problems

A few situations can lead to unexpected segmentation faults
    - Insufficient file descriptors: [issue #465](https://github.com/DrylandEcology/SOILWAT2/issues/465)
        - A temporary solution is to increase the number of file descriptors on the system
    - N_SUID_ASSIGN is too large: [issue #469](https://github.com/DrylandEcology/SOILWAT2/issues/469)
        - Depending on exact project setup and compile configurations values of 150-200 for N_SUID_ASSIGN have often caused segfaults
        - This may be related to an issue with caching values by an external library
        - A temporary solution is to decrease N_SUID_ASSIGN

Additional limitations
    - Multiple nodes are not supported: [issue #470](https://github.com/DrylandEcology/SOILWAT2/issues/470)
        - Throughput drops or the program stalls if run on multiple nodes
        - See issue for leading theory of why this is so

### Performance Results

There are two types of performance test conducted - with and without weather inputs. When reading weather from netCDFs, the original hypothesis was that the weather inputs drastically decreased the performance gain since weather inputs require so much reading to take place. This section is split into two subsections - methodology used to test performance and notable results with and without weather inputs.

#### Methodology
- Two sets of performance tests were run, with and without weather input from netCDF files, to get an idea of how the parallel version of the program runs. Since this is an intermediate step between the initial parallel version and the next version of the program, and lack of options to run, not a lot of performance analyses were obtained. The performance runs had the configuration combinations of
    - Domain size: ~112,000 (gridded)
    - Number of cores: 64
    - Number of assigned suids per compute process: 25, 40, 50, 60, 75, 90, 100
    - Number of years: 30

#### Note
Between v8.2.0 and v8.3.0, SWNC/SWNETCDF mode became more similar to how SWMPI mode works. In particular, both modes of v8.3.0 now keep files open the entire program run and only close them at the end. Whether it's the file opening or another reason, SWNC/SWNETCDF mode is now around 4x faster than v8.2.0.

### Results With and Without Weather

No Weather (Figure 1) | Weather (Figure 2)
:----------|-----------:
![Figure 1](Speed_up-no_weather.png) | ![Figure 2](Speed_up-weather.png)

Speedup relative to the number of assigned SUIDs. As expected, the program that does not read in weather (left) provides a bit more speedup than that of weather (right). However, the differences in speedup between the two are now smaller than what they were with v8.2.0 (figures 6 & 7).

No Weather (Figure 3) | Weather (Figure 4)
:----------|-----------:
![Figure 3](Partition_Timing-no_weather.png) | ![Figure 4](Partition_Timing-weather.png)

Partitioning time into two categories - I/O and compute relative to the number of assigned SUIDs. Going off of v8.2.0, we expected an increasing contribution of I/O operations as SUIDs increase. This is not what we see, however. Runs with and without weather show a stable ratio between compute and I/O time at around 75-80% I/O and 20-25% compute. This is in contrast to v8.2.0 (figures 8 & 9 and 10 & 11).

---

## v8.2.0

### Goal/Idea
SOILWAT2 previously had two modes, SWTXT (text output) and SWNC/SWNETCDF (netCDF output with/without in/out value conversions).
NetCDF is the new primary in/output method which provides a lot more functionality than using text files.
Currently, SWNC/SWNETCDF sequentially runs through the sites, reading, simulating, outputting in that order until every site has been simulated.
Generally, this is quite fast when using it on smaller domain sizes, but our use case is with domains on the larger side (hundreds of thousands to millions of sites).
The only way to simulate this many sites in a reasonable amount of time is running these simulations in parallel.
Using Open MPI, SOILWAT2 now has a new compilation mode - SWMPI - that will run sites in parallel given a set of cores and other compilation settings/values the user can provide (see the section constants for more information on the compilation settings/values).

### Overview of Design
The program has been split up into two different categories of process - I/O and compute. The names are self-explanatory where I/O processes handle all input and output operations, where compute processes just crunch numbers for simulations.<br>
Processes that do purely I/O control a chunk of the compute processes by providing the information they receive. The I/O processes will send their compute processes input information for each site they want to simulate, then compute processes will simulate the information, send back the output to their I/O process group all site output together and output to file (see figure 1 for a visual of one iteration of the cycle). This cycle will happen until the entirety of the domain is finished or until an interrupt from the user/computer is received. In that case, the program will stop as soon as it can. When the program is run next, the rest of the simulations will be detected and run using the same process.
<br>

![Figure 1](Compute_IO_cycle.png)

### Recommendations
- Make sure the product of [n compute procs] * N_SUID_ASSIGN < [n active sites]
- Scale the number of I/O processes with the amount of input will be read. For example, if weather is not to be read, use less I/O processes, whereas if weather is read, use more I/O processes
- If used on an HPC, use at most one node, or 128 cores
- If using a site-oriented domain with gridded input files, try to make coordinates as consistent as possible. Chances are, using an index from site to gridded domains will increase the number of reads necessary to read all data. The worst-case scenario is having latitudes/longitudes that are random, making indices within an index file sporatic, practically eliminating the chance of reducing of the number of reads
    - In general, when using random coordinates or indexing from site to gridded domains, try running a small sample of the program with two different values of N_SUID_ASSIGN, as a smaller value of this constant could provide better performance
    - With that said, it stands that if you do not need to use an index file no matter the consistency of latitude/longitude values, you have the best chance at reducing the time reading inputs due to less but bigger contiguous site reads

### Additional HPC Usage
#### Exiting Early
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

#### References
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

### Notable Program Internals (for developers)
The following workflow is an abstracted view of execution and is executed until all sites are simulated or the program is signaled to be shutdown.

#### Logging
- I/O processes will error for three reasons
    - netCDF library error
    - Input value setting/checking error
    - A problem occurs with Open MPI
- I/O processes will report their error to their respective log file

#### Strings and NetCDFs
- In the normal version of netCDFs (serial), we can write strings (NC_STRING) and text (NC_CHAR).
- In parallel netCDF, we cannot create strings since the library wants fixed-length
    strings, so we must use text.
- This unfortunately does not simply mean creating a statically sized array of characters
    and having variable-length strings within, they must *all* be the same *size*.

#### Input
The new parallel mode has required a slightly different method of reading inputs.
In concept, when an I/O process’ product of # compute processes and N_SUID_ASSIGN > 1 (i.e., \<\# compute processes\> * N_SUID_ASSIGN), the I/O process needs to read in more than one site’s input to distribute to its compute processes.
To properly do this without too much addition/rewriting of netCDF inputs, I/O processes will read in multiple sites at once in a long string of values, and set the values to an instance of run time input structs to distribute to the compute processes.
More specifically, using a scenario of 1 I/O to 2 compute processes and 2 N_SUID_ASSIGN what would happen is
- Create four instances of run inputs
- Read in a specific input key, e.g., model
- Read in four sites of data
- Loop through all relevant data within the read site data and set it into the respective input structure

##### Notes
The reason we need to loop over the inputs across all four sites is because the dimensions of the variable may not be ideal (e.g., site dimension last time=100, …, site=2, instead of site dimension first, site=2, time=100, etc.).
The code will keep track of these dimensions and loop over the site data and store it in the respective input structs
Plain netCDF (SWNC/SWNETCDF mode) inputs remain the same to read one site at a time

#### Output
- Previous Problem(s)
    - Output using the current netCDF method (i.e., one site at a time) is not very dependable. That is, with contiguous writes, it has been proven that we cannot simply order the outputs [all site 1 data], [all site 2 data], ..., [all site n data]. This is *mainly* due to time being split of output files time domain (e.g., 20 years, and 10 years).

- Current Solution(s)
    - If we want to write contiguous site data across netCDFs, this data is rearranged into the
      following format:
        - [file 1, var 1, site 1], [file 1, var 1, site 2], ..., [file 1, var 2, site 1], ...
          [file 2, var 1, site 1], [file 2, var 1, site 2], ..., [file 2, var 2, site 1]
    - The netCDF output function has been modified to use this format while also keeping
      functionality for the previous format (without MPI).

##### Notes
- In theory, if the "Inf" stride output years is used for output (i.e., only one netCDF
    per timestep), no rearranging is required.

### Current Known Limitations/Problems

A few situations can lead to unexpected segmentation faults
    - Insufficient file descriptors: [issue #465](https://github.com/DrylandEcology/SOILWAT2/issues/465)
        - A temporary solution is to increase the number of file descriptors on the system
    - N_SUID_ASSIGN is too large: [issue #469](https://github.com/DrylandEcology/SOILWAT2/issues/469)
        - Depending on exact project setup and compile configurations values of 150-200 for N_SUID_ASSIGN have often caused segfaults
        - This may be related to an issue with caching values by an external library
        - A temporary solution is to decrease N_SUID_ASSIGN

Additional limitations
    - Multiple nodes are not supported: [issue #470](https://github.com/DrylandEcology/SOILWAT2/issues/470)
        - Throughput drops or the program stalls if run on multiple nodes
    - N_ITER_BEFORE_OUT > 1 is not supported


### Performance Results

There are two type of performance test conducted - with and without weather inputs. When reading weather, the original hypothesis was that the weather inputs drastically decreased the performance gain since weather inputs require so much reading to take place. This section is split into two sections - methodology used to test performance and notable results with and without weather inputs.

#### Methodology
- Three main performance test runs were run to get an idea of how the parallel version of the program runs. The first batch of performance runs had the configuration combinations of
    - Domain sizes: 1x1, 5x5, 10x10, 25x25, and 50x50
    - Number of cores: 2, 10, 20, 50, and 128
        - If a domain size is too small for a certain number of cores, then the maximum number of cores they use is coined as “max cores”
    - Number of assigned suids per compute process: 1, 5, 10, 20, and 40
    - Number of I/O processes: 1, 2, and 3
        - If a number of I/O is too large for the total number of cores to split the core distribution at most 50/50, then the I/O size is not tested for the domain size

- The second batch consisted of non-weather-inclusive performance runs that took place used a domain size of ~112k sites (excluding sites which error), a size no configuration will be able to engulf in one iteration of input-simulation-output. This batch uses the configuration combinations of
    - Domain size: ~112,000
    - Number of cores: 128
    - Number of suids per compute process: 25, 40, 50, 60, 75, 90, and 100
    - Number of I/O processes: 1, 2, 3, 5, 10, 20, 40, and 64

- The metrics measured were
    - Speedup - Classic parallel speedup equation - [sequential time] / [parallel time]
    - Efficiency - Classic parallel efficiency equation - [speedup] / [# cores]
        - With more complexity than the usual parallel conversion, there are two ways this has been measured, for the most part using the equation above
            - Without suid consideration - Attempt to represent the pure parallel efficiency without the additional speedup of # assigned suids
[speed] / [# cores * # assigned suids]
        - With suid consideration - Take the numbers of speedup at face value and calculate the efficiency as is (depending on the setup, can easily be above 1)
    - Sequential/parallel compute/I/O time partition % (second/third batch only) - gives the idea of how much overall time is spent doing compute and I/O operations. This can help to get a sense of the correct balance of compute and I/O creation

- The third batch consisted of weather-inclusive (30 years) performance runs that also took place used a domain size of ~112k sites (excluding sites which error). This batch uses the configuration combinations of
    - Domain size: ~112,000
    - Number of cores: 128
    - Number of suids per compute process: 10, 20, 40, 50, 60
    - Number of I/O processes: 20, 40, 64
    - Note: A couple supplemental performance tests were provided showing 40 years worth of weather under almost exact combinations as 30 years, but not as in-depth when it comes to the visualization

- The metrics measured were the same metrics as the non-weather-inclusive were used for this batch

An important thing to keep in mind is the idea that the number of assigned suids per compute process has been the main driver of the performance gain. As you will see in the results found when the number of assigned suids, pure parallelization would not be enough to give the required boost. This is a result of how the program is highly dependent on I/O operations, or in other words, is I/O-bound by nature due to reading/writing from netCDF files with relatively light computations during simulations.

### Results With and Without Weather

![Figure 2](Speedup_1_I_O-1_Suid_Per_Compute-2_Cores-no-weather.png)

The speedup using 2 total processes (without weather), 1 I/O and 1 compute, with 1 suid assigned per compute process. This allows us to see how well the pure parallel version fares against the theoretical maximum speedup as the domain size increases. This graph shows with the use of these 2 total compute processes as the number of sites increases, it trends towards the maximum theoretical speedup (2x).

![Figure 3](Speedup_withI_O-1_Suid_Per_Compute-Max_Cores-no-weather.png)

Speedup as the domain sizes increase (without weather) with maximum number of sites where # sites >= # sites in domain. Every line represents a number of I/O processes used, with the number of assigned suids per compute process being 1. As shown above, as the domain size increases, the higher the number of I/O process, the better the program performs, and no configuration matters with lower domain sizes.

No Weather (Figure 4) | Weather (Figure 5)
:----------|-----------:
![Figure 4](Speedup_Relative_to_Assigned_Suids-no-weather.png) | ![Figure 5](weather/Speedup_Relative_to_Assigned_Suids-weather.png)

Speedup as the number of assigned suids per compute process increases. Multiple lines represent a different number of I/O processes. (no weather, left) With a lower number of assigned suids, there is no obvious pattern of which number of I/O processes is best, with a range of speedup ~75x to ~160x with 25 assigned suids. On the other end, the higher the number assigned suids, 64 I/O is the most obvious speedup at ~425x, with other I/O sizes being more mixed in their results. (With weather, right) We can see the performance does not peak for all three amounts of I/O processes. We see a big jump from 20 to 40 assigned suids for 40 & 64 I/O processes.

No Weather (Figure 6) | Weather (Figure 7)
:----------|-----------:
![Figure 6](Speedup_Relative_to_I_O-no-weather.png) | ![Figure 7](Speedup_Relative_to_I_O-weather.png)

Speedup as the number of I/O processes increases. Multiple lines represent a different number of assigned suids. (Without weather, left) Performance does not differ a lot between the number of assigned suids with a lower number of I/O processes. On the contrary, with 64 I/O processes, the more assigned suids, the better the performance gain. (With weather, right) For the most part, performance does not rely heavily on the number of I/O processes as we increase the number of assigned suids.

No Weather (Figure 8) | Weather (Figure 9)
:----------|-----------:
![Figure 8](Partition_Timing-25_Assigned_Suids-no-weather.png) | ![Figure 9](Partition_Timing-10_Assigned_Suids-weather.png)

Partitioned timing into compute and I/O process with 25 (without weather) assigned suids and 10 (with weather) per compute process. (Without weather, left) We see an unexpected increase in the I/O times and decrease in compute times as the number of I/O processes increase. (With weather, right) With 20 I/O processes, the program is already saturated with I/O operations and even more so with 64 I/O processes.

No Weather (Figure 10) | Weather (Figure 11)
:----------|-----------:
![Figure 10](Partition_Timing-100_Assigned_Suids-no-weather.png) | ![Figure 11](Partition_Timing-60_Assigned_Suids-weather.png)

Partitioned timing into compute and I/O process with 100 (without weather) and 60 (with weather) assigned suids per compute process. (Without weather, left) The graph mostly shows an unexpected increase in the I/O times and decrease in compute times as the number of I/O processes increase. The exception is with 64 I/O processes, a small increase in compute timing compared to 40 I/O processes. (With weather, right) 20 I/O is more saturated with compute operations and relative to **Figure 9** is less saturated with compute operations at 64 I/O processes.

![Figure 12](Speedup_Relative_to_Assigned_Suids-weather_40years.png)

As expected, with 40 years worth of weather, we see that relative to **Figure 5**, there is a slight but noticable decrease in speedup. Also as expected, there is a noticable difference in speedup from 40 to 64 I/O processes as the number of assigned suids increases.
<br>
<hr>
Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
