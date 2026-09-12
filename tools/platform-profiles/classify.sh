#!/bin/sh

classify_platform_result() {
    phase=$1
    status=$2
    log=${3:-/dev/null}

    case "$phase" in
        image-build)
            if test "$status" -eq 0; then
                echo "PASS|image-built|0"
                return 0
            fi
            if test "$status" -eq 124 || test "$status" -eq 137 || test "$status" -eq 143; then
                echo "FAIL-TIMEOUT|image-build-timeout|$status"
                return 4
            fi
            if grep -Eiq 'CMake Error|internal compiler error|(^|[[:space:]])error:|FAILED:|undefined reference' "$log"; then
                echo "FAIL-PRODUCT|dependency-build-failure|$status"
                return 1
            fi
            echo "FAIL-TOOL|image-build-failure|$status"
            return 3
            ;;
        docker)
            case "$status" in
                0) echo "PASS|container-completed|0"; return 0 ;;
                1) echo "FAIL-PRODUCT|contained-product-failure|1"; return 1 ;;
                2) echo "FAIL-MISSING-TOOL|contained-missing-tool|2"; return 2 ;;
                3) echo "FAIL-TOOL|contained-tool-failure|3"; return 3 ;;
                4) echo "FAIL-TIMEOUT|contained-timeout|4"; return 4 ;;
                124|137|143) echo "FAIL-TIMEOUT|container-timeout|$status"; return 4 ;;
                125) echo "FAIL-TOOL|docker-daemon-or-runtime|125"; return 3 ;;
                126) echo "FAIL-TOOL|container-command-not-executable|126"; return 3 ;;
                127) echo "FAIL-TOOL|container-command-not-found|127"; return 3 ;;
                *) echo "FAIL-PRODUCT|contained-runner-status|$status"; return 1 ;;
            esac
            ;;
        dependency-check|environment)
            if test "$status" -eq 0; then
                echo "PASS|phase-completed|0"
                return 0
            fi
            echo "FAIL-TOOL|profile-image-invalid|$status"
            return 3
            ;;
        strict-app-build|strict-test-build)
            if test "$status" -eq 0; then
                echo "PASS|phase-completed|0"
                return 0
            fi
            if test "$status" -eq 124 || test "$status" -eq 137 || test "$status" -eq 143; then
                echo "FAIL-TIMEOUT|build-timeout|$status"
                return 4
            fi
            if grep -Eiq 'internal compiler error|PLEASE submit a bug report|frontend command failed due to signal' "$log"; then
                echo "FAIL-TOOL|compiler-crash|$status"
                return 3
            fi
            if test "$phase" = strict-app-build; then
                echo "FAIL-PRODUCT|application-compile-failure|$status"
            else
                echo "FAIL-PRODUCT|test-compile-failure|$status"
            fi
            return 1
            ;;
        complete-test-run|standards-core)
            if test "$status" -eq 0; then
                echo "PASS|phase-completed|0"
                return 0
            fi
            if test "$status" -eq 124 || test "$status" -eq 137 || test "$status" -eq 143; then
                echo "FAIL-TIMEOUT|execution-timeout|$status"
                return 4
            fi
            if test "$phase" = complete-test-run; then
                echo "FAIL-PRODUCT|test-runner-failure|$status"
            else
                echo "FAIL-PRODUCT|standards-core-failure|$status"
            fi
            return 1
            ;;
        missing-compiler)
            echo "FAIL-MISSING-TOOL|compiler-not-found|$status"
            return 2
            ;;
        missing-make)
            echo "FAIL-MISSING-TOOL|make-not-found|$status"
            return 2
            ;;
        *)
            echo "FAIL-TOOL|classifier-invalid-phase|$status"
            return 3
            ;;
    esac
}

if test "${0##*/}" = classify.sh && test "$#" -gt 0; then
    classify_platform_result "$@"
fi