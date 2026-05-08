#!/usr/bin/env bash
# Build every codegen profile under gcc+clang release and report .so sizes.
# Always wipes existing build dirs to ensure a clean configure.
# Profiles are discovered from pixpat-native/profiles/*.toml.

set -euo pipefail

# Meson setup flags applied to every (compiler × profile) build dir.
# Add new -D flags or buildtype overrides here.
MESON_SETUP_ARGS=(
	--buildtype=release
)

# name → toml path.
PROFILES=()
profile_dir="pixpat-native/profiles"
for toml in "$profile_dir"/*.toml; do
	[[ -e "$toml" ]] || continue
	name="$(basename "$toml" .toml)"
	PROFILES+=("$name:$toml")
done

COMPILERS=(
	"gcc:gcc:g++"
	"clang:clang:clang++"
)

build_one() {
	local cc_name=$1 cc=$2 cxx=$3 name=$4 toml=$5 log=$6
	local dir="build-$cc_name-$name"

	local setup_args=("${MESON_SETUP_ARGS[@]}")
	[[ -d "$dir/meson-info" ]] && setup_args+=(--wipe)
	setup_args+=("-Dconfig=$toml")

	# Capture meson output so we can show it on failure; on success discard
	# it and let ninja write its (warnings-only with --quiet) output instead.
	if ! CC=$cc CXX=$cxx meson setup "$dir" "${setup_args[@]}" >"$log" 2>&1; then
		return 1
	fi
	: >"$log"
	ninja -C "$dir" --quiet >"$log" 2>&1
}

pids=()
labels=()
logs=()
for comp in "${COMPILERS[@]}"; do
	IFS=: read -r cc_name cc cxx <<<"$comp"
	for entry in "${PROFILES[@]}"; do
		name="${entry%%:*}"
		toml="${entry#*:}"
		dir="build-$cc_name-$name"
		log=$(mktemp)
		echo "==> launching $dir"
		build_one "$cc_name" "$cc" "$cxx" "$name" "$toml" "$log" &
		pids+=($!)
		labels+=("$dir")
		logs+=("$log")
	done
done

failed=0
for i in "${!pids[@]}"; do
	if wait "${pids[$i]}"; then
		if [[ -s "${logs[$i]}" ]]; then
			echo "==> ${labels[$i]} ok (warnings):"
			cat "${logs[$i]}"
		else
			echo "==> ${labels[$i]} ok"
		fi
	else
		echo "==> ${labels[$i]} FAILED:"
		cat "${logs[$i]}"
		failed=1
	fi
	rm -f "${logs[$i]}"
done
(( failed == 0 )) || exit 1

CASES=(
	"RGB888 -> BGR888"
	"NV12 -> BGR888"
	"BGR888 -> NV12"
	"NV12 -> YUV420"
	"smpte -> BGR888"
	"smpte -> NV12"
	"kmstest -> BGR888"
)
LABELS=("RGB->BGR" "NV12->BGR" "BGR->NV12" "NV12->YUV" "smpte/BGR" "smpte/NV12" "kmstest/BGR")

PERF_TEST="pixpat-python/scripts/perf_test.py"

join_cases() {
	local out=""
	local c
	for c in "$@"; do
		out+="${out:+,}$c"
	done
	printf '%s' "$out"
}
cases_arg=$(join_cases "${CASES[@]}")

echo
echo "=== libpixpat.so.0.0.0 sizes + pixpat MP/s (release, --iters 5 --warmup 2) ==="
printf "%-28s  %10s" "build-dir" "bytes"
for lbl in "${LABELS[@]}"; do
	printf "  %10s" "$lbl"
done
echo

for comp in "${COMPILERS[@]}"; do
	cc_name="${comp%%:*}"
	for entry in "${PROFILES[@]}"; do
		name="${entry%%:*}"
		dir="build-$cc_name-$name"
		so="$dir/libpixpat.so.0.0.0"
		if [[ ! -f "$so" ]]; then
			printf "%-28s  %10s\n" "$dir" "MISSING"
			continue
		fi

		bytes=$(stat -c%s "$so")
		tsv=$(PIXPAT_LIB="$so" python3 "$PERF_TEST" \
			--tsv --iters 5 --warmup 2 \
			--cases "$cases_arg" 2>/dev/null || true)

		printf "%-28s  %10d" "$dir" "$bytes"
		for c in "${CASES[@]}"; do
			val=$(awk -F'\t' -v want="$c" '$1 == want { print $2; exit }' <<<"$tsv")
			printf "  %10s" "${val:--}"
		done
		echo
	done
done
