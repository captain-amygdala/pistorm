set quartus_bin_path=C:\intelFPGA_lite\20.1\quartus\bin64
set piaddress=192.168.1.144

%quartus_bin_path%\quartus_sh --flow compile pistorm
if %errorlevel% neq 0 GOTO ERRORCOMPILE

%quartus_bin_path%\quartus_cpf -c -q 100KHz -g 3.3 -n p output_files\pistorm.pof bitstream.svf
if %errorlevel% neq 0 GOTO ERRORSVF

echo y | pscp -l pi -pw raspberry -P 22 bitstream.svf %piaddress%:./pistorm/bitstream.svf
if %errorlevel% neq 0 GOTO ERRORSCP

echo y | plink -l pi -pw raspberry -P 22 %piaddress% "cd pistorm && ./nprog.sh"
if %errorlevel% neq 0 GOTO ERRORPROG

goto done

:ERRORCOMPILE
echo "ERROR COMPILE"
goto done

:ERRORSVF
echo "ERROR SVF"
goto done

:ERRORSCP
echo "ERROR SCP"
goto done

:ERRORPROG
echo "ERROR PROGRAMM"

:DONE
