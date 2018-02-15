from __future__ import print_function

import argparse
import os
import subprocess
import xml.etree.ElementTree as ET
from contextlib import contextmanager
import shutil

JOURNAL_PARSER_DIR = os.path.join(os.path.abspath(__file__), "..", "bin", "windows-x64", "JournalParser.exe")
JOURNAL_PARSER_CONFIG_FILE = os.path.join(JOURNAL_PARSER_DIR, "JournalParser.conf")
JOURNAL_PARSER = os.path.join(JOURNAL_PARSER_DIR, "JournalParser.exe")
JOURNAL_PREFIX = "journal_"


def run_journal_parser(*args):
    with open(os.devnull, "w") as devnull:
        subprocess.check_call(" ".join([JOURNAL_PARSER] + list(args)), stderr=devnull, stdout=devnull)


@contextmanager
def temporarily_rename_config_file():
    """
    Temporarily remove the slack config file so that the journal parser can't send slack messages, put it back when done
    """
    temp_file_name = JOURNAL_PARSER_CONFIG_FILE + ".temp"
    if os.path.exists(JOURNAL_PARSER_CONFIG_FILE):
        shutil.move(JOURNAL_PARSER_CONFIG_FILE, temp_file_name)
    try:
        yield
    finally:
        if os.path.exists(temp_file_name):
            shutil.move(temp_file_name, JOURNAL_PARSER_CONFIG_FILE)


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="""Import data from journal XML files into MySQL.""")
    parser.add_argument('-i', '--instrument', help="Specify the instrument to run on, e.g. ENGINX")
    parser.add_argument('-h', '--hostname', help="Specify the instrument hostname, e.g. NDXENGINX")
    parser.add_argument('-f', '--files', help="Specify a list of files to add", nargs="+", default=None)
    arguments = parser.parse_args()

    instrument_name = arguments.instrument
    computer_name = arguments.hostname

    if arguments.files is None:
        files = [f for f in os.listdir(os.path.abspath(__file__)) if f.startswith(JOURNAL_PREFIX)]
    else:
        files = arguments.files

    with temporarily_rename_config_file():
        for filename in files:
            year_and_cycle = filename[len(JOURNAL_PREFIX):]

            try:
                print("\n\n-----\nParsing {}\n-----\n\n".format(filename))

                tree = ET.parse(filename)
                for run in tree.getroot():
                    print(".", end="")
                    run_number = int(run.attrib['name'][len(instrument_name):])

                    run_journal_parser(computer_name, "{:08d}".format(run_number), "cycle_{}".format(year_and_cycle),
                                       '"{}"'.format(os.path.abspath(__file__)), computer_name)
            except Exception as e:
                raise Exception("Couldn't load data from '{}': {} {}".format(filename, e.__class__.__name__, e))
