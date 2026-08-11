import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from silkscreen_art import ART_NAME, apply_art, extract_art


class SilkscreenArtTests(unittest.TestCase):
    def test_extract_hides_reference_and_apply_is_single_use(self):
        source = '''(kicad_pcb\n\t(footprint "LOGO"\n\t\t(layer "F.Cu")\n\t\t(at 10 20)\n\t\t(property "Reference" "G***"\n\t\t\t(at 0 0)\n\t\t\t(layer "F.SilkS")\n\t\t)\n\t\t(fp_line (start 0 0) (end 1 1) (stroke (width 0.2) (type solid)) (layer "F.SilkS"))\n\t)\n)\n'''
        target = '(kicad_pcb\n\t(gr_line (start 0 0) (end 1 0) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts"))\n)\n'
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source_path = root / "source.kicad_pcb"
            art_path = root / "art.kicad_mod"
            target_path = root / "target.kicad_pcb"
            source_path.write_text(source, encoding="utf-8")
            target_path.write_text(target, encoding="utf-8")
            extract_art(source_path, art_path)
            art = art_path.read_text(encoding="utf-8")
            self.assertIn(f'(footprint "{ART_NAME}"', art)
            self.assertIn("(hide yes)", art)
            apply_art(target_path, art_path, 109.5, 74.5)
            result = target_path.read_text(encoding="utf-8")
            self.assertEqual(result.count(f'(footprint "{ART_NAME}"'), 1)
            self.assertIn("(at 109.5 74.5)", result)
            with self.assertRaises(ValueError):
                apply_art(target_path, art_path, 109.5, 74.5)


if __name__ == "__main__":
    unittest.main()
