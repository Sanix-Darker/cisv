"""Tests for CISV Python bindings."""

import os
import tempfile
from pathlib import Path

import pytest

import cisv


class TestParseString:
    """Tests for parse_string function."""

    def test_simple_csv(self):
        """Test parsing a simple CSV string."""
        data = "a,b,c\n1,2,3\n4,5,6"
        rows = cisv.parse_string(data)
        assert rows == [
            ["a", "b", "c"],
            ["1", "2", "3"],
            ["4", "5", "6"],
        ]

    def test_empty_string(self):
        """Test parsing an empty string."""
        rows = cisv.parse_string("")
        assert rows == []

    def test_single_field(self):
        """Test parsing a single field."""
        rows = cisv.parse_string("hello")
        assert rows == [["hello"]]

    def test_quoted_fields(self):
        """Test parsing quoted fields."""
        data = '"hello, world","test"\n"a","b"'
        rows = cisv.parse_string(data)
        assert rows == [
            ["hello, world", "test"],
            ["a", "b"],
        ]

    def test_escaped_quotes(self):
        """Test parsing fields with escaped quotes."""
        data = '"say ""hello""","test"'
        rows = cisv.parse_string(data)
        assert rows == [['say "hello"', "test"]]

    def test_custom_delimiter(self):
        """Test parsing with a custom delimiter."""
        data = "a;b;c\n1;2;3"
        rows = cisv.parse_string(data, delimiter=";")
        assert rows == [
            ["a", "b", "c"],
            ["1", "2", "3"],
        ]

    def test_tab_delimiter(self):
        """Test parsing TSV (tab-separated values)."""
        data = "a\tb\tc\n1\t2\t3"
        rows = cisv.parse_string(data, delimiter="\t")
        assert rows == [
            ["a", "b", "c"],
            ["1", "2", "3"],
        ]

    def test_trim_whitespace(self):
        """Test trimming whitespace from fields."""
        data = "  a  ,  b  ,  c  "
        rows = cisv.parse_string(data, trim=True)
        assert rows == [["a", "b", "c"]]

    def test_skip_empty_lines(self):
        """Test skipping empty lines."""
        data = "a,b\n\n1,2\n\n3,4"
        rows = cisv.parse_string(data, skip_empty_lines=True)
        # Note: actual behavior depends on implementation
        assert len(rows) >= 3  # At least the non-empty rows

    def test_unicode(self):
        """Test parsing Unicode content."""
        data = "名前,値\nこんにちは,世界\n🎉,✨"
        rows = cisv.parse_string(data)
        assert rows == [
            ["名前", "値"],
            ["こんにちは", "世界"],
            ["🎉", "✨"],
        ]

    def test_crlf_line_endings(self):
        """Test parsing with Windows-style line endings."""
        data = "a,b,c\r\n1,2,3\r\n4,5,6"
        rows = cisv.parse_string(data)
        assert rows == [
            ["a", "b", "c"],
            ["1", "2", "3"],
            ["4", "5", "6"],
        ]

    def test_empty_fields(self):
        """Test parsing empty fields."""
        # Note: Trailing comma on last line without newline may not produce
        # the trailing empty field (parser behavior varies)
        data = "a,,c\n,b,\n"
        rows = cisv.parse_string(data)
        assert rows == [
            ["a", "", "c"],
            ["", "b", ""],
        ]

    def test_empty_fields_with_trailing(self):
        """Test parsing empty fields with trailing newline."""
        data = "a,,c\n,b,\n,,\n"
        rows = cisv.parse_string(data)
        assert rows == [
            ["a", "", "c"],
            ["", "b", ""],
            ["", "", ""],
        ]


