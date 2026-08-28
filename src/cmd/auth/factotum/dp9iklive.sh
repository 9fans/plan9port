#!/bin/bash
set -eu

: "${PLAN9:?set PLAN9}"
: "${DP9IK_TEST_DOMAIN:?set DP9IK_TEST_DOMAIN}"
: "${DP9IK_TEST_USER:?set DP9IK_TEST_USER}"
: "${DP9IK_TEST_PASSWORD:?set DP9IK_TEST_PASSWORD}"

ns="${DP9IK_TEST_NAMESPACE:-/tmp/ns.dp9iklive.$$}"
mnt="${DP9IK_TEST_MNT:-$ns/mnt}"
service="${DP9IK_TEST_SERVICE:-factotum}"
probe="${DP9IK_TEST_PROBE:-./o.livetest}"
factotum_bin="${DP9IK_TEST_FACTOTUM:-$PLAN9/bin/factotum}"
load_p9sk1="${DP9IK_TEST_LOAD_P9SK1:-1}"
p9any_p9sk1_only="${DP9IK_TEST_P9ANY_P9SK1_ONLY:-1}"
attrs="dom=${DP9IK_TEST_DOMAIN} user=${DP9IK_TEST_USER}"
factpid=

stop_factotum() {
	if [ -n "${factpid}" ]; then
		kill "${factpid}" 2>/dev/null || true
		wait "${factpid}" 2>/dev/null || true
		factpid=
	fi
	if command -v fusermount >/dev/null 2>&1; then
		fusermount -uz "${mnt}" 2>/dev/null || true
	fi
	rm -rf "${ns}"
}

cleanup() {
	stop_factotum
}
trap cleanup EXIT HUP INT TERM

start_factotum() {
	mkdir -p "${mnt}"
	NAMESPACE="${ns}" PLAN9="${PLAN9}" "${factotum_bin}" -n -m "${mnt}" -s "${service}" &
	factpid=$!

	for _ in $(seq 1 50); do
		if [ -e "${mnt}/ctl" ]; then
			return
		fi
		sleep 0.1
	done

	echo "factotum mount did not appear at ${mnt}" >&2
	exit 1
}

load_key() {
	printf 'key proto=%s dom=%s user=%s !password=%s\n' \
		"$1" "${DP9IK_TEST_DOMAIN}" "${DP9IK_TEST_USER}" "${DP9IK_TEST_PASSWORD}" |
		9 9p -a "unix!${ns}/${service}" write -l ctl
}

run_probe() {
	FACTOTUM_MNT="${mnt}" "${probe}" "$@"
}

start_factotum
load_key dp9ik
run_probe dp9ik 256 "${attrs}" "${attrs}"
run_probe p9any 256 "${attrs}" "${attrs}"

if [ "${load_p9sk1}" != "0" ]; then
	load_key p9sk1
	run_probe p9sk1 8 "${attrs}" "${attrs}"
	run_probe p9any 256 "${attrs}" "${attrs}"

	if [ "${p9any_p9sk1_only}" != "0" ]; then
		stop_factotum
		start_factotum
		load_key p9sk1
		run_probe p9any 8 "${attrs}" "${attrs}"
	fi
fi
