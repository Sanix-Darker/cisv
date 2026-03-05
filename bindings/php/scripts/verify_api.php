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

$repoRoot = dirname(__DIR__, 3);
$stubFile = $repoRoot . '/bindings/php/stubs/cisv.php';

if (!is_file($stubFile)) {
    fwrite(STDERR, "[cisv][php-api] stub file not found: {$stubFile}\n");
    exit(1);
}

$stubContent = file_get_contents($stubFile);
if ($stubContent === false) {
    fwrite(STDERR, "[cisv][php-api] failed to read stub file: {$stubFile}\n");
    exit(1);
}

preg_match_all('/public\\s+(?:static\\s+)?function\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(/', $stubContent, $matches);
$requiredMethods = array_values(array_unique($matches[1] ?? []));

if ($requiredMethods === []) {
    fwrite(STDERR, "[cisv][php-api] no methods detected in stubs\n");
    exit(1);
}

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
