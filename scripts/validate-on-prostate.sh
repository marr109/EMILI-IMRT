#!/usr/bin/env bash
# =============================================================================
# validate-on-prostate.sh
#
# Validates an irace best configuration on clinical-scale PROSTATE instances
# and compares it against a pre-tuning baseline.
#
# The script:
#   1. Extracts the winning configuration from an irace results Rdata file
#   2. Runs it on PROSTATE_sampled and PROSTATE_sampled2 with seeds 42-51
#   3. Runs the baseline config on the same instances and seeds
#   4. Prints a comparison table with mean objective, CI, HI, and V95%
#   5. Optionally runs a Wilcoxon signed-rank test per instance
#
# Usage:
#   scripts/validate-on-prostate.sh \
#     --results-dir <dir>                  irace results dir (contains irace.log)
#     --algorithm <ils|vns|tabu>           metaheuristic to evaluate
#     --baseline <file>                    pre-irace baseline config file
#     [--wilcoxon]                         run Wilcoxon signed-rank test
#
# Exit codes:
#   0 — success
#   1 — argument or usage error
#   2 — missing required file (irace.log, baseline config, EMILI, instances)
#
# Environment:
#   EMILI — override path to the EMILI binary (default: ../build/emili)
#   Run from the project root directory.
# =============================================================================
set -euo pipefail

# ── Constants ────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
EMILI="${EMILI:-${PROJECT_ROOT}/build/emili}"
K=4
SEEDS=(42 43 44 45 46 47 48 49 50 51)
PROSTATE_INSTANCES=("instances/PROSTATE_sampled" "instances/PROSTATE_sampled2")

# ── Usage ────────────────────────────────────────────────────────────────────
usage() {
    cat <<'EOF'
Usage: scripts/validate-on-prostate.sh [OPTIONS]

Required:
  --results-dir <dir>            irace results directory (must contain irace.log)
  --algorithm <ils|vns|tabu>     metaheuristic to evaluate
  --baseline <file>              pre-irace baseline config file

Optional:
  --wilcoxon                     run Wilcoxon signed-rank test (best vs baseline)
  -h, --help                     show this help message

Environment:
  EMILI                          override path to EMILI binary
EOF
}

# ── Argument parsing ────────────────────────────────────────────────────────
RESULTS_DIR=""
ALGORITHM=""
BASELINE_CONFIG=""
DO_WILCOXON=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --results-dir) RESULTS_DIR="$2"; shift 2 ;;
        --algorithm)   ALGORITHM="$2";   shift 2 ;;
        --baseline)    BASELINE_CONFIG="$2"; shift 2 ;;
        --wilcoxon)    DO_WILCOXON=true; shift ;;
        -h|--help)     usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

# Validate required arguments
if [[ -z "$RESULTS_DIR" || -z "$ALGORITHM" || -z "$BASELINE_CONFIG" ]]; then
    echo "Error: --results-dir, --algorithm, and --baseline are required." >&2
    echo "" >&2
    usage
    exit 1
fi

case "$ALGORITHM" in
    ils|vns|tabu) ;;
    *) echo "Error: --algorithm must be one of: ils, vns, tabu" >&2; exit 1 ;;
esac

# ── Validate inputs ──────────────────────────────────────────────────────────
IRACE_LOG="${RESULTS_DIR}/irace.log"
if [[ ! -f "$IRACE_LOG" ]]; then
    echo "Error: irace log file not found: $IRACE_LOG" >&2
    exit 2
fi

if [[ ! -f "$BASELINE_CONFIG" ]]; then
    echo "Error: baseline config file not found: $BASELINE_CONFIG" >&2
    exit 2
fi

if [[ ! -x "$EMILI" ]]; then
    echo "Error: EMILI binary not found or not executable: $EMILI" >&2
    exit 2
fi

for inst in "${PROSTATE_INSTANCES[@]}"; do
    if [[ ! -d "$inst" ]]; then
        echo "Error: PROSTATE instance directory not found: $inst" >&2
        exit 2
    fi
done

# ── Helper functions ────────────────────────────────────────────────────────

