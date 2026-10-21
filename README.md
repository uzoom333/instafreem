# instafreem

Tweak do Instagram 452.0.1 para iPhone. No Windows ele é um dylib em C, compilado com Zig e empacotado num IPA. Não usa Theos.

A versão que está neste repositório é a build definitiva de 20/10/2026, 15:08. O arquivo que se edita é `src/instagram_mod.c`.

## O que esta build faz

- Esconde o botão de Reels da barra inferior e redistribui os outros botões, sem esticar a altura deles.
- Esconde a bandeja de Stories, anéis, destaques e stories no perfil e nas DMs.
- Esconde Instants / QuickSnap. Instant Reply continua.
- Mantém a home, as DMs, a lupa e o perfil.
- Um reel recebido por DM abre, com a rolagem vertical travada.
- Ícones de curtir, comentar e repostar ficam. Some só o número.
- Publicações patrocinadas e itens de vídeo/clips no feed ficam com altura zero. Fotos normais continuam.
- O nome na tela inicial é Instagram. O ícone é o do Instagram em preto e branco.
- Na splash, o texto é "modded by Aeromorto".

## O que não entra no Git

O IPA do Instagram e a pasta descriptografada não estão aqui. Cada pessoa usa a própria cópia local, já descriptografada, da versão 452.0.1 (no disco o arquivo se chama 452.0.0). O binário precisa estar com `cryptid` 0. Um IPA que ainda abre a splash e volta para a tela inicial costuma estar criptografado, ou recebeu o dylib duas vezes.

O IPA gerado, `packages/Instagram.ipa`, também fica de fora do Git. Ele contém o aplicativo inteiro.

## 1. Ferramentas

No Windows:

1. Python 3.12.
2. Zig 0.17.0, com `zig` no PATH. O alvo usado é `aarch64-ios.16.3`.
3. Sideloadly, para assinar e instalar. A assinatura é feita na janela do Sideloadly.
4. Pillow:

```powershell
pip install -r requirements.txt
```

## 2. IPA de base

1. Consiga o IPA descriptografado do Instagram 452.0.1.
2. Extraia. A pasta precisa conter `Payload\Instagram.app\Instagram`.
3. Aponte o empacotador para essa pasta. O padrão desta máquina é `C:\Users\aerom\Downloads\com.burbn.instagram-452.0.0-Decrypted`. Em outra máquina, defina a variável antes de empacotar:

```powershell
$env:INSTAGRAM_DECRYPTED = "D:\caminho\com.burbn.instagram-452.0.0-Decrypted"
```

Não edite os arquivos dentro dessa pasta. O script só lê de lá e grava um IPA novo em `packages\`.

## 3. Compilar e empacotar

No PowerShell, a partir da raiz do repositório. O PowerShell 5.1 não aceita `&&`. Cada `-Wl` vai entre aspas.

```powershell
zig cc -target aarch64-ios.16.3 -dynamiclib -fvisibility=default -O2 "-Wl,-undefined,dynamic_lookup" "-Wl,-install_name,@executable_path/InstagramMod.dylib" -o packages\InstagramMod.dylib src\instagram_mod.c
python inject.py
```

Se os dois terminarem sem erro, o IPA está em `packages\Instagram.ipa`. A última linha do script é `verified load command, icon, splash and display name`.

`inject.py` reescreve os binds do dylib para `libobjc`, alinha `__TEXT` para a assinatura, injeta `@executable_path/InstagramMod.dylib`, troca o ícone para preto e branco e mantém o nome Instagram.

## 4. Instalar no iPhone

1. Apague o Instagram sideloadado que já estiver no aparelho.
2. Abra o `packages\Instagram.ipa` no Sideloadly.
3. Deixe **Inject dylibs/frameworks** desmarcado. O dylib já está dentro do IPA. Marcar isso carrega o tweak duas vezes e o app fecha na splash.
4. Deixe **Use automatic bundle ID** marcado.
5. Assine pela janela do Sideloadly.
6. No iPhone: Ajustes, Geral, VPN e Gerenciamento de Dispositivo, e confie no certificado.
7. Abra o app e entre na conta uma vez.

Para trocar o tweak, repita o passo 3 e instale de novo por cima, sempre apagando o app anterior.

Atalho com pyzule, se preferir, parte do IPA descriptografado original, não do IPA já modificado:

```text
pyzule -i com.burbn.instagram-452.0.0-Decrypted.ipa -o Instagram.ipa -j InstagramMod.dylib
```

## 5. Onde mexer

Edite só `src/instagram_mod.c`. Depois rode de novo os dois comandos do passo 3.

Pontos que já quebraram o app quando foram mudados:

- Não bana a aba DIRECT, FEED, EXPLORE, SEARCH ou PROFILE. Só CLIPS e REELS saem da barra.
- Não filtre `allTabBarSurfaces` e não troque `setSelectedTabBarSurface`. Isso desencontrava os índices e a home deixava de abrir.
- Não troque `layoutSubviews` da `UIView`, nem o `layoutSubviews` do container da barra. O reflow atual só mexe na largura dos botões e preserva a altura.
- Não percorra a lista de classes com `objc_copyClassList`.
- Não devolva altura zero para todos os itens do feed. Isso embolou a home. Altura zero fica para patrocinado, vídeo/clips e para as section controllers já listadas no `IFInstall`.
- Não esconda `IGUFIButtonWithCountsView`. O número some em `setCountString:showButton:`. O ícone fica.
- Não esconda views de vídeo com `setHidden` ou alfa 0. Isso deixava um quadro preto. O colapso é pela altura da section.
- Não zere `numberOfItems` da bandeja de Stories.
- Não injete o dylib uma segunda vez no Sideloadly.

O construtor cedo só prepara a pasta falsa de app group. O resto dos hooks entra depois, na fila principal. Strings passadas ao `NSString` passam por `IFStr()`.

## 6. Testes (sem iPhone)

Testam o empacotador num `Instagram.app` falso e as regras que apagam curtidas, sem o IPA de verdade. Precisam de Zig no PATH e de Pillow:

```text
python -m unittest discover -s tests -v
```

Os testes linkam contra os stubs `.tbd` de `tests/stubs`, porque fora do Windows o `-undefined dynamic_lookup` derruba o linker do Zig 0.17. O dylib que eles geram é só para teste. O do iPhone continua saindo do passo 3.
