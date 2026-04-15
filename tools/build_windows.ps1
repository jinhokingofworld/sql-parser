param(
    [ValidateSet("all", "sql_processor", "unit", "verify", "bptree-contract", "tests")]
    [string] $Target = "all"
)

# ms: Fail fast so CI-style local runs stop on the first broken build step.
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-GccCommand {
    # ms: Resolve gcc from PATH once so every build path uses the same toolchain.
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

    # ms: Centralize native invocation so all build failures surface uniformly.
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

    # ms: Ensure target directories exist before compiling into nested test paths.
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
    # ms: Keep the application and tests on the same shared library source set.
    $libSources = @(
        "src/cli.c",
        "src/tokenizer.c",
        "src/parser.c",
        "src/schema.c",
        "src/storage.c",
        "src/bptree.c",
        "src/db_context.c",
        "src/executor.c",
        "src/utils.c"
    )

    Build-NativeTarget `
        -OutputPath "sql_processor.exe" `
        -Sources (@("src/main.c") + $libSources) `
        -IncludeDirs @("include")
}

function Build-UnitTests {
    # ms: Unit binaries mix plain main-based tests and Unity-based tests, so build them explicitly.
    $libSources = @(
        "src/cli.c",
        "src/tokenizer.c",
        "src/parser.c",
        "src/schema.c",
        "src/storage.c",
        "src/bptree.c",
        "src/db_context.c",
        "src/executor.c",
        "src/utils.c"
    )

    Build-NativeTarget `
        -OutputPath "tests/unit/test_tokenizer.exe" `
        -Sources (@("tests/unit/test_tokenizer.c") + $libSources) `
        -IncludeDirs @("include")

    Build-NativeTarget `
        -OutputPath "tests/unit/test_parser.exe" `
        -Sources (@("tests/unit/test_parser.c") + $libSources) `
        -IncludeDirs @("include")

    Build-NativeTarget `
        -OutputPath "tests/unit/test_storage.exe" `
        -Sources (@(
            "tests/unit/test_storage.c",
            "tests/support/unity.c",
            "tests/support/test_helpers.c"
        ) + $libSources) `
        -IncludeDirs @("include", "tests/support")

    Build-NativeTarget `
        -OutputPath "tests/unit/test_executor.exe" `
        -Sources (@("tests/unit/test_executor.c") + $libSources) `
        -IncludeDirs @("include")
}

function Build-VerifyTarget {
    # ms: Dataset verification is a standalone executable because it exercises file fixtures directly.
    $libSources = @(
        "src/cli.c",
        "src/tokenizer.c",
        "src/parser.c",
        "src/schema.c",
        "src/storage.c",
        "src/bptree.c",
        "src/db_context.c",
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
    # ms: The contract test compiles independently so the adapter can be introduced later.
    Build-NativeTarget `
        -OutputPath "tests/unit/test_bptree_contract.exe" `
        -Sources @(
            "tests/unit/test_bptree_contract.c",
            "tests/support/unity.c",
            "src/bptree.c",
            "src/utils.c"
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

# ms: Emit a single success marker so wrapper scripts can treat this as one build step.
Write-Host "[OK] build target '$Target' completed"
