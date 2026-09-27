# Por que este mod não usa UE4SS

Decisão tomada depois de o build falhar no CI. O impedimento não era de
toolchain, era de permissão.

## O que foi verificado

| Verificação | Resultado |
|---|---|
| Mod C++ compila standalone contra um SDK do UE4SS? | Não. A release `zDEV-UE4SS_v3.0.1.zip` tem 166 arquivos e **nenhum** header ou `.lib` |
| Fluxo oficial | `add_subdirectory(RE-UE4SS)` — o mod compila junto com o UE4SS inteiro |
| RE-UE4SS compila sozinho? | Não. Exige o submódulo `Re-UE4SS/UEPseudo` |
| `Re-UE4SS/UEPseudo` é público? | **Não.** A API do GitHub devolve `Not Found`; o CI do próprio UE4SS usa `token: ${{ secrets.UEPSEUDO_PAT }}` |
| Versão antiga resolve? | Não. `v2.5.2` depende do mesmo repositório privado |
| Existe espelho público confiável? | Não. Um repositório vazio (0 KB) e um fork para outro jogo |
| A API Lua do UE4SS tem ImGui? | Não. Só keybind, hook e comando de console |

Ou seja: ninguém sem acesso ao UEPseudo compila um mod C++ de UE4SS — nem este
repositório, nem o CI, nem você na sua máquina.

## O que foi feito no lugar

Stack pública inteira, sem nenhuma dependência privada:

| Peça | Projeto | Licença | Papel |
|---|---|---|---|
| SDK do jogo | [Dumper-7](https://github.com/Encryqed/Dumper-7) | permissiva | gera os headers C++ das classes do Unreal para **esta** versão do jogo — o papel que o UEPseudo teria |
| Hook | [MinHook](https://github.com/TsudaKageyu/minhook) | BSD-2 | engancha as vtables de D3D |
| Interface | [Dear ImGui](https://github.com/ocornut/imgui) | MIT | desenha o overlay |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) | MIT | config e dados |

A DLL entra no processo por proxy (`dinput8.dll` ao lado do executável — o
UE4SS usa `dwmapi.dll`, então os dois convivem) ou por injetor, engancha o
`Present` da swapchain e desenha no quadro do jogo.

**O que se perde:** a API de reflexão do UE4SS, que resolvia sozinha achar
classes e propriedades do Unreal. No lugar dela vem o SDK do Dumper-7, gerado
uma vez por versão do jogo. É mais trabalho no M0, que já era o milestone de
reconhecimento de qualquer forma.

**O que se ganha:** nenhuma dependência privada, CI compila e publica o zip, e
o mod não depende do UE4SS estar instalado.
