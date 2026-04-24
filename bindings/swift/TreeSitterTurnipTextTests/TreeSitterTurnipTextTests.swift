import XCTest
import SwiftTreeSitter
import TreeSitterTurnipText

final class TreeSitterTurnipTextTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_turnip_text())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading turnip_text grammar")
    }
}
