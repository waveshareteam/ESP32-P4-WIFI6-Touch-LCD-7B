[CmdletBinding()]
param([string]$Port = '', [switch]$ListOnly, [switch]$SelfTest)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Repo = 'waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B'
$Board = 'ESP32-P4-WIFI6-Touch-LCD-7B'
$FlashLimit = 32MB
$DefaultStartIndex = 1
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$Specs = @(
    '00_board_check','01_how_to_create_project','02_hello_world','03_i2c_tools','04_sdmmc',
    '05_wifistation','06_i2s_codec','07_color_panel','08_lvgl_display_panel','09_lvgl_demo_v9','11_esp_brookesia_phone','12_usb_extend_screen','13_rs485_test',
    '14_twai_transmit','15_nvs_counter','16_freertos_tasks','17_system_monitor','18_mp4_player'
)
$ExcludedSpecs = @()
$Items = @()
foreach ($name in $Specs) {
    $configs = if ($name -eq '04_sdmmc') { @('default','format_on_mount_failure') } elseif ($name -eq '12_usb_extend_screen') { @('default','esp32_p4_function_ev_board','no_hid_uac','without_hid','without_uac') } else { @('default') }
    foreach ($version in @('v5.5.5','v6.0.2')) {
        foreach ($config in $configs) {
            $Items += [pscustomobject]@{ Index=$Items.Count+1; Workflow='esp-idf-examples.yml'; Artifact="firmware-esp-idf-$name-$version-$config-rev3_x"; Framework='esp-idf'; Version=$version; ConfigId=$config; Profile='rev3_x'; SourceProject="examples/esp-idf/$name" }
        }
    }
}
function Test-Port([string]$Value) { return $Value -match '^COM\d+$' }
function Test-RelativePackagePath([string]$Root, [string]$Relative) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative)) { return $false }
    $prefix = [IO.Path]::GetFullPath($Root).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    return [IO.Path]::GetFullPath((Join-Path $Root $Relative)).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)
}
function Get-FileSha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-ChipRevisionCode([string]$Probe) { $match=[regex]::Match($Probe,'(?im)revision\s+v?(\d+)\.(\d+)');if(-not $match.Success){throw 'Port probe did not provide a parseable chip revision.'};return ([int]$match.Groups[1].Value * 100 + [int]$match.Groups[2].Value) }
function Assert-ProfileMatchesChip([string]$Profile,[string]$Probe) { if($Probe -notmatch 'ESP32-P4'){throw 'Port probe did not prove an ESP32-P4.'};$revision=Get-ChipRevisionCode $Probe;if(($Profile -eq 'rev1_3' -and $revision -ge 300) -or ($Profile -eq 'rev3_x' -and $revision -lt 300)){throw "Selected profile $Profile does not match detected chip revision v$revision."};return $revision }
function Test-PackageManifest([string]$PackageDir,$Item,[string]$Sha) {
    $manifestPath=Join-Path $PackageDir 'manifest.json';if(-not(Test-Path -LiteralPath $manifestPath -PathType Leaf)){throw 'Package manifest.json is missing.'};$manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json
    if($manifest.schema_version -ne 2 -or $manifest.board -ne $Board -or $manifest.chip -ne 'esp32p4' -or $manifest.framework -ne 'esp-idf' -or $manifest.framework_version -ne $Item.Version -or $manifest.config_id -ne $Item.ConfigId -or $manifest.profile -ne $Item.Profile -or $manifest.source_project -ne $Item.SourceProject -or $manifest.git_sha -ne $Sha -or $manifest.flash.baud -ne 921600 -or $manifest.flash.size_bytes -ne $FlashLimit){throw 'Package manifest identity is not safe for the selected item.'}
    $plan=@();$offsets=@{};foreach($file in @($manifest.files)){$relative=[string]$file.archive_path;if(-not(Test-RelativePackagePath $PackageDir $relative) -or [string]$file.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or [int64]$file.size -le 0 -or [string]$file.offset -notmatch '^0x[0-9a-fA-F]+$'){throw 'Manifest contains unsafe flash metadata.'};$path=Join-Path $PackageDir $relative;if(-not(Test-Path $path -PathType Leaf) -or (Get-FileSha256 $path) -ne [string]$file.sha256 -or [int64](Get-Item $path).Length -ne [int64]$file.size){throw 'Manifest checksum or size verification failed.'};$offset=[Convert]::ToInt64($file.offset.Substring(2),16);if($offsets.ContainsKey($offset) -or $offset+$file.size -gt $FlashLimit){throw 'Manifest contains duplicate or out-of-range flash data.'};$offsets[$offset]=$true;$plan += [pscustomobject]@{Offset=$offset;Size=[int64]$file.size;Path=$path}}
    $ordered=@($plan|Sort-Object Offset);if($ordered.Count -lt 1){throw 'Package contains no flashable files.'};for($i=1;$i -lt $ordered.Count;$i++){if($ordered[$i-1].Offset+$ordered[$i-1].Size -gt $ordered[$i].Offset){throw 'Package contains overlapping flash ranges.'}};return $ordered
}
function Get-NextProgress([int]$Current, [int[]]$Confirmed) {
    $all = @($Confirmed + $Current | Where-Object { $_ -ge 1 -and $_ -le $Items.Count } | Sort-Object -Unique)
    return [pscustomobject]@{ CurrentIndex=if($Current -lt $Items.Count){$Current+1}else{$Current}; ConfirmedIndexes=$all; Completed=($Current -eq $Items.Count) }
}
function Get-StateForFinalSha($Saved, [string]$Sha, [string]$DefaultPort) {
    if (-not $Saved -or -not $Saved.PSObject.Properties['FinalSha'] -or -not $Saved.PSObject.Properties['CurrentIndex'] -or [string]$Saved.FinalSha -ne $Sha) { return [pscustomobject]@{CurrentIndex=$DefaultStartIndex;ConfirmedIndexes=@();Port=$DefaultPort} }
    $current = [int]$Saved.CurrentIndex
    if ($current -lt 1 -or $current -gt $Items.Count) { throw 'Saved progress is outside the item range.' }
    return [pscustomobject]@{CurrentIndex=$current;ConfirmedIndexes=@($Saved.ConfirmedIndexes | ForEach-Object {[int]$_} | Where-Object {$_ -ge 1 -and $_ -le $Items.Count} | Sort-Object -Unique);Port=$DefaultPort}
}

