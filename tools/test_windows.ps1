param(
    [ValidateSet("all", "unit", "integration", "verify", "bptree-contract")]
    [string] $Suite = "all",
    [switch] $Build,
    [switch] $IncludeLargeVerify
)

# ms: Keep PowerShell errors strict while still handling native exit codes manually.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $false

function Invoke-NativeChecked {
    param(
        [string] $FilePath,
        [string[]] $Arguments = @()
    )

    # ms: Route every native test execution through one helper for consistent failure messages.
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

    # ms: Negative fixtures are valid only when the target exits non-zero.
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

    # ms: Integration assertions should ignore Windows vs Unix line-ending differences.
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

    # ms: Use normalized text comparison so fixture mismatches are semantic, not newline-related.
    if ((Normalize-Text -Text $Expected) -ne (Normalize-Text -Text $Actual)) {
        throw "output mismatch: $Label"
    }
}

function New-IntegrationDb {
    # ms: Each integration case gets an isolated copy of the fixture database to avoid cross-test state.
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

    # ms: Run the SQL file end-to-end and compare rendered output against the checked-in fixture.
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

    # ms: Dataset generation is external to the C binaries, so guard it with explicit exit-code checks.
    & python @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "python failed: python $($Arguments -join ' ')"
    }
}

function Run-UnitSuite {
    # ms: Keep the unit suite order stable so failures are easy to map back to the legacy Makefile flow.
    Invoke-NativeChecked -FilePath ".\tests\unit\test_tokenizer.exe"
    Invoke-NativeChecked -FilePath ".\tests\unit\test_parser.exe"
    Invoke-NativeChecked -FilePath ".\tests\unit\test_storage.exe"
    Invoke-NativeChecked -FilePath ".\tests\unit\test_executor.exe"
}

function Run-BptreeContractSuite {
    # ms: The adapter-backed contract test is isolated so it can be enabled independently.
    Invoke-NativeChecked -FilePath ".\tests\unit\test_bptree_contract.exe"
}

function Run-IntegrationSuite {
    # ms: These fixtures lock down the CLI-visible behavior for the current SQL feature set.
    Invoke-IntegrationCase -SqlPath "tests/integration/insert_select.sql" -ExpectedPath "tests/integration/insert_select.expected"
    Invoke-IntegrationCase -SqlPath "tests/integration/select_where.sql" -ExpectedPath "tests/integration/select_where.expected"
    Invoke-IntegrationCase -SqlPath "tests/integration/select_order_by.sql" -ExpectedPath "tests/integration/select_order_by.expected"
    Invoke-IntegrationCase -SqlPath "tests/integration/select_primary_key.sql" -ExpectedPath "tests/integration/select_primary_key.expected"
    Invoke-IntegrationCase -SqlPath "tests/integration/insert_then_select_primary_key.sql" -ExpectedPath "tests/integration/insert_then_select_primary_key.expected"
}

function Run-VerifySuite {
    # ms: Verification covers generated dataset contracts plus negative fixtures for malformed input.
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
    # ms: Optional rebuild keeps local test runs aligned with the currently checked-out sources.
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

# ms: One final marker makes it obvious the selected suite completed without an early throw.
Write-Host "[OK] test suite '$Suite' completed"
