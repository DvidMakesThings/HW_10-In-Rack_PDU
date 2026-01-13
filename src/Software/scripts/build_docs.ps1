# Builds Doxygen documentation and compiles a single PDF (refman.pdf)
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_docs.ps1

[CmdletBinding()]
param(
    [string]$DoxyfilePath,
    [string]$OutputDir,
    [switch]$OpenPdf
)

# Resolve script directory robustly (works even if $PSScriptRoot is empty)
$ScriptPath = $MyInvocation.MyCommand.Path
$ScriptDir  = Split-Path -Parent $ScriptPath
$RootDir    = Split-Path -Parent $ScriptDir  # .. from scripts/

if (-not $DoxyfilePath) { $DoxyfilePath = Join-Path $RootDir 'Doxyfile' }
if (-not $OutputDir)    { $OutputDir    = Join-Path $RootDir 'ENERGIS_RTOS\\docs\\doxygen' }

function Ensure-Tool {
    param(
        [Parameter(Mandatory=$true)][string]$Name
    )
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    return [bool]$cmd
}

function Invoke-Doxygen {
    param([string]$Config, [string]$WorkDir)
    Write-Host "Running Doxygen with: $Config" -ForegroundColor Cyan
    Push-Location $WorkDir
    try {
        & doxygen $Config
        if ($LASTEXITCODE -ne 0) {
            throw "Doxygen failed with exit code $LASTEXITCODE"
        }
    }
    finally { Pop-Location }
}

function Build-PDF-LaTeXmk {
    param([string]$LatexDir)
    Push-Location $LatexDir
    try {
        Write-Host "Building PDF with latexmk..." -ForegroundColor Cyan
        & latexmk -pdf -silent -interaction=nonstopmode refman.tex
        if ($LASTEXITCODE -ne 0) { Write-Warning "latexmk failed with exit code $LASTEXITCODE"; return $false }
        return $true
    }
    finally { Pop-Location }
}

function Build-PDF-PdfLaTeX {
    param([string]$LatexDir)
    Push-Location $LatexDir
    try {
        Write-Host "Building PDF with pdflatex/makeindex..." -ForegroundColor Cyan
        & pdflatex -interaction=nonstopmode refman.tex
        if (Test-Path .\refman.idx) {
            & makeindex refman.idx
        }
        & pdflatex -interaction=nonstopmode refman.tex
        & pdflatex -interaction=nonstopmode refman.tex
    }
    finally { Pop-Location }
}

function Fix-LatexKnownIssues {
    param([string]$LatexDir)
    Write-Host "Applying minor LaTeX fixes..." -ForegroundColor Cyan
    # Fix stray closing brace after DoxyEnumerate causing TeX errors
    Get-ChildItem -Path $LatexDir -Filter '*.tex' | ForEach-Object {
        $path = $_.FullName
        $content = Get-Content -Raw -Path $path
        $fixed = $content -replace "\\end\{DoxyEnumerate\}\}", "\\end{DoxyEnumerate}"
        if ($fixed -ne $content) {
            Set-Content -Path $path -Value $fixed
        }
    }
}

# 1) Check tools
if (-not (Ensure-Tool -Name 'doxygen')) { throw "'doxygen' is not available on PATH. Please install Doxygen and retry." }
$hasLatexmk = Ensure-Tool -Name 'latexmk'
$hasPdflatex = Ensure-Tool -Name 'pdflatex'
$hasMakeindex = Ensure-Tool -Name 'makeindex'
$hasTexify   = Ensure-Tool -Name 'texify'
if (-not $hasPdflatex) { throw "'pdflatex' is not available on PATH. Install MiKTeX or TeX Live and retry." }
if (-not $hasMakeindex) { Write-Warning "'makeindex' not found; proceeding without index generation." }

# 2) Run Doxygen
Invoke-Doxygen -Config $DoxyfilePath -WorkDir $RootDir

# 3) Locate LaTeX output
$latexDir = Join-Path $OutputDir 'latex'
if (-not (Test-Path $latexDir)) { throw "LaTeX directory not found: $latexDir" }
$refmanTex = Join-Path $latexDir 'refman.tex'
if (-not (Test-Path $refmanTex)) { throw "refman.tex not found in $latexDir; check Doxygen output and configuration." }

# 4) Build PDF
$built = $false
if ($hasLatexmk) {
    $built = Build-PDF-LaTeXmk -LatexDir $latexDir
}
if (-not $built -and $hasTexify) {
    Write-Host "Building PDF with texify..." -ForegroundColor Cyan
    Push-Location $latexDir
    try {
        & texify.exe --pdf --batch --quiet refman.tex
        if ($LASTEXITCODE -ne 0) { Write-Warning "texify failed with exit code $LASTEXITCODE" }
        else { $built = $true }
    }
    finally { Pop-Location }
}
if (-not $built) {
    Fix-LatexKnownIssues -LatexDir $latexDir
    Build-PDF-PdfLaTeX -LatexDir $latexDir
}

# 5) Verify result
$pdfPath = Join-Path $latexDir 'refman.pdf'
if (-not (Test-Path $pdfPath)) { throw "PDF not found at $pdfPath after build." }
Write-Host "PDF generated: $pdfPath" -ForegroundColor Green

if ($OpenPdf) {
    Write-Host "Opening PDF..." -ForegroundColor Cyan
    Invoke-Item $pdfPath
}
