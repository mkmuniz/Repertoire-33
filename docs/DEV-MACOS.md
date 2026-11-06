# Onde cada parte do trabalho roda

O jogo só existe no PC Windows. O MacBook não tem o jogo. Isso divide
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
| Compilar a DLL do mod | MSVC, target Windows x64 (o CI faz isso a cada push) |
| Qualquer hook, leitura de memória, ImGui | Precisa do processo do jogo |
| FModel (dump dos dados) | App .NET/WPF, Windows-only |
| Toda validação e medição de dano | Precisa jogar |

## Fluxo prático

1. Escreva no Mac, commite, `git push`.
2. No PC: baixar o `Repertoire33.zip` do CI (ou `cmake --build build --target
   package`), extrair em `Sandfall\Binaries\Win64\` e testar seguindo
   `docs/TESTING.md`.
3. Achados do jogo (ids de encontro, nomes de classe, offsets) voltam como
   commit de `data/*.json` ou de comentário no código.
4. O CI em `windows-latest` pega erro de compilação sem você ir ao PC.

**Como o M0 e o M2 do Boss Music nasceram no Mac:** os nomes de classe e de
campo vêm dos SDKs públicos gerados do jogo (Dumper-7), e os testes montam o
layout do UE 5.4 num buffer para exercitar a leitura e a escrita sem o jogo. O
que só o PC responde é se o jogo lê a música de onde o SDK diz — e o log foi
feito para responder isso na primeira sessão.
