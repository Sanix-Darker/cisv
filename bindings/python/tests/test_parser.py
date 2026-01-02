"""Tests for CISV Python bindings."""

import pytest
import tempfile
import os

from cisv import CisvParser, parse_file, parse_string, count_rows


class TestCisvParser:
    def test_parse_simple_string(self):
        """Test parsing a simple CSV string."""
        csv = "a,b,c\n1,2,3\n4,5,6\n"
        rows = parse_string(csv)
        assert len(rows) == 3
        assert rows[0] == ['a', 'b', 'c']
        assert rows[1] == ['1', '2', '3']
        assert rows[2] == ['4', '5', '6']

    def test_parse_quoted_fields(self):
        """Test parsing CSV with quoted fields."""
        csv = '"hello, world",b\n"test ""quote""",c\n'
        rows = parse_string(csv)
        assert len(rows) == 2
        assert rows[0][0] == 'hello, world'

    def test_parse_custom_delimiter(self):
        """Test parsing with custom delimiter."""
        csv = "a;b;c\n1;2;3\n"
        parser = CisvParser(delimiter=';')
        rows = parser.parse_string(csv)
        assert len(rows) == 2
        assert rows[0] == ['a', 'b', 'c']

    def test_parse_file(self):
        """Test parsing a CSV file."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
            f.write("id,name,value\n")
            f.write("1,test,100\n")
            f.write("2,demo,200\n")
            f.flush()

            try:
                rows = parse_file(f.name)
                assert len(rows) == 3
                assert rows[0] == ['id', 'name', 'value']
            finally:
                os.unlink(f.name)

    def test_count_rows(self):
        """Test counting rows in a file."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
            f.write("a,b,c\n")
            f.write("1,2,3\n")
            f.write("4,5,6\n")
            f.flush()

            try:
                count = count_rows(f.name)
                assert count == 3
            finally:
                os.unlink(f.name)

    def test_empty_string(self):
        """Test parsing an empty string."""
        rows = parse_string("")
        assert len(rows) == 0


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
