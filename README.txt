To test teams url use e.g.

    curl.exe -H "Content-Type:application/json" -d "{'text':'Hello World'}" <YOUR WEBHOOK URL>

may need to add   --ca-native   argument for our curl build


or create a JournalParser.conf in same directory as executable with slack and teams urls and run

    JournalTestPost.exe inst_name slack_message teams_message summ_message

