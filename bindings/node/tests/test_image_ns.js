#!/usr/bin/env node
'use strict';
const { runTests } = require('./helpers');

runTests([
  // Test Image: vs File: namespace for nested link in caption
  '[[Image:Foo.jpg|caption [[Link]] text]]',
  '[[File:Foo.jpg|caption [[Link]] text]]',
  '[[File:Foo.jpg|upright|caption [[Link]] text]]',
  '[[Image:Foo.jpg|upright|caption [[Link]] text]]',
]);
