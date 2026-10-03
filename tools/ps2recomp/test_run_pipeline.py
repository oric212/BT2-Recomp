"""Synthetic provenance tests; no game bytes or generated retail source required."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import run_pipeline


class GenerationProvenanceTests(unittest.TestCase):
    def setUp(self):
        fixture_parent = run_pipeline.ROOT / "work/test-provenance"
        fixture_parent.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=fixture_parent)
        self.root = Path(self.temp.name)
        if not self.root.resolve().is_relative_to(fixture_parent.resolve()):
            raise ValueError("Unexpected test cleanup location")
        self.addCleanup(self.temp.cleanup)
        files = {"rom/SLUS_214.41": b"authored fixture", "config/ps2recomp.lock.json": b"lock",
                 "build/ps2recomp-tools/ps2xRecomp/Debug/ps2_recomp.exe": b"tool fixture",
                 "work/bt2-generation/candidates.csv": b"map fixture",
                 "game/generated/bt2/fixture.cpp": b"// authored source\n"}
        for name, contents in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(contents)
        digest = lambda name: hashlib.sha256(files[name]).hexdigest()
        manifest = {"executable_sha256": digest("rom/SLUS_214.41"),
                    "map_sha256": digest("work/bt2-generation/candidates.csv"),
                    "lock_sha256": digest("config/ps2recomp.lock.json"),
                    "recompiler_sha256": digest("build/ps2recomp-tools/ps2xRecomp/Debug/ps2_recomp.exe"),
                    "files": {"fixture.cpp": digest("game/generated/bt2/fixture.cpp")}}
        (self.root / "work/bt2-generation/generation.manifest.json").write_text(json.dumps(manifest))
        self.root_patch = patch.object(run_pipeline, "ROOT", self.root)
        self.hash_patch = patch.object(run_pipeline, "LOCAL_CANDIDATE_SHA256", manifest["executable_sha256"])
        self.root_patch.start()
        self.hash_patch.start()
        self.addCleanup(self.root_patch.stop)
        self.addCleanup(self.hash_patch.stop)

    def test_matching_generation_accepted(self):
        run_pipeline.verify_generated()

    def test_modified_generated_source_rejected(self):
        (self.root / "game/generated/bt2/fixture.cpp").write_text("changed source")
        with self.assertRaises(ValueError):
            run_pipeline.verify_generated()

    def test_stale_extra_source_rejected(self):
        (self.root / "game/generated/bt2/stale.cpp").write_text("stale source")
        with self.assertRaises(ValueError):
            run_pipeline.verify_generated()

    def test_changed_lock_rejected(self):
        (self.root / "config/ps2recomp.lock.json").write_text("different lock")
        with self.assertRaises(ValueError):
            run_pipeline.verify_generated()


if __name__ == "__main__":
    unittest.main()
