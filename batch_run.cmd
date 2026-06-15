@echo on

REM Set parameters here

set N_RUN=100
set SIZES_LIST=20
set POINTS=8
set CONCENTRATION=15
set NUM_PROC=4
set ALGORITHM=Moore
set OUTPUT_DIR=D:\ansys_results\
set NUM_RND_LOADS=0
set WORKING_DIRECTORY=D:\Project\ansys_results\working_dir\
set PATH_TO_MATVIZ3D=D:\Projects\MatViz3D\build\Desktop_Qt_6_10_2_MinGW_64_bit-Debug\debug\MatViz3d.exe

FOR /F "tokens=*" %%g IN ('cd') do (SET my_pwd=%%g)

REM Run QT Enviroment
call c:\Qt\6.8.0\mingw_64\bin\qtenv2.bat
cd /D %my_pwd%


REM Outer Loop to iterate through sizes list
FOR %%s IN (%SIZES_LIST%) DO (
    echo =================================
    echo Processing SIZE: %%s
    echo =================================

    REM Loop to run the program N_RUN times
    for /L %%i in (1,1,%N_RUN%) do (
        echo Running iteration %%i for Size %%s...
        
        REM Run the application with command line arguments
        
        %PATH_TO_MATVIZ3D% --size %%s --concentration %CONCENTRATION% --algorithm %ALGORITHM% --autostart --nogui --np %NUM_PROC% --output "%OUTPUT_DIR%\result-%%s-%CONCENTRATION%.hdf5" --num_rnd_loads %NUM_RND_LOADS% --run_stress_calc --working_directory %WORKING_DIRECTORY%

        h5ls "%OUTPUT_DIR%\result-%%s-%CONCENTRATION%.hdf5"

        timeout /t 5
        
        REM Add any custom logic here if needed between iterations
    )
)

echo Finished running iterations for all sizes.
pause