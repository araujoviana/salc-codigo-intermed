# SALc — Geração de Código Intermediário (Projeto 2)

**Matheus Gabriel Viana Araujo — 10420444**  
**Luis Fernando de Mesquita Pereira — 10410686**

Compilador da linguagem SAL (*Simple Academic Language*) que realiza análise léxica,
sintática e semântica completas e gera código intermediário em MEPA
(*Máquina de Execução de Pascal*) usando Tradução Dirigida à Sintaxe (SDT).

---

## Compilar

```bash
make          # compila o binário ./salc
make clean    # remove binário, objetos e arquivos de saída
```

Requer `gcc` com suporte a C99.

---

## Uso

```
salc <arquivo.sal> [--tokens] [--symtab] [--trace]
```

| Argumento | Descrição |
|-----------|-----------|
| `<arquivo.sal>` | Arquivo-fonte SAL a compilar (obrigatório) |
| `--tokens` | Gera `<base>.tk` com a lista de todos os tokens léxicos |
| `--symtab` | Gera `<base>.ts` com a tabela de símbolos após a compilação |
| `--trace`  | Gera `<base>.trc` com cada instrução MEPA emitida |

Em caso de **sucesso**, cria `<base>.mepa` com o código MEPA executável.  
Em caso de **erro**, imprime a mensagem no `stderr` e remove o `.mepa` incompleto.

### Exemplos

```bash
# Compilação simples
./salc tests/fibonacci.sal

# Com tabela de símbolos e rastreamento
./salc tests/match_test.sal --symtab --trace

# Todos os logs
./salc tests/seq_while.sal --tokens --symtab --trace
```

---

## Testes

```bash
make test          # roda 11 testes de sucesso (deve mostrar 11/11 OK)
make test-erros    # roda 5 testes de rejeição semântica (deve mostrar 5/5 OK)
make test-verbose  # exibe o MEPA gerado por cada teste de sucesso
```

### Testes de sucesso (`tests/`)

| Arquivo | Construções cobertas |
|---------|----------------------|
| `fibonacci.sal` | `for`, `locals`, múltiplas variáveis por linha |
| `seq_while.sal` | `loop while` |
| `if_else.sal` | `if`/`else` |
| `loop_until.sal` | `loop until` |
| `match_test.sal` | `match`/`when` com valores e listas |
| `aritmetica.sal` | `+`, `-`, `*`, `/`, menos unário |
| `logica.sal` | `~` (NOT), `^` (AND), `v` (OR) |
| `relacionais.sal` | `=`, `<>`, `<`, `<=`, `>`, `>=` |
| `for_passo.sal` | `for step +1` e `for step -1` |
| `match_range.sal` | `match`/`when` com intervalos (`..`) |
| `subrotinas.sal` | Análise semântica de `fn` e `proc` |

### Testes de rejeição (`tests/erros/`)

| Arquivo | Erro esperado |
|---------|---------------|
| `var_nao_declarada.sal` | Uso de variável não declarada |
| `duplicata.sal` | Identificador duplicado no mesmo escopo |
| `ret_fora_funcao.sal` | `ret` dentro de `proc` (não `fn`) |
| `args_errados.sal` | Chamada com número errado de argumentos |
| `tipo_incompativel.sal` | Atribuição de tipo incompatível (`bool` → `int`) |

---

## Análise Semântica

O compilador verifica e reporta os seguintes erros semânticos:

- **Declaração prévia**: variável usada sem ter sido declarada
- **Unicidade**: identificador declarado duas vezes no mesmo escopo
- **Controle de escopo**: variáveis locais não visíveis fora de seu escopo; sub-rotinas têm escopo próprio para parâmetros
- **Checagem de tipos**: compatibilidade em atribuições, expressões e retornos de função
- **Validação de parâmetros**: quantidade e tipos dos argumentos em chamadas a `proc`/`fn`
- **`ret` restrito a `fn`**: uso de `ret` dentro de `proc` é detectado

---

## Geração de Código MEPA

Instruções MEPA geradas por construção da linguagem:

| Construção SAL | Instruções MEPA |
|----------------|-----------------|
| Início / fim do programa | `INPP`, `PARA`, `FIM` |
| Alocação de variáveis | `AMEM N`, `DMEM N` |
| Leitura / escrita | `LEIT`, `IMPR` |
| Constante / variável | `CRCT`, `CRVL`, `ARMZ` |
| Aritmética | `SOMA`, `SUBT`, `MULT`, `DIVI` |
| Lógica | `NEGA`, `CONJ`, `DISJ` |
| Relacional | `CMIG`, `CMDIF`, `CMME`, `CMMEG`, `CMMA`, `CMMAG` |
| Desvios / rótulos | `DSVS`, `DSVF`, `NADA` |
| `if` / `if-else` | `DSVF`, `DSVS` + 2–4 rótulos |
| `loop while` | 4 rótulos (L_test, L_exit, L_body, L_back) |
| `loop until` | 4 rótulos |
| `for` (step ±1) | `CMMEG`/`CMMAG` + `SOMA`/`SUBT` + 4 rótulos |
| `match`/`when` | `ARMZ`/`CRVL` no temporário + cadeia de desvios |

> **Simplificação (conforme especificação):** sub-rotinas (`proc`/`fn` ≠ `main`) e
> vetores passam pela verificação semântica completa mas **não geram instruções MEPA**.

---

## Estrutura dos Módulos

```
main.c        Ponto de entrada; parsing de flags CLI; abertura de arquivos
analex.h      Analisador léxico (header-only, autômato finito)
asdr.c / .h   Parser ASDR + ações semânticas + geração SDT
tabsimb.c/.h  Tabela de símbolos (inserção, busca, controle de escopo, dump)
gerador.c/.h  Emissão formatada de instruções MEPA; gerenciamento de rótulos
```
