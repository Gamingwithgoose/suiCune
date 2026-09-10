"""Source-level ownership guards. These do not compile or run the game."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
FIELDS = (
    r"wBattleMon|wEnemyMon|wCurBattleMon|wCurOTMon|"
    r"wBattleParticipants(?:NotFainted|IncludingFainted)|"
    r"w(?:Player|Enemy)(?:SubStatus[1-5]|StatLevels|"
    r"(?:Atk|Def|Spd|SAtk|SDef|Acc|Eva)Level|"
    r"Stats|Attack|Defense|Speed|SpAtk|SpDef)"
)

def executable(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', '', text, flags=re.S)

class BattleOwnershipTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = {}
        for folder in ('engine', 'home', 'util', 'tools', 'mobile', 'data', 'maps'):
            for path in (ROOT / folder).rglob('*.c'):
                cls.sources[path.relative_to(ROOT)] = executable(path.read_text(encoding='utf-8'))

    def test_no_retired_gameplay_access(self):
        pattern = re.compile(r'wram->(?:' + FIELDS + r')\b|hram\.hBattleTurn\b|\bretired_\w+')
        for path, text in self.sources.items():
            with self.subTest(path=path):
                self.assertIsNone(pattern.search(text))

    def test_no_retired_address_transport(self):
        pattern = re.compile(r'\b(?:' + FIELDS + r'|hBattleTurn|'
            r'w(?:BattleMon|EnemyMon)(?:Species|Item|Moves|DVs|PP|Happiness|Level|'
            r'Status|HP|MaxHP|Attack|Defense|Speed|SpclAtk|SpclDef|Stats|Type))\b')
        for path, text in self.sources.items():
            with self.subTest(path=path):
                self.assertIsNone(pattern.search(text))

    def test_battle_stats_are_scalar(self):
        pattern = re.compile(r'gBattle\.(?:player|enemy)\.mon\.'
                             r'(?:hp|maxHP|attack|defense|speed|spclAtk|spclDef|status)\s*\[')
        for path, text in self.sources.items():
            with self.subTest(path=path):
                self.assertIsNone(pattern.search(text))

    def test_no_packed_battle_type_in_consumers(self):
        for path, text in self.sources.items():
            with self.subTest(path=path):
                self.assertNotRegex(text, r'\bstruct\s+BattleMon\b')

if __name__ == '__main__':
    unittest.main()
