declare module 'cisv' {
  export class cisvParser {
    /**
     * Parse CSV file synchronously
     * @param path Path to CSV file
     * @returns Array of rows with string values
     */
    parseSync(path: string): string[][];

    /**
     * Write chunk of CSV data
     * @param chunk Buffer containing CSV data
     */
    write(chunk: ArrayBufferLike | Buffer): void;

    /**
     * Signal end of CSV data
     */
    end(): void;

    /**
     * Get all parsed rows
     * @returns Array of parsed rows
     */
    getRows(): string[][];
  }
}