if ($SelfTest) {
    $profileMismatches = @($Items | Where-Object { $_.Profile -ne 'rev3_x' -or $_.Artifact -notmatch '-rev3_x$' })
    if ($Items.Count -ne 46 -or @($Items.Artifact | Sort-Object -Unique).Count -ne 46 -or $profileMismatches.Count -ne 0) { throw 'Self-test item contract failed.' }
    $exampleRoot = Join-Path $RepoRoot 'examples\esp-idf'
    $discovered = @(Get-ChildItem -LiteralPath $exampleRoot -Directory | Where-Object {
        (Test-Path -LiteralPath (Join-Path $_.FullName 'CMakeLists.txt') -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $_.FullName 'main') -PathType Container)
    } | ForEach-Object { $_.Name } | Sort-Object)
    $declared = @($Specs + $ExcludedSpecs)
    $uniqueDeclared = @($declared | Sort-Object -Unique)
    $inventoryDiff = @(Compare-Object $uniqueDeclared $discovered)
    $missingExclusions = @($ExcludedSpecs | Where-Object { $_ -notin $discovered })
    if ($uniqueDeclared.Count -ne $declared.Count -or $inventoryDiff.Count -ne 0 -or $missingExclusions.Count -ne 0) {
        throw 'Self-test example inventory or exclusions do not match the repository.'
    }
    $counts = @{}; foreach ($item in $Items) { $counts[$item.SourceProject] = 1 + [int]$counts[$item.SourceProject] }
    $regularExamplePairs=@($counts.GetEnumerator()|Where-Object {$_.Key -like 'examples/esp-idf/*' -and $_.Value -eq 2})
    if ($counts['examples/esp-idf/04_sdmmc'] -ne 4 -or $counts['examples/esp-idf/12_usb_extend_screen'] -ne 10 -or $regularExamplePairs.Count -ne 16) { throw 'Self-test configuration cardinality failed.' }
    if ((Test-RelativePackagePath 'C:\package' '..\escape.bin') -or (Test-RelativePackagePath 'C:\package' 'C:\escape.bin') -or -not (Test-RelativePackagePath 'C:\package' 'bin\app.bin')) { throw 'Self-test safe-path contract failed.' }
    $reset = Get-StateForFinalSha ([pscustomobject]@{FinalSha='old';CurrentIndex=2;ConfirmedIndexes=@(1)}) 'new' ''
    if ($reset.CurrentIndex -ne 1 -or @($reset.ConfirmedIndexes).Count -ne 0) { throw 'Self-test SHA state reset failed.' }
    if((Get-ChipRevisionCode 'Chip is ESP32-P4 (revision v1.3)') -ne 103 -or (Get-ChipRevisionCode 'ESP32-P4 revision v1.10') -ne 110 -or (Get-ChipRevisionCode 'ESP32-P4 revision v3.0') -ne 300){throw 'Self-test revision parser failed.'}
    Assert-ProfileMatchesChip 'rev1_3' 'ESP32-P4 revision v1.10'|Out-Null;Assert-ProfileMatchesChip 'rev3_x' 'ESP32-P4 revision v3.0'|Out-Null
    foreach($case in @(@('rev1_3','ESP32-P4 revision v3.0'),@('rev3_x','ESP32-P4 revision v1.3'),@('rev1_3','ESP32-P4 revision unknown'))){try{Assert-ProfileMatchesChip $case[0] $case[1]|Out-Null}catch{continue};throw 'Self-test profile mismatch or malformed revision failed.'}
    $testRoot=Join-Path ([IO.Path]::GetTempPath()) ("waveshare-flasher-selftest-"+[guid]::NewGuid().ToString('N'));$packageDir=Join-Path $testRoot 'package';New-Item -ItemType Directory -Path (Join-Path $packageDir 'bin') -Force|Out-Null
    try {
        $bin=Join-Path $packageDir 'bin\\app.bin';[IO.File]::WriteAllBytes($bin,[byte[]](1,2,3));$sha=Get-FileSha256 $bin;$item=[pscustomobject]@{Version='v5.5.5';ConfigId='default';Profile='rev1_3';SourceProject='examples/esp-idf/00_board_check'};$finalSha='a'*40
        $manifest=[ordered]@{schema_version=2;board=$Board;chip='esp32p4';framework='esp-idf';framework_version=$item.Version;config_id=$item.ConfigId;profile=$item.Profile;source_project=$item.SourceProject;git_sha=$finalSha;flash=[ordered]@{baud=921600;size_bytes=$FlashLimit};files=@([ordered]@{archive_path='bin/app.bin';sha256=$sha;size=3;offset='0x10000'})}
        $writeManifest={param($value) $value|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $packageDir 'manifest.json') -Encoding utf8}
        & $writeManifest $manifest;$plan=Test-PackageManifest $packageDir $item $finalSha;if(@($plan).Count -ne 1){throw 'Self-test valid package manifest failed.'}
        $other=Join-Path $packageDir 'bin\\other.bin';[IO.File]::WriteAllBytes($other,[byte[]](4,5));$otherSha=Get-FileSha256 $other
        function Assert-ManifestRejected([string]$Label,[scriptblock]$Mutate) {
            $candidate=$manifest|ConvertTo-Json -Depth 6|ConvertFrom-Json;& $Mutate $candidate;& $writeManifest $candidate;$rejected=$false
            try { Test-PackageManifest $packageDir $item $finalSha|Out-Null } catch { $rejected=$true }
            if(-not $rejected){throw "Self-test manifest $Label rejection failed."}
        }
        Assert-ManifestRejected 'chip mismatch' {param($candidate)$candidate.chip='esp32c6'}
        Assert-ManifestRejected 'profile mismatch' {param($candidate)$candidate.profile='rev3_x'}
        Assert-ManifestRejected 'unsafe path' {param($candidate)$candidate.files[0].archive_path='../escape.bin'}
        Assert-ManifestRejected 'unsafe hash' {param($candidate)$candidate.files[0].sha256='not-a-sha256'}
        Assert-ManifestRejected 'unsafe size' {param($candidate)$candidate.files[0].size=0}
        Assert-ManifestRejected 'duplicate offset' {param($candidate)$candidate.files=@($candidate.files[0],[pscustomobject]@{archive_path='bin/other.bin';sha256=$otherSha;size=2;offset='0x10000'})}
        Assert-ManifestRejected 'overlapping range' {param($candidate)$candidate.files=@($candidate.files[0],[pscustomobject]@{archive_path='bin/other.bin';sha256=$otherSha;size=2;offset='0x10001'})}
        Assert-ManifestRejected 'out-of-range offset' {param($candidate)$candidate.files=@($candidate.files[0],[pscustomobject]@{archive_path='bin/other.bin';sha256=$otherSha;size=2;offset='0x2000000'})}
    } finally { Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue }
    Write-Output 'SELF_TEST_OK items=46 state-reset=ok safe-paths=ok revision-profiles=ok manifest=ok'
    return
}
if ($ListOnly) {
    Write-Output 'finalSHA=resolved-at-runtime'
    Write-Output 'defaultPort=auto-detect-at-runtime'
    Write-Output 'count=46'
    foreach ($item in $Items) { Write-Output ('{0}: workflow={1} artifact={2} source={3} config={4} profile={5}' -f $item.Index,$item.Workflow,$item.Artifact,$item.SourceProject,$item.ConfigId,$item.Profile) }
    return
}

