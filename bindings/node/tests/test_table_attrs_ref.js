// Table cell with attribute and inline ref tag (parity minimal repro)
runTests([
  '{|\n| alias = ISO-IR-006,<ref>reftext</ref> ANSI_X3.4-1968\n|}',
], { name: 'table-attrs-ref' });
