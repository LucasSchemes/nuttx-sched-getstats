# Chamada de Sistema `sched_getstats()` no Apache NuttX

Segundo Trabalho Prático da disciplina de **Sistemas Operacionais Embarcados (DEC7562)** — Curso de Engenharia de Computação, Universidade Federal de Santa Catarina (UFSC).  
**Professor:** Anderson Luiz Fernandes Perez  
**Autor:** Lucas Saft Schemes

---

## 1. Descrição da Funcionalidade

Este projeto implementa uma nova chamada de sistema no kernel do **Apache NuttX**, denominada `sched_getstats()`, voltada aos subsistemas de **escalonamento de tarefas** e **monitoramento de recursos do sistema operacional**.

A interface permite que aplicações consultem um *snapshot* consistente das estatísticas internas do escalonador em tempo de execução, retornando:

- Total de tarefas ativas no sistema (`total_tasks`);
- Distribuição por estado de execução (`running_tasks`, `ready_tasks`, `blocked_sem`, `blocked_sig` e `blocked_other`);
- Distribuição por tipo de tarefa (`kernel_threads`, `user_tasks` e `pthreads`);
- Capacidade atual da tabela hash de PIDs (`pidhash_size`);
- Contagem atual de ticks do sistema no instante da coleta (`sched_ticks`);
- Faixa de prioridades ativas (`min_priority` e `max_priority`).

### Interface pública (`<sys/sched_stats.h>`)

```c
#define SCHED_STATS_ALL        0x00u
#define SCHED_STATS_NO_IDLE    0x01u
#define SCHED_STATS_VALID_MASK (SCHED_STATS_NO_IDLE)

struct sched_stats_s
{
  uint32_t total_tasks;
  uint32_t running_tasks;
  uint32_t ready_tasks;
  uint32_t blocked_sem;
  uint32_t blocked_sig;
  uint32_t blocked_other;
  uint32_t kernel_threads;
  uint32_t user_tasks;
  uint32_t pthreads;
  uint32_t pidhash_size;
  uint32_t sched_ticks;
  uint8_t  min_priority;
  uint8_t  max_priority;
};

int sched_getstats(FAR struct sched_stats_s *stats, unsigned int flags);
```

- **`stats`**: ponteiro de saída que recebe a estrutura `struct sched_stats_s` preenchida pelo kernel.
- **`flags`**:
  - `SCHED_STATS_ALL` (`0x00u`): contabiliza todas as tarefas ativas, incluindo a tarefa `IDLE`.
  - `SCHED_STATS_NO_IDLE` (`0x01u`): exclui a tarefa `IDLE` (`sched_priority == 0`) da contagem.
- **Retorno e tratamento de erros**: retorna `0` (`OK`) em caso de sucesso; em caso de falha, retorna `-1` (`ERROR`) e define `errno = EINVAL` quando `stats == NULL` ou quando qualquer bit inválido fora de `SCHED_STATS_VALID_MASK` é informado em `flags`.

---

## 2. Arquivos do NuttX Adicionados e Modificados

Base utilizada: Apache NuttX (`master`, commit `3196e0e090` / NuttX-13.1.0), compilado para **ESP32-DevKitC** no modelo **FLAT**.

| Arquivo | Status | Descrição |
| :--- | :---: | :--- |
| `nuttx/include/sys/sched_stats.h` | **Adicionado** | Cabeçalho público contendo as flags, `struct sched_stats_s` e o protótipo de `sched_getstats()`. |
| `nuttx/sched/sched/sched_getstats.c` | **Adicionado** | Implementação no kernel (`nxsched_get_stats()`) e interface pública (`sched_getstats()`). |
| `nuttx/include/nuttx/sched.h` | **Modificado** | Declaração interna da função de kernel `nxsched_get_stats()`. |
| `nuttx/sched/sched/Make.defs` | **Modificado** | Inclusão de `sched_getstats.c` na variável `CSRCS` do build Make. |
| `nuttx/sched/sched/CMakeLists.txt` | **Modificado** | Inclusão de `sched_getstats.c` na lista `SRCS` do build CMake. |
| `nuttx/syscall/syscall.csv` | **Modificado** | Registro da chamada `sched_getstats` na tabela oficial de syscalls do NuttX. |
| `apps/examples/sched_stats_test/*` | **Adicionado** | Aplicação de teste para o NSH (`sched_stats_test`) cobrindo cenários normais e de erro. |
| `patches/nuttx-sched-getstats.patch` | **Adicionado** | Patch consolidado (`git diff`) contendo todas as alterações realizadas no código-fonte do NuttX. |