$StateRoot = Join-Path $env:LOCALAPPDATA 'Waveshare\ESP32-P4-WIFI6-Touch-LCD-7B\ci-firmware'
$StatePath = Join-Path $StateRoot 'state-v1.json'
function Resolve-Executable([string]$Name, [string[]]$Fallbacks) { $command=Get-Command $Name -ErrorAction SilentlyContinue|Select-Object -First 1; if($command -and $command.Source){return $command.Source}; foreach($candidate in $Fallbacks){if(Test-Path -LiteralPath $candidate -PathType Leaf){return $candidate}}; throw "$Name was not found." }
function Resolve-Git { return Resolve-Executable 'git' @((Join-Path ${env:ProgramFiles} 'Git\cmd\git.exe'),'C:\Git\cmd\git.exe','D:\Git\cmd\git.exe') }
function Resolve-Gh { return Resolve-Executable 'gh' @((Join-Path ${env:ProgramFiles} 'GitHub CLI\gh.exe')) }
function Resolve-PythonWithEsptool {
    $candidates=@(); foreach($name in @('python','py')){$command=Get-Command $name -ErrorAction SilentlyContinue|Select-Object -First 1;if($command -and $command.Source){$candidates+=$command.Source}}
    foreach($candidate in @($candidates|Select-Object -Unique)){& $candidate -c 'import esptool' *> $null;if($LASTEXITCODE -eq 0){return $candidate}}
    throw 'No Python interpreter with esptool was found.'
}
function Resolve-DefaultPort {
    $ports=@(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue | Where-Object {$_.PNPDeviceID -match '(?i)VID_303A' -and $_.Name -match '\(COM\d+\)'} | ForEach-Object {[regex]::Match($_.Name,'\((COM\d+)\)').Groups[1].Value} | Sort-Object -Unique)
    if($ports.Count -eq 1){return $ports[0]}; throw 'Unable to identify exactly one VID_303A USB serial port; pass -Port COMx.'
}
function Resolve-FinalSha([string]$Git) {$sha=(& $Git -C $RepoRoot rev-parse HEAD 2>&1|Out-String).Trim();if($LASTEXITCODE -ne 0 -or $sha -notmatch '^[0-9a-fA-F]{40}$'){throw 'Unable to resolve a full local HEAD SHA.'};return $sha.ToLowerInvariant()}
function Assert-ReadySource([string]$Git) {$status=(& $Git -C $RepoRoot status --porcelain=v1 --untracked-files=all 2>&1|Out-String);if($LASTEXITCODE -ne 0 -or -not [string]::IsNullOrWhiteSpace($status)){throw 'Refusing to continue: the working tree must be clean.'};$branch=(& $Git -C $RepoRoot symbolic-ref --quiet --short HEAD 2>&1|Out-String).Trim();if($LASTEXITCODE -ne 0 -or -not $branch){throw 'Refusing to continue: check out a non-detached branch first.'};return $branch}
function Assert-ReadyPullRequest([string]$Gh,[string]$Branch,[string]$Sha) {$raw=(& $Gh pr list --repo $Repo --head $Branch --state open --limit 2 --json number,state,isDraft,headRefName,headRefOid 2>&1|Out-String);if($LASTEXITCODE -ne 0){throw 'Unable to query the current pull request.'};$prs=@($raw|ConvertFrom-Json);if($prs.Count -ne 1 -or $prs[0].isDraft -or [string]$prs[0].state -ine 'OPEN' -or $prs[0].headRefName -ne $Branch -or $prs[0].headRefOid -ne $Sha){throw 'Exactly one ready open pull request at the complete local HEAD is required.'}}
function Resolve-ArtifactRun([string]$Gh,[string]$Sha,[string]$Workflow) {$raw=(& $Gh run list --repo $Repo --workflow $Workflow --commit $Sha --status success --limit 20 --json databaseId,headSha,createdAt 2>&1|Out-String);if($LASTEXITCODE -ne 0){throw "Unable to list successful $Workflow runs."};$runs=@($raw|ConvertFrom-Json|Where-Object {$_.headSha -eq $Sha}|Sort-Object createdAt -Descending);if($runs.Count -lt 1){throw "No successful $Workflow workflow run exists for local HEAD $Sha."};return [string]$runs[0].databaseId}
function New-RunPaths {if(-not(Test-Path $StateRoot)){New-Item -ItemType Directory -Path $StateRoot|Out-Null};$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff';$root=Join-Path $StateRoot "runs\$stamp";if(Test-Path $root){throw 'Refusing to overwrite an existing timestamped extraction directory.'};New-Item -ItemType Directory -Path $root|Out-Null;return [pscustomobject]@{Root=$root;Log=(Join-Path $root 'flash.log')}}
function Invoke-CurrentFlash($Item,[string]$SelectedPort,[string]$Gh,[string]$Python,[string]$Sha,[string]$Run) {
    $probe=(& $Python -m esptool --chip esp32p4 --port $SelectedPort chip_id 2>&1|Out-String);if($LASTEXITCODE -ne 0){throw 'Port probe failed.'};$firstRevision=Assert-ProfileMatchesChip $Item.Profile $probe
    $paths=New-RunPaths;$download=(& $Gh run download $Run --repo $Repo --name $Item.Artifact --dir $paths.Root 2>&1|Out-String);if($LASTEXITCODE -ne 0){throw "Artifact download failed with exit code $LASTEXITCODE."};Add-Content -LiteralPath $paths.Log -Value $download
    $zips=@(Get-ChildItem -LiteralPath $paths.Root -Recurse -Filter '*.zip' -File);if($zips.Count -ne 1){throw 'Downloaded artifact must contain exactly one ZIP.'};$package=Join-Path $paths.Root 'package';if(Test-Path $package){throw 'Refusing to overwrite extraction directory.'};Expand-Archive -LiteralPath $zips[0].FullName -DestinationPath $package -ErrorAction Stop;$plan=Test-PackageManifest $package $Item $Sha
    $secondProbe=(& $Python -m esptool --chip esp32p4 --port $SelectedPort chip_id 2>&1|Out-String);if($LASTEXITCODE -ne 0){throw 'Pre-flash port probe failed.'};$secondRevision=Assert-ProfileMatchesChip $Item.Profile $secondProbe;if($secondRevision -ne $firstRevision){throw 'Chip revision changed between validation probes; refusing to flash.'}
    $arguments=@('-m','esptool','--port',$SelectedPort,'--chip','esp32p4','--baud','921600','write_flash');foreach($entry in $plan){$arguments+=('0x{0:X}' -f $entry.Offset);$arguments+=$entry.Path};$output=(& $Python @arguments 2>&1|Out-String);$exit=$LASTEXITCODE;Add-Content -LiteralPath $paths.Log -Value $output;return [pscustomobject]@{Success=($exit -eq 0 -and $output.Contains('Hash of data verified'));Output=$output;LogPath=$paths.Log}
}

$Git=Resolve-Git;$Sha=Resolve-FinalSha $Git;$Branch=Assert-ReadySource $Git;$Gh=Resolve-Gh;Assert-ReadyPullRequest $Gh $Branch $Sha;$Python=Resolve-PythonWithEsptool;if(-not $Port){$Port=Resolve-DefaultPort};$Port=$Port.Trim().ToUpperInvariant();if(-not(Test-Port $Port)){throw 'Port must be COM followed by digits.'}
Add-Type -AssemblyName System.Windows.Forms;Add-Type -AssemblyName System.Drawing
$saved=if(Test-Path $StatePath){Get-Content $StatePath -Raw|ConvertFrom-Json}else{$null};$state=Get-StateForFinalSha $saved $Sha $Port;$script:CurrentIndex=$state.CurrentIndex;$script:Confirmed=@($state.ConfirmedIndexes);$script:FlashVerified=$false
function Save-State {[pscustomobject]@{FinalSha=$Sha;CurrentIndex=$script:CurrentIndex;ConfirmedIndexes=$script:Confirmed;Port=$portBox.Text.Trim().ToUpperInvariant();UpdatedAt=(Get-Date).ToString('o')}|ConvertTo-Json|Set-Content -LiteralPath $StatePath -Encoding utf8}
$form=New-Object Windows.Forms.Form;$form.Text='CI Firmware Flasher';$form.StartPosition='CenterScreen';$form.ClientSize=New-Object Drawing.Size(850,640);$form.FormBorderStyle='FixedDialog';$form.MaximizeBox=$false
function Add-Label($text,$x,$y,$width=810){$label=New-Object Windows.Forms.Label;$label.Text=$text;$label.Location=New-Object Drawing.Point($x,$y);$label.Size=New-Object Drawing.Size($width,20);$form.Controls.Add($label);return $label}
[void](Add-Label "Repository: $Repo" 15 15);[void](Add-Label "Final SHA: $Sha" 15 40);[void](Add-Label 'Port:' 15 70 45);$portBox=New-Object Windows.Forms.TextBox;$portBox.Text=$state.Port;$portBox.Location=New-Object Drawing.Point(65,67);$form.Controls.Add($portBox);$current=Add-Label '' 15 100;$status=Add-Label 'Status: flash one item, test the board, then explicitly mark PASS.' 15 125
$list=New-Object Windows.Forms.ListBox;$list.Font=New-Object Drawing.Font('Consolas',9);$list.Location=New-Object Drawing.Point(15,155);$list.Size=New-Object Drawing.Size(820,260);$form.Controls.Add($list);$output=New-Object Windows.Forms.TextBox;$output.Multiline=$true;$output.ReadOnly=$true;$output.ScrollBars='Both';$output.Location=New-Object Drawing.Point(15,425);$output.Size=New-Object Drawing.Size(820,140);$form.Controls.Add($output)
$flash=New-Object Windows.Forms.Button;$flash.Text='Flash current';$flash.Location=New-Object Drawing.Point(15,580);$form.Controls.Add($flash);$pass=New-Object Windows.Forms.Button;$pass.Text='Mark PASS and advance';$pass.Location=New-Object Drawing.Point(150,580);$pass.Size=New-Object Drawing.Size(200,30);$pass.Enabled=$false;$form.Controls.Add($pass);$exit=New-Object Windows.Forms.Button;$exit.Text='Exit';$exit.Location=New-Object Drawing.Point(715,580);$form.Controls.Add($exit)
function Update-Display {$item=$Items[$script:CurrentIndex-1];$current.Text="Current: $($item.Index)/$($Items.Count) $($item.Artifact)";$list.Items.Clear();foreach($entry in $Items){$mark=if($script:Confirmed -contains $entry.Index){'[PASS]'}elseif($entry.Index -eq $script:CurrentIndex){'[CURRENT]'}else{'[WAIT]'};[void]$list.Items.Add("$mark $($entry.Index): $($entry.Artifact)")};$list.SelectedIndex=$script:CurrentIndex-1}
function Flash-Current {$chosen=$portBox.Text.Trim().ToUpperInvariant();if(-not(Test-Port $chosen)){[Windows.Forms.MessageBox]::Show('Port must be COM followed by digits.');return};$script:FlashVerified=$false;$pass.Enabled=$false;$item=$Items[$script:CurrentIndex-1];try{$run=Resolve-ArtifactRun $Gh $Sha $item.Workflow;$result=Invoke-CurrentFlash $item $chosen $Gh $Python $Sha $run;$output.Text="Log: $($result.LogPath)`r`n$($result.Output)";if($result.Success){$script:FlashVerified=$true;$pass.Enabled=$true;$status.Text='Status: hash verified. Chip silicon revision is not PCB/electrical revision proof; perform the board test, then explicitly mark PASS.'}else{$status.Text='Status: flash did not provide exit 0 and Hash of data verified.'}}catch{$output.Text=$_|Out-String;$status.Text='Status: error; item was not advanced.'}}
$flash.Add_Click({Flash-Current});$pass.Add_Click({if(-not $script:FlashVerified){return};$next=Get-NextProgress $script:CurrentIndex $script:Confirmed;$script:CurrentIndex=$next.CurrentIndex;$script:Confirmed=@($next.ConfirmedIndexes);$script:FlashVerified=$false;Save-State;Update-Display;$pass.Enabled=$false;$status.Text=if($next.Completed){'Status: all items were manually confirmed.'}else{'Status: advanced after human PASS; select Flash current when ready.'}});$exit.Add_Click({$form.Close()});$list.Add_SelectedIndexChanged({if($list.SelectedIndex -ne ($script:CurrentIndex-1)){$list.SelectedIndex=$script:CurrentIndex-1}});Update-Display;[void]$form.ShowDialog()
