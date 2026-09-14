#!/bin/sh

classify_result() {
    phase=$1
    status=$2
    log=${3:-/dev/null}

    case "$phase" in
        docker)
            if test "$status" -eq 1 &&
              grep -Eiq 'failed to connect to the docker API|Cannot connect to the Docker daemon' "$log"; then
                echo "FAIL-TOOL: reason=docker-daemon-or-runtime status=1"
                return 3
            fi
            case "$status" in
                0) echo "PASS: reason=container-completed"; return 0 ;;
                1) echo "FAIL-PRODUCT: reason=contained-product-failure status=1"; return 1 ;;
                2) echo "FAIL-MISSING-TOOL: reason=contained-missing-tool status=2"; return 2 ;;
                3) echo "FAIL-TOOL: reason=contained-tool-failure status=3"; return 3 ;;
                4) echo "FAIL-TIMEOUT: reason=contained-timeout status=4"; return 4 ;;
                124|137|143) echo "FAIL-TIMEOUT: reason=container-timeout status=$status"; return 4 ;;
                125) echo "FAIL-TOOL: reason=docker-daemon-or-runtime status=125"; return 3 ;;
                126) echo "FAIL-TOOL: reason=container-command-not-executable status=126"; return 3 ;;
                127) echo "FAIL-TOOL: reason=container-command-not-found status=127"; return 3 ;;
                *) echo "FAIL-PRODUCT: reason=contained-runner-status status=$status"; return 1 ;;
            esac
            ;;
        direct)
            if test "$status" -eq 0; then
                echo "PASS: reason=direct-runner-passed"
                return 0
            fi
            echo "FAIL-PRODUCT: reason=direct-runner-failure status=$status"
            return 1
            ;;
        memcheck)
            if test "$status" -eq 0; then
                echo "PASS: reason=memcheck-clean"
                return 0
            fi
            if test "$status" -eq 100; then
                echo "FAIL-PRODUCT: reason=memcheck-findings status=100"
                return 1
            fi
            if test "$status" -eq 124 || test "$status" -eq 137 || test "$status" -eq 143; then
                echo "FAIL-TIMEOUT: reason=memcheck-timeout status=$status"
                return 4
            fi
            if grep -Eiq 'unrecognised instruction|unhandled bytes|SIGILL|Illegal instruction' "$log"; then
                echo "FAIL-TOOL: reason=unsupported-client-instruction status=$status"
                return 3
            fi
            echo "FAIL-TOOL: reason=valgrind-crash status=$status"
            return 3
            ;;
        missing)
            echo "FAIL-MISSING-TOOL: reason=valgrind-not-found"
            return 2
            ;;
        *)
            echo "FAIL-TOOL: reason=classifier-invalid-phase phase=$phase"
            return 3
            ;;
    esac
}

if test "${0##*/}" = classify.sh && test "$#" -gt 0; then
    classify_result "$@"
fi