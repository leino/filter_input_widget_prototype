@echo off

REM === environment =======
set vc_install_path="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC"

REM === build target =======
set target_architecture=x64


REM === arguments =======
set source_path=%1
set builds_path=%2
set build_type=%3

REM === constants =======
if %build_type% == debug (
   set buildtype_release=0
   set buildtype_internal=1
   set debuglevel_expensive_checks=1
   set performance_spam_level=0
)
if %build_type% == release (
   set buildtype_release=1
   set buildtype_internal=0
   set debuglevel_expensive_checks=0
   set performance_spam_level=0
)

REM === warnings ======
REM this is disabled because potentially unsafe things are totally fine
set disable_potentially_unsafe_methods_warning_flag=/wd4996
REM this is disabled because I cannot set stuff to positive float infinity if this is on
set disable_overflow_in_constant_arithmetic_warning_flag=/wd4756
REM this is disabled because I like to do while(1) loops
set disable_conditional_expression_is_constant_warning_flag=/wd4127
set disable_function_deprecated_warnings=/wd4995
REM this is not disabled for now, turn it on if it gets too annoying
REM (or just turn it on sometimes when cleaning up)
REM set disable_unreferenced_local_variable_warning_flag=/wd4100


REM TODO: remove first few, we want these warnings!
set disabled_warning_flags=^
    %disable_function_deprecated_warnings%^
    %disable_potentially_unsafe_methods_warning_flag%^
    %disable_overflow_in_constant_arithmetic_warning_flag%^
    %disable_conditional_expression_is_constant_warning_flag%
REM    %disable_unreferenced_local_variable_warning_flag%

set generate_intrinsic_functions_flag=/Oi
set disable_optimizations_flag=/Od
set generate_7_0_compatible_debug_info_flag=/Z7

set optimization_flags =^
    %generate_intrinsic_functions_flag%

set debug_flags=^
    %disable_optimizations_flag%^
    %generate_7_0_compatible_debug_info_flag%

set common_compiler_flags=^
    /nologo^
    /MT^
    /Gm-^
    /EHa-^
    /WX^
    /W4^
    %disabled_warning_flags%

set common_linker_flags=/incremental:no

set output_switches=^
    /Fo%builds_path%\^
    /Fe%builds_path%\^
    /Fd%builds_path%\
    
set libs=^
    user32.lib^
    d3d11.lib^
    winmm.lib
    
set buildtype_def=/DIIR4_WIDGET_BUILDTYPE=%buildtype_internal%
set debuglevel_def=/DIIR4_WIDGET_DEBUGLEVEL=%debuglevel_expensive_checks%
set grid_orientation_horizontal_def=/DGRID_ORIENTATION_HORIZONTAL=0
set grid_orientation_vertical_def=/DGRID_ORIENTATION_VERTICAL=1

set defs=^
    /DIIR4_WIDGET_BUILDTYPE_RELEASE=%buildtype_release%^
    /DIIR4_WIDGET_BUILDTYPE_INTERNAL=%buildtype_internal%^
    /DIIR4_WIDGET_PERFORMANCE_SPAM_LEVEL=%performance_spam_level%^
    /DIIR4_WIDGET_DEBUGLEVEL_EXPENSIVE_CHECKS=%debuglevel_expensive_checks%^
    %grid_orientation_horizontal_def%^
    %grid_orientation_vertical_def%
    
REM make sure that the output direcotry exists
IF NOT EXIST %builds_path% mkdir %builds_path%

REM set the compiler environment variables for 64 bit builds
call %vc_install_path%\"\vcvarsall.bat" %target_architecture%

if %build_type% == debug (
   set build_type_specific_flags=%debug_flags%
)
if %build_type% == release (
   set build_type_specific_flags=%optimization_flags%
)

REM compile release build!
call cl^
     %common_compiler_flags%^
     %output_switches%^
     %build_type_specific_flags%^
     %libs%^
     %defs%^
     %buildtype_def%^
     %debuglevel_def%^
     %source_path%\iir4_input.cpp^
     /link %common_linker_flags% /OUT:%builds_path%\test_%build_type%.exe


REM exit early if compile failed
if %ERRORLEVEL% gtr 0 (
    exit /b %ERRORLEVEL%
    )

set fxc_warnings_are_errors_flag=/WX
set fxc_disable_optimizations_flag=/Od
set fxc_generate_pdb_debug_info_flag=/Zi
set fxc_optimization_level_flag=/O1
set fxc_debug_flags=^
    /nologo^
    %fxc_disable_optimizations_flag%^
    %fxc_generate_pdb_debug_info_flag%^
    %fxc_warnings_are_errors_flag%^
    %grid_orientation_horizontal_def%^
    %grid_orientation_vertical_def%
set fxc_release_flags=^
    /nologo^
    %fxc_optimization_level_flag%^
    %fxc_warnings_are_errors_flag%^
    %grid_orientation_horizontal_def%^
    %grid_orientation_vertical_def%
if %build_type% == debug (
   set fxc_flags=%fxc_debug_flags%
)
if %build_type% == release (
   set fxc_flags=%fxc_release_flags%
)




REM ship the shaders!
call fxc %fxc_flags% %source_path%\shaders.hlsl /T vs_5_0 /E circle_transform /Fo %builds_path%\circle_vs.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )

call fxc %fxc_flags% %source_path%\grid_shaders.hlsl /T vs_5_0 /E grid_vertex_shader /Fo %builds_path%\grid_vs.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )

call fxc %fxc_flags% %source_path%\shaders.hlsl /T vs_5_0 /E colorbar_transform /Fo %builds_path%\colorbar_vs.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )

call fxc %fxc_flags% %source_path%\grid_shaders.hlsl /T vs_5_0 /E font_vertex_shader /Fo %builds_path%\grid_font_vs.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\grid_shaders.hlsl /T ps_5_0 /E font_pixel_shader /Fo %builds_path%\grid_font_ps.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )

call fxc %fxc_flags% %source_path%\shaders.hlsl /T vs_5_0 /E dynamic_transform /Fo %builds_path%\solid_dynamic_vs.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\shaders.hlsl /T vs_5_0 /E plot_transform /Fo %builds_path%\plot_vs.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\shaders.hlsl /T vs_5_0 /E ttf_font_vertex_shader /Fo %builds_path%\ttf_font_vs.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\shaders.hlsl /T ps_5_0 /E solid /Fo %builds_path%\solid_ps.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\shaders.hlsl /T ps_5_0 /E colorbar_magnitude /Fo %builds_path%\colorbar_magnitude_ps.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\shaders.hlsl /T ps_5_0 /E colorbar_colorwheel /Fo %builds_path%\colorbar_colorwheel_ps.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\shaders.hlsl /T ps_5_0 /E density /Fo %builds_path%\density_ps.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\shaders.hlsl /T ps_5_0 /E domain_coloring /Fo %builds_path%\domain_coloring_ps.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )
call fxc %fxc_flags% %source_path%\shaders.hlsl /T ps_5_0 /E ttf_font_pixel_shader /Fo %builds_path%\ttf_font_ps.cso
if %ERRORLEVEL% gtr 0 (exit /b %ERRORLEVEL% )

exit /b %ERRORLEVEL%
