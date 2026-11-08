#!/usr/bin/env python3
"""Verifiche del classificatore del kit a sei stadi (senza file audio)."""
import unittest

from build_ordered_kit import classify, make_kit


class OrderedKitTests(unittest.TestCase):
    def test_stages_and_midi_mapping(self):
        wav = [
            "01 Natural /Indra Kick low.wav",
            "01 Natural /Indra Kick low Over.wav",
            "01 Natural /Indra Kick low room.wav",
            "02 Natural/S 1.wav",
            "03 processed/Kick.wav",
            "04 processed/Rullo bordo.wav",
            "05 processed/KICK.wav",
            "06 Electro/Kick/Kick Want2Survive DS 1.wav",
            "06 Electro/HiHat/OpenHH Atomized.wav",
            "06 Electro/Cymbal/RevCymbal SteamPipe.wav",
        ]
        kit = make_kit(wav)
        self.assertEqual(kit["version"], 2)
        self.assertEqual(kit["sampleFiles"], len(wav))
        self.assertEqual([s["position"] for s in kit["morphStages"]], [0, 20, 40, 60, 80, 100])
        pieces = {p["id"]: p for p in kit["pieces"]}
        kick = pieces["kick"]
        self.assertEqual(kick["note"], 36)
        self.assertEqual(kick["stages"][0][0]["oh"], "01 Natural /Indra Kick low Over.wav")
        self.assertEqual(kick["stages"][0][0]["room"], "01 Natural /Indra Kick low room.wav")
        self.assertEqual(len(kick["stages"][0]), 1)
        self.assertEqual(len(kick["stages"][2]), 1)
        self.assertEqual(len(kick["stages"][4]), 1)
        self.assertEqual(len(kick["stages"][5]), 1)
        self.assertEqual(pieces["hihat_open"]["note"], 46)
        self.assertEqual(pieces["reverse_cymbal"]["note"], 52)
        self.assertEqual(pieces["snare_rim"]["note"], 37)

    def test_classification(self):
        self.assertEqual(classify(1, "02 Natural/hh F long 1.wav"), "hihat_foot")
        self.assertEqual(classify(1, "02 Natural/hh op bell.wav"), "hihat_open")
        self.assertEqual(classify(3, "04 processed/Rullo rimshot.wav"), "snare_rimshot")
        self.assertEqual(classify(4, "05 processed/TIMPANO2.wav"), "floor_tom")
        self.assertEqual(classify(5, "06 Electro/Cymbal/Ride Atomized 1.wav"), "ride")


if __name__ == "__main__":
    unittest.main()
