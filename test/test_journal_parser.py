import os
import unittest
import subprocess
import mysql.connector

EPICS = os.path.join("C:\\", "instrument", "apps", "epics")
JOURNAL_PARSER = os.path.join(EPICS, "isis", "journalparser", "master", "bin", "windows-x64", "JournalParser.exe")


def run_journal_parser(*args):
    with open(os.devnull) as devnull:
        subprocess.check_call(" ".join([JOURNAL_PARSER] + list(args)), stderr=devnull, stdout=devnull)


class JournalParserTests(unittest.TestCase):

    def setUp(self):
        self.connection = \
            mysql.connector.connect(user='journal', password='$journal', host='localhost', database='journal')
        self.cursor = self.connection.cursor()

    def tearDown(self):
        try:
            self.cursor.execute("DELETE FROM journal_entries")
            self.connection.commit()
        except mysql.connector.Error as e:
            print(e)
        finally:
            self.connection.close()

    def test_GIVEN_data_containing_run_WHEN_journal_parser_is_called_THEN_data_is_inserted_into_database(self):

        run_number = 1638
        run_journal_parser("NDW1799", "{:08d}".format(run_number), "cycle_00_0", '"C:\\data"', "NDWNDW1799")

        self.cursor.execute("SELECT run_number FROM journal_entries WHERE run_number={}".format(run_number))

        length = len([o for o in self.cursor])
        self.assertEqual(length, 1)
        self.connection.commit()

    def test_GIVEN_data_not_containing_run_WHEN_journal_parser_is_called_THEN_error(self):
        pass

    def test_GIVEN_invalid_arguments_WHEN_journal_parser_called_THEN_error(self):
        with self.assertRaises(subprocess.CalledProcessError):
            run_journal_parser("junk", "arguments", "should", "raise", "error")


if __name__ == "__main__":
    unittest.main()
