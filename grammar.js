/**
 * @file TurnipText grammar for tree-sitter
 * @author Samuel W. Stark
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

export default grammar({
  name: "turnip_text",

  rules: {
    // TODO: add the actual grammar rules
    source_file: $ => "hello"
  }
});