---

## 3. Estruturas e Funções Internas Utilizadas

A implementação segue a convenção arquitetural do Apache NuttX de separar a rotina interna do kernel (`nxsched_get_stats()`, que retorna códigos negativos de erro como `-EINVAL` sem alterar `errno`) do wrapper público (`sched_getstats()`, que traduz retornos negativos via `set_errno(-ret)` e retorna `ERROR`).

- **`g_pidhash` e `g_npidhash`** (`sched/sched/sched.h`): tabela hash dinâmica de ponteiros para os blocos de controle de tarefa (`FAR struct tcb_s **`) e sua capacidade atual. O loop percorre todas as posições válidas, descartando ponteiros `NULL` e tarefas com estado `TSTATE_TASK_INVALID`.
- **`struct tcb_s`** (`include/nuttx/sched.h`): *Task Control Block* de cada tarefa ativa, do qual são consultados:
  - `tcb->task_state`: estado atual no escalonador (`TSTATE_TASK_RUNNING`, `TSTATE_TASK_READYTORUN`, `TSTATE_TASK_PENDING`, `TSTATE_WAIT_SEM`, `TSTATE_WAIT_SIG` e demais estados bloqueados a partir de `FIRST_BLOCKED_STATE`);
  - `tcb->flags`: classificação do tipo de tarefa aplicando a máscara `TCB_FLAG_TTYPE_MASK` (`TCB_FLAG_TTYPE_KERNEL`, `TCB_FLAG_TTYPE_PTHREAD`, `TCB_FLAG_TTYPE_TASK`);
  - `tcb->sched_priority`: prioridade atual da tarefa, usada para calcular `min_priority`, `max_priority` e filtrar a tarefa `IDLE` (prioridade `0`).
- **`enter_critical_section()` / `leave_critical_section()`** (`include/nuttx/irq.h`): protege o acesso concorrente a `g_pidhash` e aos TCBs, impedindo preempção e modificações por rotinas de interrupção (ISRs) durante a coleta. Os valores são acumulados em uma estrutura local na pilha e copiados para `*stats` somente após sair da seção crítica, minimizando o tempo com interrupções desabilitadas.
- **`clock_systime_ticks()`** (`include/nuttx/clock.h`): obtém a contagem de ticks do sistema no mesmo instante atômico do *snapshot*.
- **`syscall/syscall.csv`**: integra `sched_getstats` à infraestrutura de chamadas de sistema do NuttX. No modelo `BUILD_FLAT`, kernel e aplicação compartilham o mesmo espaço de endereçamento e a invocação é direta; o registro garante compatibilidade com a geração de stubs/proxies nos modos `PROTECTED` e `KERNEL`.

---

## 4. Instruções de Compilação e Execução (ESP32-DevKitC — FLAT)

### Pré-requisitos
- Repositórios `nuttx/` e `apps/` clonados em `~/nuttxspace`;
- Toolchain `xtensa-esp32-elf-gcc` configurado no `PATH`;
- Ambiente Python (`nuttx-env`) com `esptool` e `kconfig-tweak`.

### 1. Aplicar as modificações no NuttX e copiar a aplicação de teste

```bash
cd ~/nuttxspace/nuttx
git apply ~/nuttx-sched-getstats/patches/nuttx-sched-getstats.patch

cp -r ~/nuttx-sched-getstats/apps/examples/sched_stats_test ~/nuttxspace/apps/examples/
```

### 2. Configurar para `esp32-devkitc:nsh` (FLAT) e compilar

```bash
source ~/nuttxspace/nuttx-env/bin/activate
export PATH="$HOME/toolchains/xtensa-esp-elf/bin:$PATH"

cd ~/nuttxspace/nuttx
make distclean
./tools/configure.sh -l esp32-devkitc:nsh
kconfig-tweak --enable CONFIG_EXAMPLES_SCHED_STATS_TEST
make olddefconfig
grep -E "CONFIG_BUILD_FLAT|CONFIG_EXAMPLES_SCHED_STATS_TEST" .config
make -j$(nproc)
```

