from __future__ import print_function

import argparse
import os
import glob
import subprocess
import xml.etree.ElementTree as ET
from contextlib import contextmanager
import shutil

__doc__ = """
Script to import data from journal.xml files into the MySQL database.

To use:
- Copy some journal files from <instrument>\c$\data to the same directory as this file
- Run add_journal_entries.bat --instrument ENGINX --hostname NDXENGINX
- Wait while the data is imported!
"""

INGEST_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = INGEST_DIR
JOURNAL_PARSER_DIR = os.path.abspath(os.path.join(INGEST_DIR, "..", "bin", "windows-x64"))
JOURNAL_PARSER_CONFIG_FILE = os.path.join(JOURNAL_PARSER_DIR, "JournalParser.conf")
JOURNAL_PARSER = os.path.join(JOURNAL_PARSER_DIR, "JournalParser.exe")
JOURNAL_PREFIX = "journal_"
JOURNAL_SUFFIX = ".xml"
JOURNAL_GLOB = JOURNAL_PREFIX + "[0-9]*_[0-9]*" + JOURNAL_SUFFIX


def run_journal_parser(*args):
    with open(os.devnull, "w") as devnull:
        #subprocess.check_call(" ".join([JOURNAL_PARSER] + list(args)), stderr=devnull, stdout=devnull)
        #subprocess.check_call(" ".join([JOURNAL_PARSER] + list(args)))
        subprocess.call(" ".join([JOURNAL_PARSER] + list(args)), stdout=devnull)


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
    parser.add_argument('-host', '--hostname', help="Specify the instrument hostname, e.g. NDXENGINX")
    parser.add_argument('-f', '--files', help="Specify a list of files to add", nargs="+", default=None)
    parser.add_argument('-d', '--dir', help="Directory to ingest", default=None)

    os.environ["JOURNALPARSER_NOMESSAGE"] = "1"

    arguments = parser.parse_args()

    instrument_name = arguments.instrument
    computer_name = arguments.hostname

    if arguments.dir is not None:
        DATA_DIR = arguments.dir
    if arguments.files is None:
        files = glob.glob(JOURNAL_GLOB, root_dir=DATA_DIR)
    else:
        files = arguments.files

    with temporarily_rename_config_file():
        for filename in files:
            year_and_cycle = filename[len(JOURNAL_PREFIX):-len(JOURNAL_SUFFIX)]

            try:
                print("\n\n-----\nParsing {} from {}\n-----\n\n".format(filename, DATA_DIR))
                tree = ET.parse(os.path.join(DATA_DIR, filename))
            except Exception as e:
                print("Malformed data from '{}': {} {}".format(filename, e.__class__.__name__, e))
            try:
#                tree = ET.parse(os.path.join(DATA_DIR, filename))
#                for run in tree.getroot():
#                    print(".", end="")
#                    run_number = int(run.attrib['name'][len(instrument_name):])

#                    run_journal_parser(instrument_name, "{:08d}".format(run_number), "cycle_{}".format(year_and_cycle),
#                                       '"{}"'.format(DATA_DIR), computer_name)
                run_journal_parser(instrument_name, "*", "cycle_{}".format(year_and_cycle),
                                       '"{}"'.format(DATA_DIR), computer_name)
            except Exception as e:
                print("Couldn't load data from '{}': {} {}".format(filename, e.__class__.__name__, e))
                raise
