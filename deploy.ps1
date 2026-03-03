$src = 'C:\Users\Max Pote\Desktop\patina-plugin\build\Patina_artefacts\Release\VST3\Patina.vst3\Contents\x86_64-win\Patina.vst3'
$destDir = 'C:\Main Plugin Folder\Patina.vst3\Contents\x86_64-win'
$dest = Join-Path $destDir 'Patina.vst3'
$old = Join-Path $destDir 'Patina.vst3.old'
$new = Join-Path $destDir 'Patina_new.vst3'

# Ensure dest dir exists
New-Item -ItemType Directory -Path $destDir -Force | Out-Null

# Copy new build as temp name
Copy-Item $src $new -Force
Write-Output "Copied new build to temp location"

# Rename locked file out of the way
if (Test-Path $dest) {
    try {
        Rename-Item $dest $old -Force -ErrorAction Stop
        Write-Output "Renamed old file"
    } catch {
        Write-Output "Could not rename old - trying direct overwrite"
        Copy-Item $src $dest -Force
        Remove-Item $new -Force -ErrorAction SilentlyContinue
        Write-Output "Direct overwrite done"
        Get-Item $dest | Select-Object Length, LastWriteTime
        exit
    }
}

# Move new into place
Rename-Item $new (Split-Path $dest -Leaf) -Force
Write-Output "Renamed new file into place"

# Clean up old
if (Test-Path $old) { Remove-Item $old -Force -ErrorAction SilentlyContinue }

Write-Output "Deployed successfully"
Get-Item $dest | Select-Object Length, LastWriteTime