### 3. Gravar no ESP32-DevKitC e executar via NSH

```bash
make flash ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BAUD=115200
picocom -b 115200 /dev/ttyACM0
```

*(Ajuste `/dev/ttyACM0` caso o dispositivo seja enumerado como `/dev/ttyUSB0`.)*

No prompt do NuttShell (`nsh>`), execute:

```text
nsh> sched_stats_test
```

---

## 5. Testes Realizados e Resultados Obtidos

A aplicação `sched_stats_test` executa quatro baterias de testes no ESP32-DevKitC:

1. **Teste 1 (`SCHED_STATS_ALL`)**: coleta o estado base do sistema, confirmando presença de tarefas ativas, pelo menos uma tarefa em execução (`running_tasks >= 1`) e a tarefa `IDLE` (`min_priority == 0`).
2. **Teste 2 (`SCHED_STATS_NO_IDLE`)**: verifica a filtragem da tarefa `IDLE`, validando a redução em `total_tasks`, `kernel_threads == 0` e `min_priority > 0`.
3. **Teste 3 (Criação dinâmica de `pthreads`)**: cria duas `pthreads` — uma bloqueada em semáforo (`sem_wait`) e outra em espera temporizada (`usleep`) — confirmando o aumento exato em `total_tasks`, `pthreads`, `blocked_sem` e `blocked_sig`, além do retorno ao estado inicial após a finalização via `pthread_join()`.
4. **Teste 4 (Validação de erros)**: confirma retorno `-1` e `errno == EINVAL (22)` para ponteiro `NULL`, flag inválida (`0x02`) e combinação inválida (`NULL` + `0xFF`).

### Saída real obtida no ESP32-DevKitC (NuttX-13.1.0)

```text
NuttShell (NSH) NuttX-13.1.0
nsh> sched_stats_test
Iniciando testes da syscall sched_getstats()...

=== Teste 1: SCHED_STATS_ALL (baseline) ===
  total_tasks    : 3
  running_tasks  : 1
  ready_tasks    : 1
  blocked_sem    : 1
  blocked_sig    : 0
  blocked_other  : 0
  kernel_threads : 1
  user_tasks     : 2
  pthreads       : 0
  pidhash_size   : 16
  sched_ticks    : 1315
  priority range : [0 .. 100]
[OK] Teste 1 passou.

=== Teste 2: SCHED_STATS_NO_IDLE ===
  total_tasks    : 2
  running_tasks  : 1
  ready_tasks    : 0
  blocked_sem    : 1
  blocked_sig    : 0
  blocked_other  : 0
  kernel_threads : 0
  user_tasks     : 2
  pthreads       : 0
  pidhash_size   : 16
  sched_ticks    : 1315
  priority range : [100 .. 100]
[OK] Teste 2 passou (IDLE filtrada).

=== Teste 3: Com 2 pthreads bloqueadas ===
  total_tasks    : 5
  running_tasks  : 1
  ready_tasks    : 1
  blocked_sem    : 2
  blocked_sig    : 1
  blocked_other  : 0
  kernel_threads : 1
  user_tasks     : 2
  pthreads       : 2
  pidhash_size   : 16
  sched_ticks    : 1339
  priority range : [0 .. 100]
[OK] Teste 3 passou (incremento de pthreads e bloqueios confirmado).

=== Pos-Teste 3: Apos encerrar pthreads ===
  total_tasks    : 3
  running_tasks  : 1
  ready_tasks    : 1
  blocked_sem    : 1
  blocked_sig    : 0
  blocked_other  : 0
  kernel_threads : 1
  user_tasks     : 2
  pthreads       : 0
  pidhash_size   : 16
  sched_ticks    : 1470
  priority range : [0 .. 100]

=== Teste 4: Validacao de erros ===
[OK] Teste 4a (stats == NULL): ret=-1, errno=EINVAL (22)
[OK] Teste 4b (flag invalida 0x02): ret=-1, errno=EINVAL (22)
[OK] Teste 4c (NULL + 0xFF): ret=-1, errno=EINVAL (22)

========================================
Resumo: TODOS OS TESTES PASSARAM (0 falha(s))
========================================
```
