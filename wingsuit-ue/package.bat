@echo off
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%~dp0Wingsuit.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%~dp0Dist" -unattended -utf8output -ubtargs="-MaxParallelActions=3"
