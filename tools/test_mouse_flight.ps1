# Language: en-GB
# Hidden, loopback-only functional test. No OS input is generated.
param(
    [Parameter(Mandatory=$true)][string]$ClientExe,
    [Parameter(Mandatory=$true)][string]$ServerExe,
    [Parameter(Mandatory=$true)][string]$GameDir,
    [Parameter(Mandatory=$true)][string]$CacheDir,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateSet(30,60,120)][int]$FrameRate=30,
    [ValidateSet('flight','handoff','inverted','turret')][string]$Scenario='flight',
    [string]$PackFile='',
    [ValidateRange(90,300)][int]$Seconds=120
)
$ErrorActionPreference='Stop'
$repo = Split-Path -Parent $PSScriptRoot
$level=if($Scenario -eq 'turret') {'Levels/Guadal_Canal'} else {'Levels/Coral_sea'}
$targetParts=if($Scenario -eq 'turret') {56} else {57}
foreach ($required in @($ClientExe,$ServerExe,$GameDir,$CacheDir)) {
    if (-not (Test-Path -LiteralPath $required)) {throw "Missing test input: $required"}
}
$ClientExe=[IO.Path]::GetFullPath($ClientExe); $ServerExe=[IO.Path]::GetFullPath($ServerExe)
$GameDir=[IO.Path]::GetFullPath($GameDir); $CacheDir=[IO.Path]::GetFullPath($CacheDir)
if (Get-Process -Name bf1943 -ErrorAction SilentlyContinue) {throw 'Another game is running; refusing to interfere.'}
foreach ($port in @(18241,18246,18250)) {
    if (Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue) {throw "Loopback test port $port is occupied."}
}
$run=[IO.Path]::GetFullPath($OutputDirectory)
if ((Test-Path -LiteralPath $run) -and (Get-ChildItem -LiteralPath $run -Force | Select-Object -First 1)) {
    throw 'The output directory must be new or empty; refusing to overwrite existing files.'
}
$user=Join-Path $run 'client'
New-Item -ItemType Directory -Force $run,$user,(Join-Path $run 'server'),(Join-Path $run 'frames') | Out-Null
$settings=@'
install_defaults = 5
launcher_setup = done
player_name_chosen = 1
player_name = LocalTest
player_id = 19430002
bf_startup_cards = false
fullscreen = off
window_width = 960
window_height = 540
vsync = off
cvar.bf_button_prompts = keyboard
cvar.bf_master_volume = 0
cvar.bf_menu_music = false
cvar.bf_share_pipelines = false
cvar.native_x_renderer_hd_textures = true
'@
$settings += "`nframe_rate = $FrameRate`n"
if ($Scenario -eq 'inverted') {$settings += "cvar.bf_mouse_invert_y = true`n"}
Set-Content -LiteralPath (Join-Path $user 'bf1943-settings.txt') -Value $settings -Encoding utf8
if ($PackFile) {
    New-Item -ItemType Directory -Force (Join-Path $user 'hd') | Out-Null
    Copy-Item -LiteralPath $PackFile -Destination (Join-Path $user 'hd\bf1943-hd.bfhd')
}
$environmentNames=@('SDL_WINDOW_ACTIVATE_WHEN_SHOWN','SDL_WINDOW_ACTIVATE_WHEN_RAISED','BF_LOOPBACK_ONLY','BF_LOOPBACK_ADDR','BF_DEV_NO_OPEN','BF_AUTO_JOIN','BF_DEV_PADLOG','BF_DEV_CMDLOG','BF_SEEK_VEHICLE','BF_SEEK_PARTS','BF_SEEK_POINT','BF_VEHICLE_SCRIPT','BF_DEV_FLIGHT_LOG','BF_DEV_VIEWLOG','BF_DEV_VMOUSE','NXR_FRAME_DUMP','BF_DEV_PAD','BF_DEV_MOUSE','BF_DEV_MOUSE_VEHICLE','BF_MENU_STEPS','BF_MATCH_SCRIPT')
$oldEnvironment=@{}
foreach ($name in $environmentNames) {$oldEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$backend=$null; $server=$null; $client=$null
try {
    foreach ($name in @('BF_DEV_PAD','BF_DEV_MOUSE','BF_DEV_MOUSE_VEHICLE','BF_MENU_STEPS','BF_MATCH_SCRIPT')) {[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:SDL_WINDOW_ACTIVATE_WHEN_SHOWN='0'; $env:SDL_WINDOW_ACTIVATE_WHEN_RAISED='0'
    $env:BF_LOOPBACK_ONLY='1'; $env:BF_DEV_NO_OPEN='1'
    $backendArgs=@('-I',"`"$repo\server\plasma.py`"",'--host','127.0.0.1','--registry-host','127.0.0.1','--theater-host','127.0.0.1','--seconds',"$($Seconds+45)",'--log',"`"$run\backend.log`"")
    $backend=Start-Process -FilePath 'python' -ArgumentList $backendArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'backend.stdout.log') -RedirectStandardError (Join-Path $run 'backend.stderr.log')
    Start-Sleep -Seconds 1
    if ($backend.HasExited) {throw 'Local backend failed to start.'}
    $env:BF_LOOPBACK_ADDR='127.0.0.11'
    $serverArgs=@('--server',"--game_data_root=`"$GameDir`"","--user_data_root=`"$run\server`"","--log_file=`"$run\server.log`"",'--window_start=hidden','--fullscreen=false','--mnk_mouse=false',"--cl=`"-dedicated -level $level`"",'--bf_gm_backend=127.0.0.1:18250','--bf_gm_listen=127.0.0.21:1000','--bf_gm_name=LocalTest','--bf_gm_listed=true','--bf_gm_upnp=false','--bf_share_pipelines=false')
    $server=Start-Process -FilePath $ServerExe -WorkingDirectory (Split-Path -Parent $ServerExe) -ArgumentList $serverArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'server.stdout.log') -RedirectStandardError (Join-Path $run 'server.stderr.log')
    $server.PriorityClass='BelowNormal'
    for ($wait=0; $wait -lt 15; ++$wait) {
        if ($server.HasExited) {throw 'The local game server exited.'}
        Start-Sleep -Seconds 1
    }
    $env:BF_LOOPBACK_ADDR='127.0.0.2'; $env:BF_AUTO_JOIN='1'
    $env:BF_DEV_PADLOG='1'; $env:BF_DEV_CMDLOG='1'
    $env:BF_SEEK_VEHICLE='1'; $env:BF_SEEK_PARTS="$targetParts"
    if ($Scenario -eq 'turret') {Remove-Item Env:BF_SEEK_POINT -ErrorAction SilentlyContinue} else {$env:BF_SEEK_POINT='-605.5,74.5,869.9'}
    $env:BF_DEV_FLIGHT_LOG='1'; $env:BF_DEV_VIEWLOG='1'; $env:BF_DEV_VMOUSE='1'
    $script='1:key=W/90,6:mouse=0:-80/0.5,7:mouse=0:80/0.5,14:mouse=200:0/0.5,14.75:mouse=-200:0/0.5,20:mouse=0:80/0.5,20.75:mouse=0:-80/0.5,30:mouse=650:0/0.8,32:mouse=-650:0/0.8,36:mouse=0:-500/0.5,38:mouse=0:500/0.5,44:mouse=0:0/1'
    if ($Scenario -eq 'inverted') {$script='1:key=W/90,6:mouse=0:80/0.5,7:mouse=0:-80/0.5,14:mouse=200:0/0.5,14.75:mouse=-200:0/0.5,20:mouse=0:-80/0.5,20.75:mouse=0:80/0.5,30:mouse=650:0/0.8,32:mouse=-650:0/0.8,36:mouse=0:500/0.5,38:mouse=0:-500/0.5,44:mouse=0:0/1'}
    if ($Scenario -eq 'handoff') {$script='1:key=W/90,6:mouse=0:-80/0.5,7:mouse=0:80/0.5,14:mouse=200:0/0.5,14.75:mouse=-200:0/0.5,20:mouse=0:80/0.5,20.75:mouse=0:-80/0.5,30:mouse=650:0/0.8,30:key=J/0.5,32:mouse=-650:0/0.8,36:mouse=0:500/0.5,36:key=I/0.3,38:mouse=0:-500/0.5,44:mouse=0:0/1'}
    if ($Scenario -eq 'turret') {$script='2:mouse=200:0/0.5,6:mouse=0:-200/0.5,10:mouse=-200:0/0.5,14:mouse=0:200/0.5'}
    $env:BF_VEHICLE_SCRIPT=$script
    $env:NXR_FRAME_DUMP="$(Join-Path $run 'frames'),70,2,1"
    $clientArgs=@("--game_data_root=`"$GameDir`"","--user_data_root=`"$user`"","--cache_root=`"$CacheDir`"","--log_file=`"$run\client.log`"",'--window_start=hidden','--fullscreen=false','--window_width=960','--window_height=540','--mnk_mode=true','--mnk_mouse=false','--bf_smooth_motion=true','--native_x_renderer=true','--bf_engine_d3d_path=true','--gpu_plugin=bfnative','--bf_online_host=127.0.0.1','--bf_backend_registry=127.0.0.1:18250','--bf_share_pipelines=false','--bf_pipelines_url=http://127.0.0.1:9/no-upload','--bf_master_volume=0','--bf_menu_music=false','--bf_button_prompts=keyboard')
    $client=Start-Process -FilePath $ClientExe -WorkingDirectory (Split-Path -Parent $ClientExe) -ArgumentList $clientArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'client.stdout.log') -RedirectStandardError (Join-Path $run 'client.stderr.log')
    $client.PriorityClass='BelowNormal'
    @{language='en-GB'; scenario=$Scenario; requires_target_confirmation=$true; level=$level; target_parts=$targetParts; frame_rate=$FrameRate; duration_seconds=$Seconds; script=$script; client_sha256=(Get-FileHash -LiteralPath $ClientExe -Algorithm SHA256).Hash; runtime_sha256=(Get-FileHash -LiteralPath (Join-Path (Split-Path -Parent $ClientExe) 'rexruntime.dll') -Algorithm SHA256).Hash; server_sha256=(Get-FileHash -LiteralPath $ServerExe -Algorithm SHA256).Hash; os_input_sent=$false; client_pid=$client.Id; server_pid=$server.Id; backend_pid=$backend.Id} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'run.json') -Encoding utf8
    Write-Output "Hidden mouse-flight test: $Scenario at $FrameRate FPS, client PID $($client.Id)."
    if ($client.WaitForExit($Seconds*1000)) {$status="Exited with code $($client.ExitCode)"} else {$status='Reached test deadline'}
    @{language='en-GB'; status=$status; ended_utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'end.json') -Encoding utf8
    Write-Output $status
} finally {
    foreach ($process in @($client,$server,$backend)) {
        if ($process -and -not $process.HasExited) {Stop-Process -Id $process.Id; $process.WaitForExit()}
    }
    foreach ($name in $environmentNames) {[Environment]::SetEnvironmentVariable($name,$oldEnvironment[$name],'Process')}
}
