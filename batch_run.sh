#!/bin/bash

# --- Parameters configuration ---
N_RUN=100
SIZES_LIST=(20) # Corresponds to SIZES_LIST in cmd 
CONCENTRATION=15
NUM_PROC=4      # Added from cmd logic 
ALGORITHM="Moore"
OUTPUT_DIR="/media/oleksii/Data/ansys_results"
NUM_RND_LOADS=0
WORKING_DIRECTORY="/media/oleksii/Data/ansys_results/working_dir"
PATH_TO_MATVIZ3D="/home/oleksii/MatViz3D/build/Desktop_Qt_6_8_2-Debug/MatViz3D"

# Ensure directories exist
mkdir -p "$OUTPUT_DIR"
mkdir -p "$WORKING_DIRECTORY"

# Outer Loop to iterate through sizes list (equivalent to FOR %%s IN (%SIZES_LIST%)) 
for SIZE in "${SIZES_LIST[@]}"; do
    echo "================================="
    echo "Processing SIZE: $SIZE"
    echo "================================="

    # Inner Loop to run the program N_RUN times 
    for ((i=1; i<=N_RUN; i++)); do
        echo "Running iteration $i for Size $SIZE..."
        
        # Run the application with command line arguments including --np 
        "$PATH_TO_MATVIZ3D" \
            --size "$SIZE" \
            --concentration "$CONCENTRATION" \
            --algorithm "$ALGORITHM" \
            --autostart \
            --nogui \
            --np "$NUM_PROC" \
            --output "$OUTPUT_DIR/result-$SIZE-$CONCENTRATION.hdf5" \
            --num_rnd_loads "$NUM_RND_LOADS" \
            --run_stress_calc \
            --working_directory "$WORKING_DIRECTORY"
        
        # Check HDF5 file structure 
        h5ls "$OUTPUT_DIR/result-$SIZE-$CONCENTRATION.hdf5"
        
        # Wait for 5 seconds between iterations 
        sleep 5
        
        # Add any custom logic here if needed between iterations
    done
done

echo "Finished running iterations for all sizes." 
read -p "Press Enter to exit..."