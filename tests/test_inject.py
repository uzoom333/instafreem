"""Testa o empacotador num Instagram.app falso, sem o IPA de verdade.

Precisa de Zig no PATH: compila o tweak e um executável iOS mínimo contra os
stubs .tbd de tests/stubs, porque fora do Windows o `-undefined dynamic_lookup`
derruba o linker do Zig 0.17. O dylib daqui serve para testar, não para o iPhone.

Uso: python -m unittest discover tests
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

from PIL import Image

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STUBS = os.path.join(RAIZ, "tests", "stubs")
sys.path.insert(0, RAIZ)
import inject  # noqa: E402

ALVO = "aarch64-ios.16.3"


def zig(*args):
    subprocess.run(["zig", "cc", "-target", ALVO, "-O2", "-nostdlib", *args,
                    os.path.join(STUBS, "libobjc.tbd"), os.path.join(STUBS, "libSystem.tbd")],
                   check=True, capture_output=True)


@unittest.skipUnless(shutil.which("zig"), "Zig não está no PATH")
class Empacotador(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.dylib = os.path.join(self.tmp, "InstagramMod.dylib")
        zig("-dynamiclib", "-fvisibility=default", "-Wall", "-Werror",
            "-Wl,-install_name,@executable_path/InstagramMod.dylib",
            "-o", self.dylib, os.path.join(RAIZ, "src", "instagram_mod.c"))

        self.src = os.path.join(self.tmp, "decrypted")
        app = os.path.join(self.src, "Payload", "Instagram.app")
        os.makedirs(os.path.join(app, "IGLaunchScreen.storyboardc"))
        main_c = os.path.join(self.tmp, "main.c")
        with open(main_c, "w") as f:
            f.write("int main(void) { return 0; }\n")
        zig("-o", os.path.join(app, "Instagram"), main_c)
        Image.new("RGBA", (120, 120), (200, 50, 120, 255)).save(os.path.join(app, "Prod60x60@2x.png"))
        Image.new("RGBA", (40, 40), (255, 0, 0, 255)).save(os.path.join(app, "IGLaunchScreenPulseGradient@2x.png"))
        for nome in inject.METAL:  # o app real tem as quatro; o script só troca as que existem
            Image.new("RGBA", (40, 40), (255, 255, 255, 255)).save(os.path.join(app, nome))
        with open(os.path.join(app, "IGLaunchScreen.storyboardc", "MmX-nZ-UTa-view-5K5-hL-BWV.nib"), "wb") as f:
            f.write(b"nib logo-splash fb-branding")
        icons = {"CFBundlePrimaryIcon": {"CFBundleIconName": "AppIcon", "CFBundleIconFiles": ["AppIcon60x60"]}}
        with open(os.path.join(app, "Info.plist"), "wb") as f:
            inject.plistlib.dump({"CFBundleDisplayName": "Instagram", "CFBundleIcons": icons,
                                  "CFBundleIcons~ipad": icons}, f)

        self.out = os.path.join(self.tmp, "Instagram.ipa")
        inject.SRC, inject.DYLIB, inject.OUT = self.src, self.dylib, self.out

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def test_gera_ipa_verificado(self):
        inject.main()  # termina chamando verify(), que levanta SystemExit se algo faltar
        with zipfile.ZipFile(self.out) as zf:
            nomes = zf.namelist()
            self.assertEqual(nomes.count("Payload/Instagram.app/InstagramMod.dylib"), 1)
            with zf.open("Payload/Instagram.app/Instagram60x60@3x.png") as f:
                icone = Image.open(f).convert("RGB")
                r, g, b = icone.getpixel((90, 90))
                self.assertTrue(r == g == b, "ícone não ficou em preto e branco")

    def test_recusa_binario_ja_injetado(self):
        inject.main()
        binario = os.path.join(self.src, "Payload", "Instagram.app", "Instagram")
        with zipfile.ZipFile(self.out) as zf:
            injetado = zf.read("Payload/Instagram.app/Instagram")
        with open(binario, "wb") as f:
            f.write(injetado)
        with self.assertRaises(SystemExit) as erro:
            inject.main()
        self.assertIn("ja tem o InstagramMod.dylib", str(erro.exception))

    def test_dylib_rodado_duas_vezes(self):
        # rodar `python inject.py` de novo sem recompilar não pode estragar o dylib
        inject.fix_dylib(self.dylib)
        inject.fix_dylib(self.dylib)
        with open(self.dylib, "rb") as f:
            dados = f.read()
        self.assertTrue(inject.dylib_layout_ok(dados))
        self.assertTrue(inject.dylib_minos_ok(dados))


@unittest.skipUnless(shutil.which("zig"), "Zig não está no PATH")
class Frases(unittest.TestCase):
    def test_frases(self):
        exe = os.path.join(tempfile.mkdtemp(), "frases")
        subprocess.run(["zig", "cc", "-O1", "-w", "-o", exe, os.path.join(RAIZ, "tests", "test_frases.c")],
                       check=True, capture_output=True)
        r = subprocess.run([exe], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout)


if __name__ == "__main__":
    unittest.main()
