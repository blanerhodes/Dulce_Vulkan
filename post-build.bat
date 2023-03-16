@echo off

REM Run from root directory!
if not exist "%cd%\bin\assets\shaders\" mkdir "%cd%\bin\assets\shaders"

echo "Compiling shaders..."

echo "assets/shaders/starter.vert.glsl -> bin/assets/shaders/starter.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert assets/shaders/starter.vert.glsl -o bin/assets/shaders/starter.vert.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets/shaders/starter.frag.glsl -> bin/assets/shaders/starter.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag assets/shaders/starter.frag.glsl -o bin/assets/shaders/starter.frag.spv
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

REM echo "assets/shaders/starter.vert.glsl -> bin/assets/shaders/starter.vert.spv"
REM %VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert assets/shaders/starter.vert.glsl -o bin/assets/shaders/starter.vert.spv
REM IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)
 
REM echo "assets/shaders/starter.frag.glsl -> bin/assets/shaders/starter.frag.spv"
REM %VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag assets/shaders/starter.frag.glsl -o bin/assets/shaders/starter.frag.spv
REM IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "Copying assets..."
echo xcopy "assets" "bin\assets" /h /i /c /k /e /r /y
xcopy "assets" "bin\assets" /h /i /c /k /e /r /y

echo "Done."