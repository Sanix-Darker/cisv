declare module 'cisv' {
  /**
   * Configuration options for the CSV parser
   */
  export interface CisvConfig {
    /** Field delimiter character (default: ',') */
    delimiter?: string;

    /** Quote character (default: '"') */
    quote?: string;

    /** Escape character (null for RFC4180 "" style, default: null) */
    escape?: string | null;

    /** Comment character to skip lines (default: null) */
    comment?: string | null;

    /** Trim whitespace from fields (default: false) */
    trim?: boolean;

    /** Skip empty lines (default: false) */
    skipEmptyLines?: boolean;

    /** Use relaxed parsing rules (default: false) */
    relaxed?: boolean;

    /** Skip lines with parse errors (default: false) */
    skipLinesWithError?: boolean;

    /** Maximum row size in bytes (0 = unlimited, default: 0) */
    maxRowSize?: number;

    /** Start parsing from line N (1-based, default: 1) */
    fromLine?: number;

    /** Stop parsing at line N (0 = until end, default: 0) */
    toLine?: number;
  }

  /**
   * Parsed row is an array of string values
   */
  export type ParsedRow = string[];

  /**
   * Statistics about the parsing operation
   */
  export interface ParseStats {
    /** Number of rows parsed */
    rowCount: number;

    /** Number of fields per row */
    fieldCount: number;

    /** Total bytes processed */
    totalBytes: number;

    /** Time taken to parse in milliseconds */
    parseTime: number;

    /** Current line number being processed */
    currentLine: number;
  }

  /**
   * Information about registered transforms
   */
  export interface TransformInfo {
    /** Number of C transforms registered */
    cTransformCount: number;

    /** Number of JavaScript transforms registered */
    jsTransformCount: number;

    /** Field indices that have transforms */
    fieldIndices: number[];
  }

  /**
   * Transform function signature for field transforms
   */
  export type FieldTransformFn = (value: string, fieldIndex: number) => string;

  /**
   * Transform function signature for row transforms
   * @param row - Array of field values
   * @param rowObj - Object with field names as keys (if header is known)
   * @returns Modified row array, object, or null to skip the row
   */
  export type RowTransformFn = (
    row: string[],
    rowObj?: Record<string, string>
  ) => string[] | Record<string, string> | null;

  /**
   * Built-in transform types
   */
  export type TransformType =
    | 'uppercase'
    | 'lowercase'
    | 'trim'
    | 'to_int'
    | 'int'
    | 'to_float'
    | 'float'
    | 'hash_sha256'
    | 'sha256'
    | 'base64_encode'
    | 'base64';

  /**
   * Transform context for advanced transforms
   */
  export interface TransformContext {
    /** Encryption/hash key if needed */
    key?: string;

    /** Initialization vector */
    iv?: string;

    /** Extra context data */
    extra?: any;
  }

  /**
   * High-performance CSV parser with SIMD optimization
   */
  export class cisvParser {
    /**
     * Create a new CSV parser instance
     * @param config - Optional configuration options
     */
    constructor(config?: CisvConfig);

    /**
     * Parse CSV file synchronously
     * @param path - Path to CSV file
     * @returns Array of parsed rows
     */
    parseSync(path: string): ParsedRow[];

    /**
     * Parse CSV file asynchronously
     * @param path - Path to CSV file
     * @returns Promise resolving to array of parsed rows
     */
    parse(path: string): Promise<ParsedRow[]>;

    /**
     * Parse CSV string content
     * @param csv - CSV string content
     * @returns Array of parsed rows
     */
    parseString(csv: string): ParsedRow[];

    /**
     * Write chunk of data for streaming parsing
     * @param chunk - Data chunk as Buffer or string
     */
    write(chunk: Buffer | string): void;

    /**
     * Signal end of streaming data
     */
    end(): void;

    /**
     * Get accumulated parsed rows
     * @returns Array of parsed rows
     */
    getRows(): ParsedRow[];

    /**
     * Clear accumulated data
     */
    clear(): void;

    /**
     * Set parser configuration
     * @param config - Configuration options
     */
    setConfig(config: CisvConfig): void;

    /**
     * Get current parser configuration
     * @returns Current configuration
     */
    getConfig(): CisvConfig;

    /**
     * Add field transform by index or name
     * @param field - Field index (0-based) or field name, use -1 for all fields
     * @param transform - Transform type or custom function
     * @param context - Optional transform context
     * @returns Parser instance for chaining
     */
    transform(
      field: number | string,
      transform: TransformType | FieldTransformFn,
      context?: TransformContext
    ): this;

    /**
     * Add row-level transform
     * @param transform - Row transform function
     * @returns Parser instance for chaining
     */
    transformRow(transform: RowTransformFn): this;

    /**
     * Set header fields for field name mapping
     * @param fields - Array of field names
     * @returns Parser instance for chaining
     */
    setHeader(fields: string[]): this;

    /**
     * Remove transform for specific field
     * @param field - Field index or name
     * @returns Parser instance for chaining
     */
    removeTransform(field: number | string): this;

    /**
     * Clear all transforms
     * @returns Parser instance for chaining
     */
    clearTransforms(): this;

    /**
     * Get parsing statistics
     * @returns Statistics object
     */
    getStats(): ParseStats;

    /**
     * Get information about registered transforms
     * @returns Transform information
     */
    getTransformInfo(): TransformInfo;

    /**
     * Destroy parser and free resources
     */
    destroy(): void;

    /**
     * Count rows in CSV file without parsing
     * @param path - Path to CSV file
     * @returns Number of rows
     */
    static countRows(path: string): number;

    /**
     * Count rows with specific configuration
     * @param path - Path to CSV file
     * @param config - Configuration options
     * @returns Number of rows
     */
    static countRowsWithConfig(path: string, config?: CisvConfig): number;
  }

  /**
   * Transform type constants
   */
  export const TransformType: {
    readonly UPPERCASE: 'uppercase';
    readonly LOWERCASE: 'lowercase';
    readonly TRIM: 'trim';
    readonly TO_INT: 'to_int';
    readonly TO_FLOAT: 'to_float';
    readonly HASH_SHA256: 'hash_sha256';
    readonly BASE64_ENCODE: 'base64_encode';
  };

  /**
   * Library version
   */
  export const version: string;
}
