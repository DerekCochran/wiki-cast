You are a coding agent testing the JS and C code for the wiki parser.  

## Step by Step Protocol

1. Run the JS tests.  It is setup to stop on the first failure, so you can investigate the failure immediately.  You must run all the tests for regression:
- Process, run all items
```bash
cd ~/git/wikiparser-node-c-tokenizer
./scripts/ai-test.sh
# If directed to, run the test_wikitext.js test for the specific failing sample.
```
2. Evaluate the new output to identify the if it is one of the below issues.  
  - The JS may produce the wrong AST.  If it is, stop and notify the end user.
  - There may just be a formatting issues between the JS and C AST.  For example, the JS creates a childNode with a `data` property for the text, but the C code just has a text node with a pointer to the text.  If this is the case, stop and notify the end user that the issue is just a formatting issue in the AST output and does not indicate a problem with the tokenization logic.
  - If you are uncertain of how it should look, get the official wikitext documentation to determine the correct output.
  - If it is not one of the above two issues, then continue to step 3.
3. Use the output information to identify a stage for the root cause.  Then set the debug for the specific stage and rerun to get more detailed information about the failure.
```bash
cd ~/git/wikiparser-node-c-tokenizer
WTC_DEBUG_STAGE_1=1 WTC_DEBUG_STAGE_DUMP=1 node bindings/node/tests/test_{type}.js 2>&1 | tail -200
```
4. Use the stage logs and AST analysis to identify the root cause of the failure.  Then fix the C code to maintain parity with the JS code.  If you need additional logs from the code, add it for the stage and rebuild/test.
```C
log_debug_env_token("WTC_DEBUG_STAGE_1", NULL, 
  "Get braces symbol: out=%c base_lc=%s is_magic_out=%d", out, base_lc, *is_magic_out);
```
5. Rebuild the C code and run the tests again to confirm that the issue is resolved.
```bash
cd /home/djc/git/wikiparser-node-c-tokenizer
cmake --build build -j"$(nproc)"
cd ~/git/wikiparser-node-c-tokenizer
WTC_DEBUG_STAGE_1=1 node bindings/node/tests/test_{type}.js 2>&1 | tail -200
# WTC_DEBUG_STAGE_1=1 WTC_DEBUG_STAGE_DUMP=1 node bindings/node/tests/test_{type}.js 2>&1 | tail -200
```

## Rules

1. The test harness has been proved, so do not read those files.  That adds useless information to your context window.

