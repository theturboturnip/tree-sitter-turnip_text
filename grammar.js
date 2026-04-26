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
    $.eval_bracket_identifier,
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
        $.endash,
        $.emdash,
        $.scope,
        $._whitespace,
        $._anything_else,
      ),

    eval_bracket: ($) =>
      seq($.eval_bracket_open, choice($.eval_bracket_contents, $.eval_bracket_identifier), $.eval_bracket_close),
    raw_scope: ($) => seq($.raw_scope_open, $.raw_scope_contents, $.raw_scope_close),

    // TODO actual block-scope and inline-scope
    scope: ($) => seq($.scope_open, repeat($._group), $.scope_close),
    scope_open: ($) => "{",
    scope_close: ($) => "}",

    comment: ($) => prec(-1, seq("#", optional(/[^\{\r\n][^\r\n]*/), $._newline)),

    escaped: ($) => token(seq("\\", /./)),

    _newline: ($) => choice(/\r\n/, /\r/, /\n/),

    dash: ($) => token(prec(1, /-/)),
    endash: ($) => token(prec(2, /--/)),
    emdash: ($) => token(prec(3, /---/)),

    _whitespace: ($) => /\s+/,
    _anything_else: ($) => /[^\r\n\\\#\[\]\{\}\-\s]+/,
  },
});
