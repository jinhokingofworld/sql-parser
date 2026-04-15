param(
    [ValidateSet("all", "unit", "integration", "verify", "bptree-contract")]
    [string] $Suite = "all",
    [switch] $Build,
    [switch] $IncludeLargeVerify
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $false

function Invoke-NativeChecked {
    param(
        [string] $FilePath,
        [string[]] $Arguments = @()
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "native command failed: $FilePath $($Arguments -join ' ')"
    }
}

function Invoke-NativeExpectFailure {
    param(
        [string] $FilePath,
        [string[]] $Arguments = @()
    )

    $quotedArgs = @("""$FilePath""")
    foreach ($argument in $Arguments) {
        $quotedArgs += """$argument"""
    }

    & cmd /c (($quotedArgs -join " ") + " 1>nul 2>nul")
    if ($LASTEXITCODE -eq 0) {
        throw "native command was expected to fail: $FilePath $($Arguments -join ' ')"
    }
}

function Normalize-Text {
    param(
        [string] $Text
    )

    if ($null -eq $Text) {
        return ""
    }

    return ($Text -replace "`r`n", "`n").TrimEnd("`n")
}

function Assert-TextEqual {
    param(
        [string] $Expected,
        [string] $Actual,
        [string] $Label
    )

    if ((Normalize-Text -Text $Expected) -ne (Normalize-Text -Text $Actual)) {
        throw "output mismatch: $Label"
    }
}

function New-IntegrationDb {
    $tempRoot = Join-Path $env:TEMP ("sql-parser-integration-" + [guid]::NewGuid().ToString())
    $schemaDir = Join-Path $tempRoot "schema"
    $tableDir = Join-Path $tempRoot "tables"

    New-Item -ItemType Directory -Force -Path $schemaDir | Out-Null
    New-Item -ItemType Directory -Force -Path $tableDir | Out-Null
    Copy-Item -LiteralPath "tests/fixtures/db/schema/users.schema" -Destination (Join-Path $schemaDir "users.schema")
    Copy-Item -LiteralPath "tests/fixtures/db/tables/users.csv" -Destination (Join-Path $tableDir "users.csv")

    return $tempRoot
}

function Invoke-IntegrationCase {
    param(
        [string] $SqlPath,
        [string] $ExpectedPath
    )

    $dbRoot = New-IntegrationDb
    try {
        $actual = & ".\sql_processor.exe" --sql $SqlPath --db $dbRoot | Out-String
        $expected = Get-Content -Raw -LiteralPath $ExpectedPath
        Assert-TextEqual -Expected $expected -Actual $actual -Label $SqlPath
        Write-Host "[PASS] $SqlPath"
    } finally {
        if (Test-Path -LiteralPath $dbRoot) {
            Remove-Item -LiteralPath $dbRoot -Recurse -Force
        }
    }
}

function Invoke-PythonChecked {
    param(
        [string[]] $Arguments
    )

    & python @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "python failed: python $($Arguments -join ' ')"
    }
}

function Run-UnitSuite {
    Invoke-NativeChecked -FilePath ".\tests\unit\test_tokenizer.exe"
    Invoke-NativeChecked -FilePath ".\tests\unit\test_parser.exe"
    Invoke-NativeChecked -FilePath ".\tests\unit\test_storage.exe"
    Invoke-NativeChecked -FilePath ".\tests\unit\test_executor.exe"
}

function Run-BptreeContractSuite {
    Invoke-NativeChecked -FilePath ".\tests\unit\test_bptree_contract.exe"
}

function Run-IntegrationSuite {
    Invoke-IntegrationCase -SqlPath "tests/integration/insert_select.sql" -ExpectedPath "tests/integration/insert_select.expected"
    Invoke-IntegrationCase -SqlPath "tests/integration/select_where.sql" -ExpectedPath "tests/integration/select_where.expected"
    Invoke-IntegrationCase -SqlPath "tests/integration/select_order_by.sql" -ExpectedPath "tests/integration/select_order_by.expected"
    Invoke-IntegrationCase -SqlPath "tests/integration/select_primary_key.sql" -ExpectedPath "tests/integration/select_primary_key.expected"
    Invoke-IntegrationCase -SqlPath "tests/integration/insert_then_select_primary_key.sql" -ExpectedPath "tests/integration/insert_then_select_primary_key.expected"
}

function Run-VerifySuite {
    New-Item -ItemType Directory -Force -Path "data/generated" | Out-Null

    Invoke-PythonChecked -Arguments @(
        "tools/generate_students_csv.py",
        "--rows", "1000",
        "--output", "data/generated/students_1k.csv",
        "--seed", "42",
        "--start-id", "1"
    )
    Invoke-NativeChecked -FilePath ".\tests\verify\verify_students_dataset.exe" -Arguments @(
        "data/generated/students_1k.csv",
        "1000",
        "1"
    )

    foreach ($fixture in @(
        "students_fail_id.csv",
        "students_fail_age.csv",
        "students_fail_score.csv",
        "students_fail_columns.csv"
    )) {
        Invoke-NativeExpectFailure -FilePath ".\tests\verify\verify_students_dataset.exe" -Arguments @(
            "tests/verify/fixtures/$fixture",
            "3",
            "1"
        )
        Write-Host "[PASS] verify failure fixture $fixture"
    }

    if ($IncludeLargeVerify) {
        Invoke-PythonChecked -Arguments @(
            "tools/generate_students_csv.py",
            "--rows", "1000000",
            "--output", "data/generated/students_1m.csv",
            "--seed", "42",
            "--start-id", "1"
        )
        Invoke-NativeChecked -FilePath ".\tests\verify\verify_students_dataset.exe" -Arguments @(
            "data/generated/students_1m.csv",
            "1000000",
            "1"
        )
    }
}

if ($Build) {
    & ".\tools\build_windows.ps1" -Target tests
    if ($LASTEXITCODE -ne 0) {
        throw "build_windows.ps1 failed"
    }
}

switch ($Suite) {
    "unit" {
        Run-UnitSuite
    }
    "integration" {
        Run-IntegrationSuite
    }
    "verify" {
        Run-VerifySuite
    }
    "bptree-contract" {
        Run-BptreeContractSuite
    }
    default {
        Run-UnitSuite
        Run-BptreeContractSuite
        Run-IntegrationSuite
        Run-VerifySuite
    }
}

Write-Host "[OK] test suite '$Suite' completed"
