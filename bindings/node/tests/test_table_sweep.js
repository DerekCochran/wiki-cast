#!/usr/bin/env node
'use strict';
// Parity test: focused table row-attribute sweep.
const { runTests } = require('./helpers');

runTests([
  `{|\n|- 1 CAR\n|}`,
  `{|\n|- 1 car\n|}`,
  `{|\n|- A1 CAR\n|}`,
  `{|\n|- _x :y\n|}`,
  `{|\n|- data-x foo\n|}`,
  `{|\n|- 1=2 CAR\n|}`,
  `{|\n|- "1" CAR\n|}`,
  `{|\n|- '1' CAR\n|}`,
  `{|\n|- {{T}} CAR\n|}`,
  `{|\n|- -{zh-hans:a;zh-hant:b;}- CAR\n|}`,
  `{|\n|- / CAR\n|}`,
  `{|\n|- 123 456\n|}`,
  `{|\n|- a.b c-d e:f\n|}`,
  `{|\n|- \0 1\n|}`,
  `{|\n|- \x7F 1\n|}`,
  `{|\n|- rowspan=4 1 CAR\n|}`,
  `{|\n|- 1   CAR\n|}`,
  `{|\n|- 1\tCAR\n|}`,
  `{|\n|- <!--c--> 1 CAR\n|}`,
  `{|\n|- \0 12t\x7F CAR\n|}`,
], { name: 'table_sweep' });