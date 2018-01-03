import os
import unittest
import subprocess
import mysql.connector

EPICS = os.path.join("C:\\", "instrument", "apps", "epics")
JOURNAL_REPO = os.path.join(EPICS, "isis", "journalparser", "master")
JOURNAL_PARSER = os.path.join(JOURNAL_REPO, "bin", "windows-x64", "JournalParser.exe")
JOURNAL_TEST_DIR = os.path.join(JOURNAL_REPO, "test")

INSTRUMENT_NAME = "SYSTEMTEST"
COMPUTER_NAME = "NDW" + INSTRUMENT_NAME

VALID_RUN_NUMBER = 1234  # Appears in journal.xml
INVALID_RUN_NUMBER = 4321  # Does not appear in journal.xml


def run_journal_parser(*args):
    with open(os.devnull) as devnull:
        subprocess.check_call(" ".join([JOURNAL_PARSER] + list(args)), stderr=devnull, stdout=devnull)


class JournalParserTests(unittest.TestCase):

    def setUp(self):

        self.assertIn("EPICS_BASE", os.environ, "Must run these tests in an epics terminal.")

        self.connection = \
            mysql.connector.connect(user='journal', password='$journal', host='localhost', database='journal')
        self.cursor = self.connection.cursor()

    def tearDown(self):
        try:
            self.cursor.execute("DELETE FROM journal_entries WHERE instrument_name='{}'".format(INSTRUMENT_NAME))
            self.connection.commit()
        except mysql.connector.Error as e:
            print(e)
        finally:
            self.connection.close()

    def assert_number_of_database_entries(self, n, run_number):
        self.cursor.execute("SELECT run_number FROM journal_entries WHERE run_number={}".format(run_number))
        self.assertEqual(len([o for o in self.cursor]), n)
        self.connection.commit()

    def test_GIVEN_data_containing_run_WHEN_journal_parser_is_called_THEN_data_is_inserted_into_database(self):

        self.assert_number_of_database_entries(0, VALID_RUN_NUMBER)
        run_journal_parser(COMPUTER_NAME, "{:08d}".format(VALID_RUN_NUMBER), "cycle_00_0",
                           '"{}"'.format(os.path.abspath(JOURNAL_TEST_DIR)), COMPUTER_NAME)
        self.assert_number_of_database_entries(1, VALID_RUN_NUMBER)

    def test_GIVEN_data_not_containing_run_WHEN_journal_parser_is_called_THEN_error(self):
        self.assert_number_of_database_entries(0, INVALID_RUN_NUMBER)
        with self.assertRaises(subprocess.CalledProcessError):
            run_journal_parser(COMPUTER_NAME, "{:08d}".format(INVALID_RUN_NUMBER), "cycle_00_0",
                               '"{}"'.format(os.path.abspath(JOURNAL_TEST_DIR)), COMPUTER_NAME)
        self.assert_number_of_database_entries(0, INVALID_RUN_NUMBER)

    def test_GIVEN_invalid_arguments_WHEN_journal_parser_called_THEN_error(self):
        with self.assertRaises(subprocess.CalledProcessError):
            run_journal_parser("junk", "arguments", "should", "raise", "error")


if __name__ == "__main__":
    unittest.main()
