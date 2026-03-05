<?php

declare(strict_types=1);

if (!extension_loaded('cisv')) {
    fwrite(STDERR, "[cisv][php-api] extension 'cisv' is not loaded\n");
    exit(1);
}

if (!class_exists('CisvParser')) {
    fwrite(STDERR, "[cisv][php-api] class CisvParser is missing\n");
    exit(1);
}

$requiredMethods = [
    '__construct',
    'parseFile',
    'parseString',
    'countRows',
    'setDelimiter',
    'setQuote',
    'parseFileParallel',
    'parseFileBenchmark',
    'openIterator',
    'fetchRow',
    'closeIterator',
];

$reflection = new ReflectionClass('CisvParser');
$available = array_map(
    static fn(ReflectionMethod $method): string => $method->getName(),
    $reflection->getMethods()
);

sort($available);
$missing = array_values(array_diff($requiredMethods, $available));

if ($missing !== []) {
    fwrite(STDERR, "[cisv][php-api] missing methods: " . implode(', ', $missing) . "\n");
    fwrite(STDERR, "[cisv][php-api] available methods: " . implode(', ', $available) . "\n");
    exit(1);
}

echo "[cisv][php-api] OK: all required methods are exported\n";
