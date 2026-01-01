<?php
/**
 * CISV PHP Extension Tests
 */

use PHPUnit\Framework\TestCase;

class CisvTest extends TestCase
{
    public function testParserConstruction()
    {
        $parser = new CisvParser();
        $this->assertInstanceOf(CisvParser::class, $parser);
    }

    public function testParserWithOptions()
    {
        $parser = new CisvParser([
            'delimiter' => ';',
            'quote' => "'",
            'trim' => true,
            'skip_empty' => true
        ]);
        $this->assertInstanceOf(CisvParser::class, $parser);
    }

    public function testParseString()
    {
        $parser = new CisvParser();
        $rows = $parser->parseString("a,b,c\n1,2,3\n");

        $this->assertCount(2, $rows);
        $this->assertEquals(['a', 'b', 'c'], $rows[0]);
        $this->assertEquals(['1', '2', '3'], $rows[1]);
    }

    public function testParseStringWithCustomDelimiter()
    {
        $parser = new CisvParser(['delimiter' => ';']);
        $rows = $parser->parseString("a;b;c\n1;2;3\n");

        $this->assertCount(2, $rows);
        $this->assertEquals(['a', 'b', 'c'], $rows[0]);
    }

    public function testParseStringWithQuotes()
    {
        $parser = new CisvParser();
        $rows = $parser->parseString('"hello, world",b\n');

        $this->assertCount(1, $rows);
        $this->assertEquals('hello, world', $rows[0][0]);
    }

    public function testCountRows()
    {
        $tempFile = tempnam(sys_get_temp_dir(), 'cisv_');
        file_put_contents($tempFile, "a,b,c\n1,2,3\n4,5,6\n");

        $count = CisvParser::countRows($tempFile);
        $this->assertEquals(3, $count);

        unlink($tempFile);
    }

    public function testParseFile()
    {
        $tempFile = tempnam(sys_get_temp_dir(), 'cisv_');
        file_put_contents($tempFile, "name,age\nJohn,30\nJane,25\n");

        $parser = new CisvParser();
        $rows = $parser->parseFile($tempFile);

        $this->assertCount(3, $rows);
        $this->assertEquals(['name', 'age'], $rows[0]);
        $this->assertEquals(['John', '30'], $rows[1]);
        $this->assertEquals(['Jane', '25'], $rows[2]);

        unlink($tempFile);
    }

    public function testMethodChaining()
    {
        $parser = new CisvParser();
        $result = $parser->setDelimiter(';')->setQuote("'");

        $this->assertSame($parser, $result);
    }

    public function testLargeFile()
    {
        $tempFile = tempnam(sys_get_temp_dir(), 'cisv_large_');
        $fp = fopen($tempFile, 'w');

        // Generate 10000 rows
        fwrite($fp, "id,name,value\n");
        for ($i = 0; $i < 10000; $i++) {
            fwrite($fp, "$i,name_$i," . ($i * 1.5) . "\n");
        }
        fclose($fp);

        $parser = new CisvParser();
        $rows = $parser->parseFile($tempFile);

        $this->assertCount(10001, $rows); // header + 10000 rows

        unlink($tempFile);
    }
}
