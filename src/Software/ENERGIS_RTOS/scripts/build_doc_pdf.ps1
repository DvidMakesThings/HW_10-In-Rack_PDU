param(
    [switch]$SkipDoxygen
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Step($msg) { Write-Host "[doc-pdf] $msg" }

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Use absolute paths (do not compose with any other directory)
$doxyfile = 'G:\_GitHub\HW_10-In-Rack_PDU\src\Software\Doxyfile'
$latexDir = 'G:\_GitHub\HW_10-In-Rack_PDU\src\Software\ENERGIS_RTOS\docs\doxygen\latex'

if (-not $SkipDoxygen) {
    if (-not (Test-Path $doxyfile)) { throw "Doxyfile not found at $doxyfile" }
    Write-Step "Running Doxygen..."
    $doxyDir = Split-Path -Parent $doxyfile
    Push-Location $doxyDir
    try {
        doxygen .\Doxyfile | Write-Host
    }
    finally { Pop-Location }
}

if (-not (Test-Path $latexDir)) { throw "LaTeX directory not found: $latexDir" }

# --- Patch LaTeX sources ---
Write-Step "Patching LaTeX sources in $latexDir"

# 1) Fix stray braces and subsection formatting across all .tex
Get-ChildItem $latexDir -Filter *.tex | ForEach-Object {
    $p = $_.FullName
    $text = Get-Content $p -Raw
    $text = $text -replace '\\end\{DoxyEnumerate\}\}', '\\end{DoxyEnumerate}'
    $text = $text -replace '\\end\{DoxyItemize\}\}', '\\end{DoxyItemize}'
    $text = [regex]::Replace($text, '\\doxysubsection\{(\r?\n)', '\\doxysubsection{}$1')
    $text = [regex]::Replace($text, '^(\\\\)(doxysubsection\{\})', '$2', [System.Text.RegularExpressions.RegexOptions]::Multiline)
    Set-Content -Path $p -Value $text -Encoding UTF8
}

# 2) Patch refman.tex (FINAL FIX)
$refman = Join-Path $latexDir 'refman.tex'
if (Test-Path $refman) {
    $r = Get-Content $refman -Raw

    # SOLUTION: force \iffalse to behave as \iftrue (prevents unmatched conditionals)
    if ($r -notmatch '\\let\\iffalse\\iftrue') {
        $r = [regex]::Replace(
            $r,
            '(\\begin\\{document\\})',
            '$1' + "`n\\let\\iffalse\\iftrue"
        )
    }

    Set-Content -Path $refman -Value $r -Encoding UTF8
}

# --- Build PDF ---
Write-Step "Building PDF with lualatex/makeindex passes..."
Push-Location $latexDir
try {
    $env:MAX_PRINT_LINE = 10000
    & lualatex -interaction=nonstopmode refman | Out-Null
    & makeindex refman | Out-Null
    & lualatex -interaction=nonstopmode refman | Out-Null
    & lualatex -interaction=nonstopmode refman | Out-Null
}
finally {
    Pop-Location
}

$pdf = Join-Path $latexDir 'refman.pdf'
if (Test-Path $pdf) {
    Write-Step "Success: $pdf"
    exit 0
}
else {
    Write-Step "Build failed; tail of refman.log:"
    Get-Content (Join-Path $latexDir 'refman.log') -Last 200 | Write-Host
    exit 2
}