# Parse an irace config file (header line + values line) into KEY=VALUE pairs.
# Handles quoted strings and NA values.
parse_config_file() {
    local file="$1"
    local header value
    header=$(head -1 "$file")
    value=$(sed -n '2p' "$file" | tr -d '"')
    local -a headers values
    read -ra headers <<< "$header"
    read -ra values <<< "$value"
    local i
    for i in "${!headers[@]}"; do
        echo "${headers[$i]}=${values[$i]:-NA}"
    done
}

# Extract the best (lowest-objective) configuration from an irace Rdata log.
# Outputs KEY=VALUE pairs on stdout.
extract_best_config() {
    local irace_log="$1"
    R --slave --vanilla 2>"${RESULTS_DIR}/.extract_stderr" <<EOF
load("${irace_log}")
n <- length(iraceResults\$allElites)
if (n > 0) {
    elites <- iraceResults\$allElites[[n]]
    best_id <- elites[1]
} else if (!is.null(iraceResults\$iterationElites) && length(iraceResults\$iterationElites) > 0) {
    best_id <- iraceResults\$iterationElites[1]
} else {
    best_id <- rownames(iraceResults\$allConfigurations)[1]
    if (is.null(best_id)) best_id <- 1
}
config_df <- if (!is.null(iraceResults\$configurations)) {
    iraceResults\$configurations
} else {
    iraceResults\$allConfigurations
}
config <- config_df[best_id, ]
for (name in names(config)) {
    if (name %in% c(".PARENT.", "Row.names", ".ID.")) next
    val <- as.character(config[[name]])
    if (is.na(config[[name]])) val <- "NA"
    cat(name, "=", val, "\n", sep="")
}
EOF
}

# Set config variables from KEY=VALUE pairs passed on stdin.
# Initialises all possible parameters to defaults before overriding.
set_config_vars() {
    inner_explore="best"
    inner_init="ifirstk"
    inner_iter="1"
    pert_type="prangswap"
    pert_swaps="1"
    D="2"
    outer_iter="2"
    outer_accept="improve"
    tabu_type="fixed"
    max_iter="10"
    tenure="5"
    tenure_min="2"
    tenure_max="8"
    p_max="2"
    w_under="1.0"
    w_over="0.5"
    w_ptv_over="0.5"

    local key value
    while IFS='=' read -r key value; do
        [[ -z "$key" ]] && continue
        case "$key" in
            inner_explore) inner_explore="$value" ;;
            inner_init)    inner_init="$value" ;;
            inner_iter)    inner_iter="$value" ;;
            pert_type)     pert_type="$value" ;;
            pert_swaps)    pert_swaps="$value" ;;
            D)             D="$value" ;;
            outer_iter)    outer_iter="$value" ;;
            tabu_type)     tabu_type="$value" ;;
            max_iter)      max_iter="$value" ;;
            tenure)        tenure="$value" ;;
            tenure_min)    tenure_min="$value" ;;
            tenure_max)    tenure_max="$value" ;;
            p_max)         p_max="$value" ;;
            w_under)       w_under="$value" ;;
            w_over)        w_over="$value" ;;
            w_ptv_over)    w_ptv_over="$value" ;;
        esac
    done
}

# Build the algorithm-specific EMILI argument string.
# Relies on config variables being set in the caller's scope.
build_emili_args() {
    local algo="$1"
    case "$algo" in
        ils)
            if [[ "$pert_type" == "pgreedy" ]]; then
                echo "ils $inner_explore $inner_init tmaxiter $inner_iter nangswap tmaxiter $outer_iter pgreedy $D improve"
            else
                echo "ils $inner_explore $inner_init tmaxiter $inner_iter nangswap tmaxiter $outer_iter prangswap $pert_swaps improve"
            fi
            ;;
        vns)
            echo "vns $inner_explore $inner_init tmaxiter $inner_iter nangswap tmaxiter $outer_iter bangshake $p_max accng improve"
            ;;
        tabu)
            if [[ "$tabu_type" == "adaptive" ]]; then
                # Ensure tenure_min <= tenure_max
                if [[ "$tenure_min" -gt "$tenure_max" ]]; then
                    local tmp="$tenure_min"; tenure_min="$tenure_max"; tenure_max="$tmp"
                fi
                echo "tabu $inner_explore $inner_init tmaxiter $max_iter nangswap TBao_adaptive $tenure_min $tenure_max"
            else
                echo "tabu $inner_explore $inner_init tmaxiter $max_iter nangswap TBao_fixed $tenure"
            fi
            ;;
    esac
}

