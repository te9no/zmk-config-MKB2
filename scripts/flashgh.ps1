param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('JOY', 'R')]
    [string]$Side = 'JOY'
)

# --------------------------------------------
# Set file paths
# --------------------------------------------
$flashScriptPath = Join-Path "scripts" "flash.ps1"
$artifactsDownloadDir = Join-Path "build" "gh"

if (-not (Test-Path $flashScriptPath)) {
    Write-Error "Flash script not found at path: $flashScriptPath"
    exit 1
}

# --------------------------------------------
# Get repository information
# --------------------------------------------
$gitUrl = git remote get-url MKB2
$gitUrl -match 'github\.com/(?<user>[^/]+)/(?<repo>[^/]+)\.git'
if (-not $Matches) {
    Write-Error "Failed to extract repository information from git remote get-url origin"
    exit 1
}
$repoOwner = $Matches.user
$repoName = $Matches.repo
Write-Host "Detected repository: $repoOwner/$repoName"

# --------------------------------------------
# Init artifacts download directory
# --------------------------------------------
if (Test-Path $artifactsDownloadDir) {
    Write-Host "Removing old artifacts download directory: $artifactsDownloadDir"
    Remove-Item -Recurse -Force $artifactsDownloadDir
}

New-Item -ItemType Directory -Path $artifactsDownloadDir | Out-Null

# --------------------------------------------
# Fetch latest successful workflow run
# --------------------------------------------
Write-Host "Fetching latest successful workflow run for $repoOwner/$repoName (branch v0.2.1_RZT-keytoggle)..."
$latestRunId = (gh run list --repo "$repoOwner/$repoName" --branch "v0.2.1_RZT-keytoggle" --json databaseId --limit 1 | ConvertFrom-Json).databaseId

if (-not $latestRunId) {
    Write-Error "Failed to get the latest successful workflow run ID. Make sure 'gh' CLI is installed and authenticated, and the workflow/branch names are correct."
    exit 1
}

Write-Host "Latest successful run ID: $latestRunId"

# --------------------------------------------
# Download artifact
# --------------------------------------------
$artifactName = "firmware"
Write-Host "Downloading artifact(s) from run ID: $latestRunId to $artifactsDownloadDir"
gh run download $latestRunId --repo "$repoOwner/$repoName" --dir $artifactsDownloadDir -n $artifactName

# --------------------------------------------
# Find uf2 file
# --------------------------------------------
# $uf2FilePattern = "*_$($Side)*.uf2"
$uf2FilePattern = "*_$($Side)*.uf2"
$firmwareFile = Get-ChildItem -Path $artifactsDownloadDir -Filter $uf2FilePattern | Select-Object -First 1

if (-not $firmwareFile) {
    Write-Error "Firmware file matching pattern '$uf2FilePattern' (for Side: $Side) not found in '$artifactsDownloadDir'."
    Write-Host "Available files in $artifactsDownloadDir :"
    Get-ChildItem -Path $artifactsDownloadDir | ForEach-Object { Write-Host $_.FullName }
    exit 1
}

Write-Host "Found firmware file: $($firmwareFile.FullName)"

# --------------------------------------------
# Flash firmware
# --------------------------------------------
& $flashScriptPath -Uf2File $($firmwareFile.Name) -Side $Side -BuildDir $artifactsDownloadDir