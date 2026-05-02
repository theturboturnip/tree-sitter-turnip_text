#include "tree_sitter/parser.h"
#include "tree_sitter/alloc.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// #define xstr(s) str(s)
// #define str(s) #s
// #define assert(x) do { if (!(x)){fprintf(stderr, "Assertion error at file %s line %d: %s", __FILE__, __LINE__, xstr(x));} } while (0);
#define assert(x)

enum TokenType {
  RAW_SCOPE_OPEN,
  RAW_SCOPE_CONTENTS,
  RAW_SCOPE_CLOSE,
  EVAL_BRACKET_OPEN,
  EVAL_BRACKET_CONTENTS,
  EVAL_BRACKET_CLOSE,
  EVAL_BRACKET_IDENTIFIER,
};

enum ScannerState {
    NORMAL,
    RAW_SCOPE,
    EVAL_BRACKET,
};

typedef struct Scanner {
    enum ScannerState state;
    uint64_t state_depth;
    bool expecting_state_close;
} Scanner;

// TODO I've put very defensive  && !lexer->eof(lexer) in any loop on lookahead - is that necessary?

void * tree_sitter_turnip_text_external_scanner_create() {
    Scanner *s = ts_calloc(1, sizeof(Scanner));
    return s;
}
void tree_sitter_turnip_text_external_scanner_destroy(void *payload) {
    Scanner *s = payload;
    free(s);
}
unsigned tree_sitter_turnip_text_external_scanner_serialize(
  void *payload,
  char *buffer
) {
    memcpy(buffer, payload, sizeof(Scanner));
    return sizeof(Scanner);
}
void tree_sitter_turnip_text_external_scanner_deserialize(
  void *payload,
  const char *buffer,
  unsigned length
) {
    if (length == sizeof(Scanner)) {
        memcpy(payload, buffer, sizeof(Scanner));
    }
}

static bool parse_open_raw_scope(Scanner *s, TSLexer *lexer) {
    uint64_t hashes = 0;
    while (lexer->lookahead == '#' && !lexer->eof(lexer)) {
        hashes++;
        lexer->advance(lexer, false);
    }
    if (hashes > 0 && lexer->lookahead == '{') {
        *s = (Scanner) {
            .state = RAW_SCOPE,
            .state_depth = hashes,
            .expecting_state_close = false
        };
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        lexer->result_symbol = RAW_SCOPE_OPEN;
        return true;
    } else {
        return false;
    }
}

static void parse_raw_scope_contents(Scanner *s, TSLexer *lexer) {
    assert (s->state_depth > 0);

    while (1) {
        while(lexer->lookahead != '}' && !lexer->eof(lexer)) {
            lexer->advance(lexer, false);
        }
        if (lexer->eof(lexer)) {
            goto eof;
        }

        // Try to parse dashes, if the next bits end up being a correct ender-token then we need to stop the contents token here.
        lexer->mark_end(lexer);
        // Move past the }
        lexer->advance(lexer, false);

        uint64_t expected_hashes = s->state_depth;
        uint64_t hashes = 0;
        while (lexer->lookahead == '#' && hashes < expected_hashes && !lexer->eof(lexer)) {
            hashes++;
            lexer->advance(lexer, false);
        }
        if (hashes == expected_hashes) {
            // We looked ahead to a true ender token
            s->expecting_state_close = true;
            // DO NOT MARK END, we have already tried
            lexer->result_symbol = RAW_SCOPE_CONTENTS;
            return;
        }
        // The lookahead did not produce a correct ender, loop back around
    }

    eof: {
        s->expecting_state_close = false;
        lexer->mark_end(lexer);
        lexer->result_symbol = RAW_SCOPE_CONTENTS;
        return;
    }
}

static bool parse_close_raw_scope(Scanner *s, TSLexer *lexer) {
    if (lexer->lookahead == '}') {
        lexer->advance(lexer, false);
        uint64_t expected_hashes = s->state_depth;
        uint64_t hashes = 0;
        while (lexer->lookahead == '#' && hashes < expected_hashes && !lexer->eof(lexer)) {
            hashes++;
            lexer->advance(lexer, false);
        }

        if (hashes == expected_hashes) {
            // We have counted a true ender token.
            lexer->mark_end(lexer);
            lexer->result_symbol = RAW_SCOPE_CLOSE;
            *s = (Scanner) {
                .state = NORMAL,
                .state_depth = 0,
                .expecting_state_close = false
            };
            return true;
        }
    }
    return false;
}

static bool parse_open_eval_bracket(Scanner *s, TSLexer *lexer) {
    if (lexer->lookahead == '[') {
        lexer->advance(lexer, false);
        uint64_t dashes = 0;
        while (lexer->lookahead == '-' && !lexer->eof(lexer)) {
            dashes++;
            lexer->advance(lexer, false);
        }

        *s = (Scanner) {
            .state = EVAL_BRACKET,
            .state_depth = dashes,
            .expecting_state_close = false
        };
        lexer->mark_end(lexer);
        lexer->result_symbol = EVAL_BRACKET_OPEN;
        return true;
    }

    return false;
}

// Eval-brackets have a special case for identifier-only contents, trying to follow the official grammar:
// https://docs.python.org/3/reference/lexical_analysis.html#names-identifiers-and-keywords
/*
 NAME:          name_start name_continue*
 name_start:    "a"..."z" | "A"..."Z" | "_" | <non-ASCII character>
 name_continue: name_start | "0"..."9"
 identifier:    <NAME, except keywords>
 */
// ignoring keywords and non-ASCII characters

typedef struct {
    bool first_char;
    bool cannot_be_identifier;
} PythonIdentifierState;

