package tree_sitter_turnip_text_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_turnip_text "github.com/theturboturnip/tree-sitter-turnip_text/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_turnip_text.Language())
	if language == nil {
		t.Errorf("Error loading turnip_text grammar")
	}
}
