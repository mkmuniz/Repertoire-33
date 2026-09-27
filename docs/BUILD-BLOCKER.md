# Por que a DLL ainda não é distribuída

Estado: **bloqueado por uma dependência privada do UE4SS**, não por código deste
repositório.

## O que foi verificado

| Verificação | Resultado |
|---|---|
| Mod C++ compila standalone contra um SDK? | Não. A release `zDEV-UE4SS_v3.0.1.zip` tem 166 arquivos e **nenhum** header ou `.lib` |
| Fluxo oficial | `add_subdirectory(RE-UE4SS)` — o mod compila junto com o UE4SS inteiro |
| RE-UE4SS compila sozinho? | Não. Exige o submódulo `Re-UE4SS/UEPseudo` |
| `Re-UE4SS/UEPseudo` é público? | **Não.** A API do GitHub devolve `Not Found`; o CI do próprio UE4SS usa `token: ${{ secrets.UEPSEUDO_PAT }}` |
| Versão antiga resolve? | Não. `v2.5.2` depende do mesmo repositório privado |
| Existe espelho público confiável? | Não. Duas buscas: um repo vazio (0 KB) e um fork para outro jogo |
| A API Lua do UE4SS tem ImGui? | Não. Só keybind, hook, comando de console |

## O que isso significa

Qualquer pessoa sem acesso ao UEPseudo — incluindo o CI deste repositório — não
consegue compilar um mod C++ de UE4SS. O impedimento não é de toolchain: é de
permissão.

## Caminhos

1. **Pedir acesso ao UEPseudo** à equipe do UE4SS (Discord do projeto). Com o
   acesso, gerar um PAT de leitura e guardá-lo como o secret `UEPSEUDO_PAT`; o
   workflow `build-mod.yml` passa a compilar e publicar o zip. É o caminho que
   preserva o overlay in-game como está construído.

2. **Mod em Lua.** Não precisa compilar e instala na hora, mas a API Lua não
   expõe ImGui: a configuração viria de arquivo e comandos de console, sem
   overlay. Boa parte da lógica deste repositório teria de ser reescrita.

3. **Lua + janela externa.** O mod Lua lê o jogo e publica o estado; um programa
   separado desenha a interface. Mantém a interface, perde o "um artefato só"
   e o overlay sobre o jogo em tela cheia.

Até a decisão, o CI cobre o que de fato funciona: a camada de lógica, que roda
em qualquer sistema sem o jogo.
