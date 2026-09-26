#!/bin/bash
# Functional test script for the Redis clone.
# Requires redis-cli (sudo apt install redis-tools) and the server
# already running on port 6379.

HOST="127.0.0.1"
PORT="6379"
PASS=0
FAIL=0

run() {
    redis-cli -h "$HOST" -p "$PORT" "$@"
}

check() {
    local desc="$1"
    local expected="$2"
    local actual="$3"
    if [ "$actual" == "$expected" ]; then
        echo "PASS: $desc"
        PASS=$((PASS+1))
    else
        echo "FAIL: $desc  (expected '$expected', got '$actual')"
        FAIL=$((FAIL+1))
    fi
}

echo "=== Basic connectivity ==="
check "PING" "PONG" "$(run PING)"

echo ""
echo "=== SET / GET ==="
run FLUSHALL > /dev/null
check "SET returns OK" "OK" "$(run SET foo bar)"
check "GET returns stored value" "bar" "$(run GET foo)"
check "GET on missing key returns nil" "" "$(run GET nosuchkey)"

echo ""
echo "=== DEL / EXISTS ==="
check "EXISTS on present key" "1" "$(run EXISTS foo)"
check "DEL removes key" "1" "$(run DEL foo)"
check "EXISTS after DEL" "0" "$(run EXISTS foo)"

echo ""
echo "=== INCR / DECR ==="
run SET counter 10 > /dev/null
check "INCR increments" "11" "$(run INCR counter)"
check "DECR decrements" "10" "$(run DECR counter)"
check "INCR on missing key starts at 1" "1" "$(run INCR newcounter)"

echo ""
echo "=== APPEND / STRLEN ==="
run SET greeting "Hello" > /dev/null
check "APPEND extends string" "11" "$(run APPEND greeting " World")"
check "STRLEN matches length" "11" "$(run STRLEN greeting)"

echo ""
echo "=== TTL / EXPIRE ==="
run SET tempkey value > /dev/null
check "TTL on key with no expiry" "-1" "$(run TTL tempkey)"
run EXPIRE tempkey 100 > /dev/null
ttl_val=$(run TTL tempkey)
if [ "$ttl_val" -gt 0 ] && [ "$ttl_val" -le 100 ]; then
    echo "PASS: TTL after EXPIRE is reasonable ($ttl_val)"
    PASS=$((PASS+1))
else
    echo "FAIL: TTL after EXPIRE looks wrong (got $ttl_val)"
    FAIL=$((FAIL+1))
fi
check "TTL on missing key returns -2" "-2" "$(run TTL nosuchkey)"

echo ""
echo "=== TYPE ==="
check "TYPE on string key" "string" "$(run TYPE greeting)"
check "TYPE on missing key" "none" "$(run TYPE nosuchkey)"

echo ""
echo "=== ECHO ==="
check "ECHO returns same string" "hello there" "$(run ECHO "hello there")"

echo ""
echo "=== DBSIZE / FLUSHALL ==="
run FLUSHALL > /dev/null
check "DBSIZE after FLUSHALL is 0" "0" "$(run DBSIZE)"
run SET a 1 > /dev/null
run SET b 2 > /dev/null
check "DBSIZE counts keys correctly" "2" "$(run DBSIZE)"

echo ""
echo "=== Unknown command handling ==="
result=$(run NOTACOMMAND 2>&1)
if [[ "$result" == *"ERR"* ]] || [[ "$result" == *"error"* ]]; then
    echo "PASS: unknown command returns an error"
    PASS=$((PASS+1))
else
    echo "FAIL: unknown command did not return an error (got: $result)"
    FAIL=$((FAIL+1))
fi

echo ""
echo "======================================"
echo "Results: $PASS passed, $FAIL failed"
echo "======================================"