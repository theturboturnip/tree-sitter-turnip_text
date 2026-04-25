/**
 * @file TurnipText grammar for tree-sitter
 * @author Samuel W. Stark
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

export default grammar({
  name: "turnip_text",

  extras: ($) => [],

  // based on https://www.jonashietala.se/blog/2024/03/19/lets_create_a_tree-sitter_grammar/#External-scanner
  // The principle is this: when the external scanner sees a raw_scope_open of a given depth or an eval_bracket of a given depth,
  // subsequent checks for _contents or _close will only close on that same depth and no other.
  externals: ($) => [
    $.raw_scope_open,
    $.raw_scope_contents,
    $.raw_scope_close,
    $.eval_bracket_open,
    $.eval_bracket_contents,
    $.eval_bracket_close,
  ],

  rules: {
    source_file: ($) => repeat($._group),

    _group: ($) =>
      choice(
        $.escaped,
        $._newline,
        $.raw_scope,
        $.eval_bracket,
        $.comment,
        $.dash,
        $.scope,
        $._anything_else,
      ),

    eval_bracket: ($) =>
      seq($.eval_bracket_open, $.eval_bracket_contents, $.eval_bracket_close),
    raw_scope: ($) =>
      seq($.raw_scope_open, $.raw_scope_contents, $.raw_scope_close),

    // TODO actual block-scope and inline-scope
    scope: ($) => seq("{", $._group, "}"),

    comment: ($) =>
      prec(-1, seq("#", optional(/[^\{\r\n][^\r\n]*/), $._newline)),

    escaped: ($) => token(seq("\\", /./)),

    _newline: ($) => choice(/\r\n/, /\r/, /\n/),

    dash: ($) => choice(/-/, /--/, /---/),

    _anything_else: ($) => /[^\r\n\\\#\[\]\{\}\-]+/,
  },
});