static void step_python_identifier_state(TSLexer *lexer, PythonIdentifierState* p) {
    if (p->cannot_be_identifier) {
        return;
    }
    int32_t c = lexer->lookahead;
    if (p->first_char) {
        if (
            ('a' <= c && c <= 'z') ||
            ('A' <= c && c <= 'Z') ||
            ('_' == c) // TODO || non-ascii-character
        ) {
            // valid
        } else {
            p->cannot_be_identifier = true;
        }

        p->first_char = false;
    } else {
        if (
            ('a' <= c && c <= 'z') ||
            ('A' <= c && c <= 'Z') ||
            ('0' <= c && c <= '9') ||
            ('_' == c) // TODO || non-ascii-character
        ) {
            // valid
        } else {
            p->cannot_be_identifier = true;
        }
    }
}

static void parse_eval_bracket_contents(Scanner *s, TSLexer *lexer) {
    PythonIdentifierState p = (PythonIdentifierState) {
        .first_char = true,
        .cannot_be_identifier = false,
    };
    if (s->state_depth > 0) {
        while (1) {
            while(lexer->lookahead != '-' && !lexer->eof(lexer)) {
                step_python_identifier_state(lexer, &p);
                lexer->advance(lexer, false);
            }
            if (lexer->eof(lexer)) {
                goto eof;
            }

            // Try to parse dashes, if the next bits end up being a correct ender-token then we need to stop the contents token here.
            lexer->mark_end(lexer);
            uint64_t expected_dashes = s->state_depth;
            uint64_t dashes = 0;
            while (lexer->lookahead == '-' && dashes < expected_dashes && !lexer->eof(lexer)) {
                dashes++;
                lexer->advance(lexer, false);
            }
            if (dashes == expected_dashes && lexer->lookahead == ']') {
                // We looked ahead to a true ender token
                s->expecting_state_close = true;
                // DO NOT MARK END, we have already tried
                lexer->result_symbol = (p.cannot_be_identifier) ? EVAL_BRACKET_CONTENTS : EVAL_BRACKET_IDENTIFIER;
                return;
            }
            // The lookahead did not produce a correct ender
        }
    } else {
        while (lexer->lookahead != ']' && !lexer->eof(lexer)) {
            step_python_identifier_state(lexer, &p);
            lexer->advance(lexer, false);
        }
        if (lexer->eof(lexer)) {
            goto eof;
        }
        // Finish. We have not consumed the closer, but we know one is there.
        s->expecting_state_close = true;
        lexer->mark_end(lexer);
        lexer->result_symbol = (p.cannot_be_identifier) ? EVAL_BRACKET_CONTENTS : EVAL_BRACKET_IDENTIFIER;
        return;
    }

    eof: {
        s->expecting_state_close = false;
        lexer->mark_end(lexer);
        // This is an error case, ignore the IDENTIFIER
        lexer->result_symbol = EVAL_BRACKET_CONTENTS;
        return;
    }
}

static bool parse_close_eval_bracket(Scanner *s, TSLexer *lexer) {
    uint64_t expected_dashes = s->state_depth;
    uint64_t dashes = 0;
    while (lexer->lookahead == '-' && dashes < expected_dashes && !lexer->eof(lexer)) {
        dashes++;
        lexer->advance(lexer, false);
    }
    if (dashes == expected_dashes && lexer->lookahead == ']') {
        *s = (Scanner) {
            .state = NORMAL,
            .state_depth = 0,
            .expecting_state_close = false
        };
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        lexer->result_symbol = EVAL_BRACKET_CLOSE;
        return true;
    }
    return false;
}

bool tree_sitter_turnip_text_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
    Scanner *s = payload;

    switch (s->state) {
    case NORMAL: {
        // parse_open_raw_scope and parse_open_eval_bracket leave residual state in the parser on failure.
        // Before, I did if (valid_symbols[X] && parse_X) else if (valid_symbols[Y] && parse_Y), but if parse_X
        // failed and left incomplete state it would be continued in parse_Y.
        // Instead, take advantage of the two tokens being disjoint to only ever try to parse one of them.
        if (valid_symbols[RAW_SCOPE_OPEN] && lexer->lookahead == '#') {
            return parse_open_raw_scope(s, lexer);
        } else if (valid_symbols[EVAL_BRACKET_OPEN] && lexer->lookahead == '[') {
            return parse_open_eval_bracket(s, lexer);
        } else {
            return false;
        }
    }
    case RAW_SCOPE: {
        // Parse the entire raw-scope-contents in one
        if (valid_symbols[RAW_SCOPE_CONTENTS]) {
            parse_raw_scope_contents(s, lexer);
            return true; // There are always contents
        }

        // At this point we should be expecting the scope to close.
        // The only reason we'd close the contents is if we looked ahead and found the closer,
        // or if we EOF-d (in which case this shouldn't be called).
        assert(valid_symbols[RAW_SCOPE_CLOSE]);
        assert(s->expecting_state_close);

        if (!parse_close_raw_scope(s, lexer)) {
            assert(false && "parse_close_raw_scope failed when expecting_state_close");
        }

        return true;
    }
    case EVAL_BRACKET: {
        // Parse the entire eval-bracket-contents in one
        if (valid_symbols[EVAL_BRACKET_CONTENTS] || valid_symbols[EVAL_BRACKET_IDENTIFIER]) {
            parse_eval_bracket_contents(s, lexer);
            return true; // There are always contents
        }

        // At this point we should be expecting the scope to close.
        // The only reason we'd close the contents is if we looked ahead and found the closer,
        // or if we EOF-d (in which case this shouldn't be called).
        assert(valid_symbols[EVAL_BRACKET_CLOSE]);
        assert(s->expecting_state_close);

        if (!parse_close_eval_bracket(s, lexer)) {
            assert(false && "parse_close_eval_bracket failed when expecting_state_close");
        }

        return true;
    }
    }
}
