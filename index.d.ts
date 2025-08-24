declare module 'cisv' {
  /**
   * Transform types available for CSV field transformation
   */
  export enum TransformType {
    UPPERCASE = 'uppercase',
    LOWERCASE = 'lowercase',
    TRIM = 'trim',
    TO_INT = 'to_int',
    TO_FLOAT = 'to_float',
    HASH_SHA256 = 'hash_sha256',
    BASE64_ENCODE = 'base64_encode',
    CUSTOM = 'custom'
  }

  /**
   * Transform context for advanced transformations
   */
  export interface TransformContext {
    key?: string | Buffer;
    iv?: string | Buffer;
    extra?: any;
  }

  /**
   * Extended configuration for CSV parsing
   */
  export interface CisvConfig {
    delimiter?: string;
    quote?: string;
    escape?: string | null;
    comment?: string | null;
    trim?: boolean;
    skipEmptyLines?: boolean;
    relaxed?: boolean;
    skipLinesWithError?: boolean;
    maxRowSize?: number;
    fromLine?: number;
    toLine?: number;
  }

  /**
   * Options for CSV parsing (legacy alias for CisvConfig)
   */
  export interface ParseOptions {
    delimiter?: string;
    quote?: string;
    escape?: string;
    headers?: boolean;
    skipEmptyLines?: boolean;
    maxRows?: number;
  }

  /**
   * Options for CSV writing
   */
  export interface WriteOptions {
    delimiter?: string;
    quote?: string;
    escape?: string;
    headers?: boolean | string[];
    quoteAll?: boolean;
    lineEnding?: '\n' | '\r\n';
  }

  /**
   * Transform function type for custom transformations
   */
  export type TransformFunction = (
    value: string,
    rowIndex: number,
    fieldIndex: number
  ) => string;

  /**
   * Information about applied transformations
   */
  export interface TransformInfo {
    [fieldIndex: number]: TransformType | TransformFunction;
  }

  /**
   * Statistics from CSV parsing
   */
  export interface ParseStats {
    rowCount: number;
    fieldCount: number;
    totalBytes: number;
    parseTime: number;
  }

  /**
   * Main CSV parser class with transformation pipeline support
   */
  export class cisvParser {
    constructor(options?: CisvConfig);

    /**
     * Parse CSV file synchronously
     * @param path Path to CSV file
     * @returns Array of rows with string values
     */
    parseSync(path: string): string[][];

    /**
     * Parse CSV file asynchronously
     * @param path Path to CSV file
     * @returns Promise resolving to array of rows
     */
    parse(path: string): Promise<string[][]>;

    /**
     * Parse CSV string content
     * @param content CSV string content
     * @returns Array of rows with string values
     */
    parseString(content: string): string[][];

    /**
     * Write chunk of CSV data (for streaming)
     * @param chunk Buffer containing CSV data
     */
    write(chunk: ArrayBufferLike | Buffer | string): void;

    /**
     * Signal end of CSV data stream
     */
    end(): void;

    /**
     * Get all parsed rows
     * @returns Array of parsed rows
     */
    getRows(): string[][];

    /**
     * Clear all parsed rows
     */
    clear(): void;

    /**
     * Update parser configuration
     * @param config New configuration
     */
    setConfig(config: CisvConfig): void;

    /**
     * Get current parser configuration
     * @returns Parser configuration
     */
    getConfig(): CisvConfig;

    /**
     * Add a transformation to a specific field
     * @param fieldIndex Index of the field to transform (-1 for all fields)
     * @param transform Transform type or custom function
     * @param context Optional transform context
     * @returns this for chaining
     */
    transform(
      fieldIndex: number,
      transform: TransformType | TransformFunction,
      context?: TransformContext
    ): this;

    /**
     * Add multiple transformations at once
     * @param transforms Map of field indices to transform types/functions
     * @returns this for chaining
     */
    transformMany(transforms: Record<number, TransformType | TransformFunction>): this;

    /**
     * Remove transformation from a field
     * @param fieldIndex Index of the field
     * @returns this for chaining
     */
    removeTransform(fieldIndex: number): this;

    /**
     * Clear all transformations
     * @returns this for chaining
     */
    clearTransforms(): this;

    /**
     * Apply transformations to existing data
     * @param data Array of rows to transform
     * @returns Transformed data
     */
    applyTransforms(data: string[][]): string[][];

    /**
     * Set callback for row processing
     * @param callback Function called for each row
     * @returns this for chaining
     */
    onRow(callback: (row: string[], index: number) => void): this;

    /**
     * Set callback for field processing
     * @param callback Function called for each field
     * @returns this for chaining
     */
    onField(callback: (field: string, rowIndex: number, fieldIndex: number) => void): this;

    /**
     * Get statistics about parsed CSV
     * @returns Object with row count, field count, etc.
     */
    getStats(): ParseStats;

    /**
     * Get information about applied transformations
     * @returns Map of field indices to transformation info
     */
    getTransformInfo(): TransformInfo;

    /**
     * Destroy the parser and release resources
     */
    destroy(): void;

    /**
     * Count rows in a CSV file without fully parsing
     * @param path Path to CSV file
     * @returns Number of rows
     */
    static countRows(path: string): number;

    /**
     * Count rows with custom configuration
     * @param path Path to CSV file
     * @param config Optional parser configuration
     * @returns Number of rows
     */
    static countRowsWithConfig(path: string, config?: CisvConfig): number;

    /**
     * Create a new parser instance with transforms
     * @param options Parse options
     * @returns New parser instance
     */
    static create(options?: CisvConfig): cisvParser;
  }

  /**
   * CSV Writer class for generating CSV files
   */
  export class cisvWriter {
    constructor(options?: WriteOptions);

    /**
     * Write CSV data to file
     * @param path Output file path
     * @param data Array of rows to write
     */
    writeSync(path: string, data: any[][]): void;

    /**
     * Write CSV data to file asynchronously
     * @param path Output file path
     * @param data Array of rows to write
     * @returns Promise that resolves when complete
     */
    write(path: string, data: any[][]): Promise<void>;

    /**
     * Convert data to CSV string
     * @param data Array of rows
     * @returns CSV string
     */
    stringify(data: any[][]): string;

    /**
     * Stream write rows
     * @param row Single row to write
     */
    writeRow(row: any[]): void;

    /**
     * Finish streaming write
     */
    end(): void;

    /**
     * Generate test CSV data
     * @param rows Number of rows to generate
     * @param fields Number of fields per row
     * @returns Generated data
     */
    static generate(rows: number, fields?: number): string[][];
  }

  /**
   * Utility functions
   */
  export namespace utils {
    /**
     * Detect CSV delimiter from file sample
     * @param path Path to CSV file
     * @returns Detected delimiter
     */
    export function detectDelimiter(path: string): string;

    /**
     * Validate CSV file structure
     * @param path Path to CSV file
     * @returns Validation result
     */
    export function validate(path: string): {
      valid: boolean;
      errors?: string[];
      warnings?: string[];
    };

    /**
     * Convert CSV to JSON
     * @param data CSV data
     * @param headers Use first row as headers
     * @returns JSON representation
     */
    export function toJSON(data: string[][], headers?: boolean): any[];

    /**
     * Convert JSON to CSV
     * @param data JSON data
     * @returns CSV representation
     */
    export function fromJSON(data: any[]): string[][];

    /**
     * Merge multiple CSV files
     * @param paths Array of file paths
     * @param outputPath Output file path
     * @param options Merge options
     */
    export function merge(
      paths: string[],
      outputPath: string,
      options?: {
        skipHeaders?: boolean;
        delimiter?: string;
      }
    ): void;

    /**
     * Split CSV file into chunks
     * @param path Input file path
     * @param chunkSize Rows per chunk
     * @param outputPrefix Output file prefix
     */
    export function split(
      path: string,
      chunkSize: number,
      outputPrefix: string
    ): string[];
  }

  /**
   * Performance benchmarking utilities
   */
  export namespace benchmark {
    /**
     * Run performance benchmark
     * @param path CSV file path
     * @param iterations Number of iterations
     * @returns Benchmark results
     */
    export function run(
      path: string,
      iterations?: number
    ): {
      avgTime: number;
      minTime: number;
      maxTime: number;
      throughput: number;
    };

    /**
     * Compare parser performance
     * @param paths Array of file paths
     * @returns Comparison results
     */
    export function compare(paths: string[]): Record<string, any>;
  }

  /**
   * Default export
   */
  const cisv: {
    Parser: typeof cisvParser;
    Writer: typeof cisvWriter;
    utils: typeof utils;
    benchmark: typeof benchmark;
    TransformType: typeof TransformType;
  };

  export default cisv;
}
