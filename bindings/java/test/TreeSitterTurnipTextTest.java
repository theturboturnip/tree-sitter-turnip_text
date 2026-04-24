import io.github.treesitter.jtreesitter.Language;
import io.github.treesitter.jtreesitter.turniptext.TreeSitterTurnipText;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;

public class TreeSitterTurnipTextTest {
    @Test
    public void testCanLoadLanguage() {
        assertDoesNotThrow(() -> new Language(TreeSitterTurnipText.language()));
    }
}