# Run a single EMILI evaluation on a PROSTATE instance.
# Outputs a single line: "objective CI HI V95" or "FAIL".
run_evaluation() {
    local instance="$1" seed="$2"
    local temp_dir
    temp_dir=$(mktemp -d)

    # Copy instance files (except instance_config.txt) to temp dir
    local f fname
    for f in "$instance"/*; do
        fname=$(basename "$f")
        if [[ "$fname" != "instance_config.txt" ]]; then
            cp "$f" "$temp_dir/$fname"
        fi
    done

    # Copy and modify instance_config.txt with tuned weights
    cp "$instance/instance_config.txt" "$temp_dir/instance_config.txt"
    sed -i "s/^w_under[[:space:]].*/w_under       $w_under/"     "$temp_dir/instance_config.txt"
    sed -i "s/^w_over[[:space:]].*/w_over        $w_over/"       "$temp_dir/instance_config.txt"
    sed -i "s/^w_ptv_over[[:space:]].*/w_ptv_over    $w_ptv_over/"   "$temp_dir/instance_config.txt"

    # Build EMILI args and run
    local args
    args=$(build_emili_args "$ALGORITHM")
    local output
    output=$("$EMILI" "$temp_dir" baoimrt "$K" $args rnds "$seed" 2>&1) || true

    # Clean up temp dir
    rm -rf "$temp_dir"

    # Parse EMILI output
    local obj ci hi v95
    obj=$(echo "$output" | grep "Objective function value:" | awk '{print $NF}' | tail -1)
    ci=$(echo "$output" | grep "Conformity Index" | awk '{print $NF}')
    hi=$(echo "$output" | grep "Homogeneity Index" | awk '{print $NF}')
    v95=$(echo "$output" | grep "PTV coverage" | awk '{print $(NF-1)}')

    if [[ -z "$obj" ]]; then
        echo "FAIL"
    else
        echo "$obj ${ci:-N/A} ${hi:-N/A} ${v95:-N/A}"
    fi
}

# ── Main logic ───────────────────────────────────────────────────────────────

echo "=== Validation on PROSTATE ==="
echo "Algorithm: $ALGORITHM"
echo "Results:   $RESULTS_DIR"
echo "Baseline:  $BASELINE_CONFIG"
echo ""

# 1. Extract best config from irace.log
echo "Extracting best config from irace.log ..."
BEST_CONFIG_STR=$(extract_best_config "$IRACE_LOG")
if [[ -z "$BEST_CONFIG_STR" ]]; then
    echo "Error: failed to extract configuration from $IRACE_LOG" >&2
    echo "  R stderr:" >&2
    cat "${RESULTS_DIR}/.extract_stderr" >&2 || true
    rm -f "${RESULTS_DIR}/.extract_stderr"
    exit 2
fi
rm -f "${RESULTS_DIR}/.extract_stderr"

# 2. Read baseline config
BASELINE_CONFIG_STR=$(parse_config_file "$BASELINE_CONFIG")

# 3. Run evaluations
#    Store results in temp files: one per (config, instance) pair.
#    Format: "seed objective CI HI V95" or "seed FAIL"
RESULTS_TMP=$(mktemp -d)
trap 'rm -rf "$RESULTS_TMP"' EXIT

for config_label in best baseline; do
    if [[ "$config_label" == "best" ]]; then
        set_config_vars <<< "$BEST_CONFIG_STR"
    else
        set_config_vars <<< "$BASELINE_CONFIG_STR"
    fi

    echo "Running $config_label config ..."
    for instance in "${PROSTATE_INSTANCES[@]}"; do
        instance_name=$(basename "$instance")
        result_file="${RESULTS_TMP}/${config_label}_${instance_name}.txt"
        : > "$result_file"

        for seed in "${SEEDS[@]}"; do
            printf "  [%-8s/%-18s seed=%s] " "$config_label" "$instance_name" "$seed" >&2
            result=$(run_evaluation "$instance" "$seed")
            echo "$seed $result" >> "$result_file"
            if [[ "$result" == "FAIL" ]]; then
                echo "FAIL" >&2
            else
                obj=$(echo "$result" | awk '{print $1}')
                printf "f=%s\n" "$obj" >&2
            fi
        done
    done
