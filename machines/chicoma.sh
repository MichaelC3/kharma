# LANL Machines: HPC and IC

# Chicoma
if [[ "$HOST" == "ch-fe"* && "$ARGS" == *"cuda"* ]]; then
  echo "MUST BE COMPILED ON A GPU NODE!"
  exit
fi
if [[ "$HOST" == "ch-fe"* || "$HOST" == "nid00"* ]]; then
  HOST_ARCH="ZEN2"
  NPROC=64

  # Cray environments get confused easy
  # Make things as simple as possible
  # TODO ONLY NVHPC WORKS
  module purge
  export CRAY_CPU_TARGET="x86-64"
  if [[ "$ARGS" == *"cuda"* ]]; then
    DEVICE_ARCH="AMPERE80"
    if [[ "$ARGS" == *"gnu"* ]]; then
      module load PrgEnv-gnu cudatoolkit
    elif [[ "$ARGS" == *"gcc"* ]]; then
      module load PrgEnv-nvhpc gcc
      C_NATIVE=gcc
      CXX_NATIVE=g++
    elif [[ "$ARGS" == *"intel"* ]]; then
      module load PrgEnv-intel
    elif [[ "$ARGS" == *"nvc++"* ]]; then
      module load PrgEnv-nvhpc
      EXTRA_FLAGS="-DCMAKE_CUDA_COMPILER=$SOURCE_DIR/bin/nvc++-wrapper -DCMAKE_CUDA_COMPILER_ID=NVHPC -DCMAKE_CUDA_COMPILER_VERSION=11.6 $EXTRA_FLAGS"
      C_NATIVE=nvc
      CXX_NATIVE=nvc++
    else
      module load PrgEnv-nvhpc
    fi
    module load craype-accel-nvidia80
    # GPU runtime opts
    MPI_EXTRA_ARGS="--cpu-bind=mask_cpu:0x0*16,0x1*16,0x2*16,0x3*16 $SOURCE_DIR/bin/select_gpu_chicoma"
    unset OMP_NUM_THREADS
    unset OMP_PROC_BIND
    unset OMP_PLACES

    # Sometimes device-side buffers don't work
    # if [conditions]
    EXTRA_FLAGS="-DPARTHENON_ENABLE_HOST_COMM_BUFFERS=ON $EXTRA_FLAGS"
    #else
    #export MPICH_GPU_SUPPORT_ENABLED=1

  else
    module load PrgEnv-aocc
    MPI_EXTRA_ARGS="--cpus-per-task=2"
  fi
  module load cmake cray-hdf5-parallel

  # Runtime opts
  MPI_EXE="srun"
  MPI_NUM_PROCS=""
fi
