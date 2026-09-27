# Onde cada parte do trabalho roda

O jogo e o UE4SS só existem no PC Windows. O MacBook não tem o jogo. Isso divide
o trabalho em duas metades bem definidas — e a maior parte do código pode nascer
no Mac.

## Roda no MacBook

| Tarefa | Observação |
|---|---|
| Escrever C++, headers, arquitetura | Sem compilar |
| `docs/`, README, plano, roadmap | — |
| `tools/extract.ts` (Node) | Precisa da pasta de exports do FModel copiada do PC |
| Fixtures de dano (JSON) | Os números vêm do PC; o formato e os testes não |
| Target `calc_tests` do xmake | `Calc/` não toca Unreal nem ImGui, compila nativo |
| CI (GitHub Actions) | O runner `windows-latest` compila o que o Mac não compila |

## Só roda no PC Windows

| Tarefa | Por quê |
|---|---|
| Compilar a DLL do mod | MSVC + headers/libs do UE4SS, target Windows x64 |
| Protótipo Lua do M0 | Precisa do jogo rodando |
| Qualquer hook, leitura de memória, ImGui | Precisa do processo do jogo |
| FModel (dump dos dados) | App .NET/WPF, Windows-only |
| Toda validação e medição de dano | Precisa jogar |
| Wwise (M6 do Boss Music) | Windows-only na prática |

## Fluxo prático

1. Escreva no Mac, commite, `git push`.
2. No PC: `git pull`, `xmake f --ue4ss=...`, `xmake`, copiar a DLL para
   `ue4ss\Mods\<Mod>\dlls\main.dll`, testar.
3. Achados do jogo (ids de encontro, nomes de classe, offsets) voltam como
   commit de `data/*.json` ou de comentário no código.
4. O CI em `windows-latest` pega erro de compilação sem você ir ao PC.

**Consequência para o cronograma:** os milestones M0 dos dois projetos são
100% no PC. Não há como adiantá-los daqui. O que dá para adiantar no Mac é
M2 do otimizador (extrator), a estrutura de `Calc/`, o CI e os READMEs.
