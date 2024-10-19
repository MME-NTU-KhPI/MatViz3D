@echo on

REM Set parameters here

set N_RUN=100 
set SIZE=5
set CONCENTRATION=5
set ALGORITHM=Moore
set OUTPUT_FILE="z:\ans_proj\matviz\result-%SIZE%-%CONCENTRATION%.hdf5"
set NUM_RND_LOADS=100
set WORKING_DIRECTORY="z:\ans_proj\matviz"
set PATH_TO_MATVIZ3D="v:\temp\build\Desktop_Qt_6_8_0_MinGW_64_bit-Debug\debug\MatViz3d.exe"

FOR /F "tokens=*" %%g IN ('cd') do (SET my_pwd=%%g)

REM Run QT Enviroment
call c:\Qt\6.8.0\mingw_64\bin\qtenv2.bat
cd /D %my_pwd%


REM Loop to run the program N_RUN times
for /L %%i in (1,1,%N_RUN%) do (
    echo Running iteration %%i...
    
    REM Run the application with command line arguments
    
    %PATH_TO_MATVIZ3D% --size %SIZE% --concentration %CONCENTRATION% --algorithm %ALGORITHM% --autostart --nogui --output %OUTPUT_FILE% --num_rnd_loads %NUM_RND_LOADS% --run_stress_calc --working_directory %WORKING_DIRECTORY%

    h5ls %OUTPUT_FILE%

    timeout /t 5
    
    REM Add any custom logic here if needed between iterations
)

echo Finished running %N% iterations.
pause
