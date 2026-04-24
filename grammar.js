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

    raw_scope: ($) =>
      seq($.raw_scope_open, optional($.raw_scope_internal), $.raw_scope_close),
    raw_scope_open: ($) => choice(/###\{/, /##\{/, /#\{/),
    raw_scope_close: ($) => choice(/\}\#\#\#/, /\}\#\#/, /\}\#/),
    raw_scope_internal: ($) =>
      repeat1(
        choice(
          $.escaped,
          $._newline,
          $.eval_bracket_open,
          $.eval_bracket_close,
          $.comment,
          $.dash,
          $.scope,
          $._anything_else,
        ),
      ),

    eval_bracket_short: ($) =>
      seq(
        $.eval_bracket_short_open,
        optional($.eval_bracket_internal),
        $.eval_bracket_short_close,
      ),
    eval_bracket_short_open: ($) => /\[/,
    eval_bracket_short_close: ($) => /\]/,

    eval_bracket: ($) =>
      seq(
        $.eval_bracket_open,
        optional($.eval_bracket_internal),
        $.eval_bracket_close,
      ),
    eval_bracket_open: ($) => choice(/\[---/, /\[--/, /\[-/),
    eval_bracket_close: ($) => choice(/---\]/, /--\]/, /-\]/),
    eval_bracket_internal: ($) =>
      repeat1(
        choice(
          $.escaped,
          $._newline,
          $.raw_scope_open,
          $.raw_scope_close,
          $.comment,
          $.dash,
          $.scope,
          $._anything_else,
        ),
      ),

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
