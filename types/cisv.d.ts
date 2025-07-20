declare module 'cisv' {
  export class cisvParser {
    parseSync(path: string): string[][];
    write(chunk: Buffer): void;
    end(): void;
    getRows(): string[][];
  }
}
