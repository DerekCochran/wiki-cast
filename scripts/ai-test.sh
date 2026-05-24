#!/bin/bash

set -eou pipefail

node bindings/node/tests/test_parsoid.js
node bindings/node/tests/test_wikitext.js
node bindings/node/tests/test_export.js
node bindings/node/tests/test_wikitext.js
