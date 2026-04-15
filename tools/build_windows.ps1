param(
    [ValidateSet("all", "sql_processor", "unit", "verify", "bptree-contract", "tests")]
    [string] $Target = "all"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-GccCommand {
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    if ($null -eq $gcc) {
        throw "gcc was not found in PATH"
    }

    return $gcc.Source
}

function Invoke-Gcc {
    param(
        [string[]] $Arguments
    )

    $gcc = Get-GccCommand
    & $gcc @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "gcc failed with exit code $LASTEXITCODE"
    }
}

function Build-NativeTarget {
    param(
        [string] $OutputPath,
        [string[]] $Sources,
        [string[]] $IncludeDirs
    )

    $parent = Split-Path -Parent $OutputPath
    if ($parent) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $args = @(
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic"
    )

    foreach ($includeDir in $IncludeDirs) {
        $args += "-I$includeDir"
    }

    $args += "-o"
    $args += $OutputPath
    $args += $Sources

    Invoke-Gcc -Arguments $args
}

function Build-SqlProcessor {
    $libSources = @(
        "src/cli.c",
        "src/tokenizer.c",
        "src/parser.c",
        "src/schema.c",
        "src/storage.c",
        "src/executor.c",
        "src/utils.c"
    )

    Build-NativeTarget `
        -OutputPath "sql_processor.exe" `
        -Sources (@("src/main.c") + $libSources) `
        -IncludeDirs @("include")
}

function Build-UnitTests {
    $libSources = @(
        "src/cli.c",
        "src/tokenizer.c",
        "src/parser.c",
        "src/schema.c",
        "src/storage.c",
        "src/executor.c",
        "src/utils.c"
    )
    $supportSources = @(
        "tests/support/unity.c",
        "tests/support/test_helpers.c"
    )

    Build-NativeTarget `
        -OutputPath "tests/unit/test_tokenizer.exe" `
        -Sources (@("tests/unit/test_tokenizer.c") + $libSources + $supportSources) `
        -IncludeDirs @("include", "tests/support")

    Build-NativeTarget `
        -OutputPath "tests/unit/test_parser.exe" `
        -Sources (@("tests/unit/test_parser.c") + $libSources + $supportSources) `
        -IncludeDirs @("include", "tests/support")

    Build-NativeTarget `
        -OutputPath "tests/unit/test_storage.exe" `
        -Sources (@("tests/unit/test_storage.c") + $libSources + $supportSources) `
        -IncludeDirs @("include", "tests/support")

    Build-NativeTarget `
        -OutputPath "tests/unit/test_executor.exe" `
        -Sources (@("tests/unit/test_executor.c") + $libSources + $supportSources) `
        -IncludeDirs @("include", "tests/support")
}

function Build-VerifyTarget {
    $libSources = @(
        "src/cli.c",
        "src/tokenizer.c",
        "src/parser.c",
        "src/schema.c",
        "src/storage.c",
        "src/executor.c",
        "src/utils.c"
    )

    Build-NativeTarget `
        -OutputPath "tests/verify/verify_students_dataset.exe" `
        -Sources (@(
            "tests/verify/verify_students_dataset.c",
            "tests/support/test_helpers.c"
        ) + $libSources) `
        -IncludeDirs @("include", "tests/support")
}

function Build-BptreeContract {
    Build-NativeTarget `
        -OutputPath "tests/unit/test_bptree_contract.exe" `
        -Sources @(
            "tests/unit/test_bptree_contract.c",
            "tests/support/unity.c"
        ) `
        -IncludeDirs @("include", "tests/support")
}

switch ($Target) {
    "sql_processor" {
        Build-SqlProcessor
    }
    "unit" {
        Build-UnitTests
    }
    "verify" {
        Build-VerifyTarget
    }
    "bptree-contract" {
        Build-BptreeContract
    }
    "tests" {
        Build-SqlProcessor
        Build-UnitTests
        Build-VerifyTarget
        Build-BptreeContract
    }
    default {
        Build-SqlProcessor
        Build-UnitTests
        Build-VerifyTarget
        Build-BptreeContract
    }
}

Write-Host "[OK] build target '$Target' completed"
