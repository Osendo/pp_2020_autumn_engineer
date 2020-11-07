FROM gitpod/workspace-full
RUN sudo apt-get update && sudo apt-get install -y libboost-all-dev	mpich libmpich-dev libomp-dev openmpi-bin libtbb-dev libopenmpi-dev	&& sudo rm -rf /var/lib/apt/lists/*
