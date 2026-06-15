#!/bin/bash

# --- Parameters configuration ---
N_RUN=100
SIZES_LIST=(20) 
CONCENTRATION=15
NUM_PROC=4
ALGORITHM="Moore"
NUM_RND_LOADS=0

# Updated paths based on your project structure
OUTPUT_DIR="$HOME/Projects/ansys_result"
WORKING_DIRECTORY="$OUTPUT_DIR/working_dir"
PATH_TO_MATVIZ3D="$HOME/MatViz3D/build/Desktop_Qt_6_8_2-Debug/MatViz3D"

# Ensure directories exist
mkdir -p "$OUTPUT_DIR"
mkdir -p "$WORKING_DIRECTORY"

# Outer Loop to iterate through sizes list (Logic from batch_run.cmd)
for SIZE in "${SIZES_LIST[@]}"; do
    echo "================================="
    echo "Processing SIZE: $SIZE"
    echo "================================="

    # Inner Loop to run the program N_RUN times
    for ((i=1; i<=N_RUN; i++)); do
        echo "Running iteration $i for Size $SIZE..."
        
        # Run the application with command line arguments
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
        
        # Add any custom logic here if needed
    done
done

echo "Finished running iterations for all sizes."
read -p "Press Enter to exit..."