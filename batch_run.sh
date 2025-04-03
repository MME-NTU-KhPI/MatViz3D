#!/bin/bash

# Set parameters here
N_RUN=100
SIZE=25
CONCENTRATION=5
ALGORITHM="Moore"
OUTPUT_FILE="/media/oleksii/Data/ans_proj/matviz/result-Mg-${SIZE}-${CONCENTRATION}.hdf5"
NUM_RND_LOADS=200
WORKING_DIRECTORY="/media/oleksii/Data/ans_proj/matviz-workdir"
PATH_TO_MATVIZ3D="/home/oleksii/MatViz3D/build/Desktop_Qt_6_8_2-Debug/MatViz3D"


# Loop to run the program N_RUN times
for ((i=1; i<=N_RUN; i++)); do
    echo "Running iteration $i..."
    
    # Run the application with command line arguments
    "$PATH_TO_MATVIZ3D" --size "$SIZE" --concentration "$CONCENTRATION" --algorithm "$ALGORITHM" \
        --autostart --nogui --output "$OUTPUT_FILE" --num_rnd_loads "$NUM_RND_LOADS" \
        --run_stress_calc --working_directory "$WORKING_DIRECTORY"
    
    h5ls "$OUTPUT_FILE"
    
    sleep 5
    
    # Add any custom logic here if needed between iterations

done

echo "Finished running $N_RUN iterations."
read -p "Press Enter to exit..."
