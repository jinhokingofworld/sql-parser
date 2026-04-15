param(
    [int]$Rows = 1000
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$seedRoot = Join-Path $repoRoot 'tests\fixtures\db'
$dbRoot = Join-Path $env:TEMP ('sql-parser-bench-' + [guid]::NewGuid().ToString('N'))

New-Item -ItemType Directory -Path $dbRoot | Out-Null
New-Item -ItemType Directory -Path (Join-Path $dbRoot 'schema') | Out-Null
New-Item -ItemType Directory -Path (Join-Path $dbRoot 'tables') | Out-Null

Copy-Item (Join-Path $seedRoot 'schema\students.schema') (Join-Path $dbRoot 'schema\students.schema')
Copy-Item (Join-Path $seedRoot 'tables\students.csv') (Join-Path $dbRoot 'tables\students.csv')

& (Join-Path $repoRoot 'sql_processor') --bench $Rows --db $dbRoot
