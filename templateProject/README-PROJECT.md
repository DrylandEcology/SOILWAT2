# SOILWAT2 Template Project


## Main steps for a SOILWAT2 project

1. Create a new project structure
    - Copy `templateProject` and rename with project name,
        e.g., `path/to/myProject/`
    - Copy the default inputs `files.in`, `Input/` and `Input_nc/`
        from `tests/example/` to `myProject/`
    - Create a new folder `myProject/logs`
2. Compile `SOILWAT2` in the git repository with selected flags,
    e.g., `make CPPFLAGS=-SWMPI all`,
3. Copy the compiled `bin/SOILWAT2` to `path/to/myProject/SOILWAT2`
4. At this point, the project folder would have the following structure
    ```
    myProject/
        files.in
        Input/
        Input_nc/
        logs/
        scripts/
        SOILWAT2
    ```
5. Provide inputs specific to your project via files in
    `Input/` and `Input_nc/`
6. Move to `scripts/` and run `SOILWAT2` for your project,
    e.g., via adequately updated `run-sw2_local.sh` or `run-sw2_hpc.sh`
7. Monitor progress of simulation via `progressTally.sh`
8. Post-process simulation output once completed, for instance
    - Review combined logging files, e.g., `collapseLogfiles.sh`
    - Standardize output netCDFs, e.g., `ncSW2Standardize.sh`
    - Calculate derived output and/or concatenate across time,
        e.g., `metric_one.sh`
9. Analyse your data ...!


## Currently available scripts

- `run-sw2_local.sh` -- template script to run a project locally
- `run-sw2_hpc.sh` -- template script to schedule a run on a HPC with slurm
- `progressTally.sh` -- calculate progress of a nc-based SOILWAT2 simulation
- `ncSW2Standardize.sh` -- standardize netCDF files created by SOILWAT2
- `collapseLogfiles.sh` -- copy content of logfiles created by individual
    processes into one combined logfile
- `metric_one.sh` -- calculates summaries and concatenates across time of
    netCDF files created by SOILWAT2