done

# 4. Compute statistics and print comparison table
echo ""
echo "=============================================="
echo " VALIDATION RESULTS — PROSTATE"
echo "=============================================="
printf "%-10s %-20s %12s %8s %8s %8s\n" "config" "instance" "f*" "CI" "HI" "V95%"
echo "------------------------------------------------------------"

for config_label in best baseline; do
    for instance in "${PROSTATE_INSTANCES[@]}"; do
        instance_name=$(basename "$instance")
        result_file="${RESULTS_TMP}/${config_label}_${instance_name}.txt"

        # Compute means using awk (skips FAIL rows)
        read f_star ci_mean hi_mean v95_mean < <(awk '
            NF >= 2 && $2 != "FAIL" {
                obj += $2; n++
                if ($3 != "N/A") ci += $3; cn++
                if ($4 != "N/A") hi += $4; hn++
                if ($5 != "N/A") v95 += $5; vn++
            }
            END {
                if (n == 0) {
                    print "N/A N/A N/A N/A"
                } else {
                    printf "%.2f ", obj / n
                    printf "%.3f ", (cn > 0) ? ci / cn : 0
                    printf "%.3f ", (hn > 0) ? hi / hn : 0
                    printf "%.2f",  (vn > 0) ? v95 / vn : 0
                }
            }
        ' "$result_file")

        printf "%-10s %-20s %12s %8s %8s %8s\n" \
            "$config_label" "$instance_name" "$f_star" "$ci_mean" "$hi_mean" "$v95_mean"
    done
done
echo "=============================================="

# 5. Optional Wilcoxon signed-rank test
if [[ "$DO_WILCOXON" == true ]]; then
    echo ""
    echo "Wilcoxon signed-rank test (best vs baseline, alpha = 0.05):"

    for instance in "${PROSTATE_INSTANCES[@]}"; do
        instance_name=$(basename "$instance")
        best_file="${RESULTS_TMP}/best_${instance_name}.txt"
        baseline_file="${RESULTS_TMP}/baseline_${instance_name}.txt"

        # Build paired vectors: only seeds where BOTH configs succeeded
        paired_file="${RESULTS_TMP}/paired_${instance_name}.txt"
        awk '
            FNR == NR {
                if ($2 != "FAIL") best[$1] = $2
                next
            }
            {
                if ($2 != "FAIL" && ($1 in best)) {
                    print best[$1], $2
                }
            }
        ' "$best_file" "$baseline_file" > "$paired_file"

        n_paired=$(wc -l < "$paired_file")

        if [[ "$n_paired" -lt 5 ]]; then
            echo "  ${instance_name}: only ${n_paired} paired seeds (< 5) — skipping"
            continue
        fi

        # Extract comma-separated vectors for R
        best_vec=$(awk '{print $1}' "$paired_file" | paste -sd, -)
        baseline_vec=$(awk '{print $2}' "$paired_file" | paste -sd, -)

        # Run Wilcoxon signed-rank test via R
        p_value=$(R --slave --vanilla 2>/dev/null <<EOF
x <- c(${best_vec})
y <- c(${baseline_vec})
if (all(x == y)) {
    cat("1\n")
} else {
    w <- wilcox.test(x, y, paired = TRUE, exact = FALSE)
    cat(format(w\$p.value, digits = 4), "\n", sep = "")
}
EOF
)

        # Determine verdict at alpha = 0.05
        if awk "BEGIN {exit !($p_value < 0.05)}"; then
            verdict="reject H0 (best != baseline)"
        else
            verdict="fail to reject H0"
        fi
        echo "  ${instance_name}: p = ${p_value}  ->  ${verdict}"
    done
fi
