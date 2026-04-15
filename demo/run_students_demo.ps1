$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$seedRoot = Join-Path $repoRoot 'tests\fixtures\db'
$dbRoot = Join-Path $env:TEMP ('sql-parser-demo-' + [guid]::NewGuid().ToString('N'))

New-Item -ItemType Directory -Path $dbRoot | Out-Null
New-Item -ItemType Directory -Path (Join-Path $dbRoot 'schema') | Out-Null
New-Item -ItemType Directory -Path (Join-Path $dbRoot 'tables') | Out-Null

Copy-Item (Join-Path $seedRoot 'schema\users.schema') (Join-Path $dbRoot 'schema\users.schema')
Copy-Item (Join-Path $seedRoot 'schema\students.schema') (Join-Path $dbRoot 'schema\students.schema')
Copy-Item (Join-Path $seedRoot 'tables\users.csv') (Join-Path $dbRoot 'tables\users.csv')
Copy-Item (Join-Path $seedRoot 'tables\students.csv') (Join-Path $dbRoot 'tables\students.csv')

& (Join-Path $repoRoot 'sql_processor') --sql (Join-Path $repoRoot 'demo\students_demo.sql') --db $dbRoot