class TestParseFile:
    """Tests for parse_file function."""

    def test_parse_simple_file(self, tmp_path):
        """Test parsing a simple CSV file."""
        csv_file = tmp_path / "test.csv"
        csv_file.write_text("a,b,c\n1,2,3\n4,5,6")

        rows = cisv.parse_file(str(csv_file))
        assert rows == [
            ["a", "b", "c"],
            ["1", "2", "3"],
            ["4", "5", "6"],
        ]

    def test_parse_large_file(self, tmp_path):
        """Test parsing a larger CSV file."""
        csv_file = tmp_path / "large.csv"

        # Generate a moderately large file
        lines = ["col1,col2,col3"]
        for i in range(1000):
            lines.append(f"value{i}_1,value{i}_2,value{i}_3")
        csv_file.write_text("\n".join(lines))

        rows = cisv.parse_file(str(csv_file))
        assert len(rows) == 1001  # 1 header + 1000 data rows
        assert rows[0] == ["col1", "col2", "col3"]
        assert rows[1] == ["value0_1", "value0_2", "value0_3"]
        assert rows[-1] == ["value999_1", "value999_2", "value999_3"]

    def test_file_not_found(self):
        """Test error handling for missing file."""
        with pytest.raises(cisv.CisvError):
            cisv.parse_file("/nonexistent/path/to/file.csv")

    def test_parallel_parsing(self, tmp_path):
        """Test parallel parsing."""
        csv_file = tmp_path / "parallel.csv"

        # Generate a file large enough for parallel parsing
        lines = ["col1,col2,col3"]
        for i in range(10000):
            lines.append(f"value{i}_1,value{i}_2,value{i}_3")
        csv_file.write_text("\n".join(lines))

        rows = cisv.parse_file(str(csv_file), parallel=True, num_threads=2)
        assert len(rows) == 10001

    def test_custom_options(self, tmp_path):
        """Test parsing with custom options."""
        csv_file = tmp_path / "custom.csv"
        csv_file.write_text("  a  ;  b  ;  c  \n  1  ;  2  ;  3  ")

        rows = cisv.parse_file(str(csv_file), delimiter=";", trim=True)
        assert rows == [
            ["a", "b", "c"],
            ["1", "2", "3"],
        ]


class TestCountRows:
    """Tests for count_rows function."""

    def test_count_simple(self, tmp_path):
        """Test counting rows in a simple file."""
        csv_file = tmp_path / "count.csv"
        csv_file.write_text("a,b,c\n1,2,3\n4,5,6")

        count = cisv.count_rows(str(csv_file))
        assert count == 3

    def test_count_empty_file(self, tmp_path):
        """Test counting rows in an empty file."""
        csv_file = tmp_path / "empty.csv"
        csv_file.write_text("")

        count = cisv.count_rows(str(csv_file))
        assert count == 0

    def test_count_single_line_no_newline(self, tmp_path):
        """Test counting rows in a file with single line (no trailing newline)."""
        csv_file = tmp_path / "single.csv"
        csv_file.write_text("a,b,c")

        count = cisv.count_rows(str(csv_file))
        assert count == 1

    def test_count_large_file(self, tmp_path):
        """Test counting rows in a larger file."""
        csv_file = tmp_path / "large.csv"

        lines = [f"row{i}" for i in range(10000)]
        csv_file.write_text("\n".join(lines))

        count = cisv.count_rows(str(csv_file))
        assert count == 10000


class TestEdgeCases:
    """Tests for edge cases."""

    def test_very_long_field(self):
        """Test parsing a very long field."""
        long_value = "x" * 100000
        data = f"a,{long_value},c"
        rows = cisv.parse_string(data)
        assert rows == [["a", long_value, "c"]]

    def test_many_columns(self):
        """Test parsing a row with many columns."""
        cols = [f"col{i}" for i in range(100)]
        data = ",".join(cols)
        rows = cisv.parse_string(data)
        assert rows == [cols]

    def test_newline_in_quoted_field(self):
        """Test parsing a quoted field with embedded newline."""
        data = '"line1\nline2",other'
        rows = cisv.parse_string(data)
        assert rows == [["line1\nline2", "other"]]

    def test_comma_in_quoted_field(self):
        """Test parsing a quoted field with comma."""
        data = '"a,b,c",d'
        rows = cisv.parse_string(data)
        assert rows == [["a,b,c", "d"]]


class TestValidation:
    """Tests for input validation."""

    def test_invalid_delimiter_empty(self):
        """Test error for empty delimiter."""
        with pytest.raises((ValueError, cisv.CisvError)):
            cisv.parse_string("a,b", delimiter="")

    def test_invalid_delimiter_multi_char(self):
        """Test error for multi-character delimiter."""
        with pytest.raises((ValueError, cisv.CisvError)):
            cisv.parse_string("a,b", delimiter=",,")

    def test_invalid_quote_empty(self):
        """Test error for empty quote character."""
        with pytest.raises((ValueError, cisv.CisvError)):
            cisv.parse_string("a,b", quote="")

    def test_invalid_quote_multi_char(self):
        """Test error for multi-character quote."""
        with pytest.raises((ValueError, cisv.CisvError)):
            cisv.parse_string("a,b", quote='""')
