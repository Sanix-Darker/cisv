<?php
/**
 * CISV PHP Extension Stubs
 *
 * This file provides IDE autocompletion for the CISV native extension.
 * Do not include this file in production - it's only for IDE support.
 *
 * @package sanix-darker/cisv
 */

if (false) {
    /**
     * High-performance CSV parser with SIMD optimizations.
     */
    class CisvParser
    {
        /**
         * Create a new CSV parser.
         *
         * @param array{
         *     delimiter?: string,
         *     quote?: string,
         *     trim?: bool,
         *     skip_empty?: bool
         * } $options Configuration options
         */
        public function __construct(array $options = []) {}

        /**
         * Parse a CSV file and return all rows.
         *
         * @param string $filename Path to CSV file
         * @return array<int, array<int, string>> Array of row arrays
         * @throws \RuntimeException If file cannot be read
         */
        public function parseFile(string $filename): array {}

        /**
         * Parse a CSV string and return all rows.
         *
         * @param string $csv CSV content
         * @return array<int, array<int, string>> Array of row arrays
         */
        public function parseString(string $csv): array {}

        /**
         * Count rows in a CSV file without full parsing.
         *
         * @param string $filename Path to CSV file
         * @return int Number of rows
         * @throws \RuntimeException If file cannot be read
         */
        public static function countRows(string $filename): int {}

        /**
         * Set the field delimiter.
         *
         * @param string $delimiter Single character delimiter
         * @return $this Fluent interface
         */
        public function setDelimiter(string $delimiter): self {}

        /**
         * Set the quote character.
         *
         * @param string $quote Single character quote
         * @return $this Fluent interface
         */
        public function setQuote(string $quote): self {}
    }
}
