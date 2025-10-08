#!/bin/bash

# Schedule with SLURM: mpi-mode SOILWAT2 to run project

#SBATCH --account=<account>
#SBATCH --partition=<partition>
#SBATCH --time=<DD-HH:MM:00>
#SBATCH --signal=15@60
#SBATCH --mem=<mem>
#SBATCH --nodes=1
#SBATCH --ntasks=<processes>
#SBATCH --cpus-per-task=1
#SBATCH --hint=nomultithread
#SBATCH --mail-type=ALL
#SBATCH --mail-user=<email>
#SBATCH --job-name=<project>
#SBATCH --output=../logs/%j_log-sw2_hpc-slurm.txt


# Add modules
# module load ...


# Provide run information
echo "------ Setup start ------"
echo "SBATCH_JOB_NAME" ${SBATCH_JOB_NAME}
echo "SBATCH_TIMELIMIT" ${SBATCH_TIMELIMIT}
echo "SLURM_JOB_NODELIST" ${SLURM_JOB_NODELIST}
echo "SLURM_NNODES" ${SLURM_NNODES}
echo "SLURM_NTASKS" ${SLURM_NTASKS}
echo "SLURM_CPUS_ON_NODE" ${SLURM_CPUS_ON_NODE}
echo "SLURM_CPUS_PER_TASK" ${SLURM_CPUS_PER_TASK}
echo "------ Setup end. ------"

echo ""
echo "------ This file dump start ------"
cat "$0"
echo "------ This file dump end. ------"
echo ""


# Run SOILWAT2 project
date

srun ../SOILWAT2 -d ../ -f files.txt \
    > ../logs/$(date +%Y%m%d-%H%M%S)_log-sw2_hpc-run.txt 2>&1

date

module load cdo
srun -n 1 ./progressTally.sh

echo "End of file."
