**Concourse Pipeline** [![Concourse Build Status](https://prod.ci.gpdb.pivotal.io/api/v1/teams/main/pipelines/gpdb_main/badge)](https://prod.ci.gpdb.pivotal.io/teams/main/pipelines/gpdb_main) |
**Travis Build** [![Travis Build Status](https://travis-ci.org/greenplum-db/gpdb.svg?branch=main)](https://travis-ci.org/greenplum-db/gpdb) |

----------------------------------------------------------------------

![Greenplum](logo-greenplum.png)

O Greenplum Database (GPDB) é um data warehouse de código aberto avançado e completo, baseado em PostgreSQL. Ele fornece análises poderosas e rápidas sobre
volumes de dados em escala de petabytes. Voltado exclusivamente para big data
analytics, o Greenplum Database é alimentado pelo otimizador de consulta baseado em custo mais avançado do mundo, oferecendo alto desempenho de consulta analítica em grandes volumes de dados.

O projeto Greenplum é lançado sob o [Apache 2
licença](http://www.apache.org/licenses/LICENSE-2.0). queremos agradecer
todos os nossos contribuidores antigos e atuais da comunidade e estamos realmente interessados ​​em
todas as novas contribuições potenciais. Para a comunidade de banco de dados Greenplum
nenhuma contribuição é muito pequena, encorajamos todos os tipos de contribuições.

## Visão geral

Um cluster Greenplum consiste em um servidor __coordinator__ e vários servidores __segment__. Todos os dados do usuário residem nos segmentos, o coordenador
contém apenas metadados. O servidor coordenador e todos os segmentos compartilham
o mesmo esquema.

Os usuários sempre se conectam ao servidor coordenador, que divide a consulta
em fragmentos que são executados nos segmentos e coleta os resultados.

Mais informações podem ser encontradas no [site do projeto](https://greenplum.org/).

## Construindo banco de dados Greenplum com GPORCA
GPORCA é um otimizador baseado em custo que é usado pelo banco de dados Greenplum em
conjunto com o planejador PostgreSQL. Também é conhecido apenas como ORCA, e
Pivotal Optimizer. O código para GPORCA reside em src/backend/gporca. é construído
automaticamente por padrão.

### Instalando dependências (para desenvolvedores macOS)
Siga [estas etapas do macOS](README.macOS.md) para preparar seu sistema para o GPDB

### Instalando dependências (para desenvolvedores Linux)
Siga [etapas linux apropriadas](README.Linux.md) para preparar seu sistema para GPDB

### Construir o banco de dados

```
# Initialize and update submodules in the repository
git submodule update --init

# Configure build environment to install at /usr/local/gpdb
./configure --with-perl --with-python --with-libxml --with-gssapi --prefix=/usr/local/gpdb

# Compile and install
make -j8
make -j8 install

# Bring in greenplum environment into your running shell
source /usr/local/gpdb/greenplum_path.sh

# Start demo cluster
make create-demo-cluster
# (gpdemo-env.sh contains __PGPORT__ and __MASTER_DATA_DIRECTORY__ values)
source gpAux/gpdemo/gpdemo-env.sh
```

O diretório, as portas TCP, o número de segmentos e a existência de esperas para segmentos 
e coordenador para o cluster de demonstração podem ser alterados em tempo real.
Ao invés de `make create-demo-cluster`, considere:

```
DATADIRS=/tmp/gpdb-cluster PORT_BASE=5555 NUM_PRIMARY_MIRROR_PAIRS=1 WITH_MIRRORS=false make create-demo-cluster
```

The TCP port for the regression test can be changed on the fly:

```
PGPORT=5555 make installcheck-world
```

Para desativar o GPORCA e usar o planejador Postgres para otimização de consultas:
```
set optimizer=off;
```

Se você quiser apagar todos os arquivors gerados:
```
make distclean
```

## Executando testes

* Os testes de regressão padrão

```
make installcheck-world
```

* O destino de nível superior __installcheck-world__ executará todas as regressões
  testes no GPDB no cluster em execução. Para testar peças individuais, os respectivos 
  alvos podem ser executados separadamente.

* O destino __check__ do PostgreSQL não funciona. Configurar um cluster Greenplum é mais complicado 
  do que uma instalação PostgreSQL de nó único, e ninguém fez o trabalho de __make check__ para criar um cluster. 
  Crie um cluster manualmente ou use gpAux/gpdemo/(exemplo abaixo) e execute o __make installcheck-world__ de 
  nível superior contra ele. Patches são bem-vindos!

* O destino __installcheck__ do PostgreSQL também não funciona, porque alguns
  testes falham com o Greenplum. A programação __installcheck-good__ em __src/test/regress__ exclui esses testes.

* Ao adicionar um novo teste, adicione-o a um dos testes específicos do GPDB,
  em greenplum_schedule, em vez dos testes PostgreSQL herdados do
  upstream. Tentamos manter os testes upstream idênticos às versões 
  upstream, para facilitar a fusão com versões mais recentes do PostgreSQL.

## Configurações Alternativas

### Construindo GPDB sem GPORCA

Atualmente, o GPDB é construído com o GPORCA por padrão. Se você deseja construir o GPDB
sem GPORCA, a configuração requer o sinalizador `--disable-orca` para ser definido.
```
# Clean environment
make distclean

# Configure build environment to install at /usr/local/gpdb
./configure --disable-orca --with-perl --with-python --with-libxml --prefix=/usr/local/gpdb
```

### Construindo GPDB com Python3 ativado

GPDB suporta Python3 com plpython3u UDF

Consulte [como ativar o Python3](src/pl/plpython/README.md) para obter detalhes.


### Construindo ferramentas de cliente GPDB no Windows

Consulte [Criando ferramentas do cliente GPDB no Windows](README.Windows.md) para obter detalhes.

## Desenvolvimento com Vagrant

Existe um [guia de início rápido para desenvolvedores](src/tools/vagrant/README.md) baseado no Vagrant.

## Layout do código

O layout do diretório do repositório segue o mesmo layout geral
como PostgreSQL upstream. Há mudanças em comparação com o PostgreSQL em toda a base de 
código, mas algumas adições maiores dignas de nota:

* __gpMgmt/__

  Contém ferramentas de linha de comando específicas do Greenplum para gerenciar o cluster. 
  Scripts como gpinit, gpstart, gpstop vivem aqui. Eles são escritos principalmente em Python.

* __gpAux/__

  Contém scripts de gerenciamento de versão específicos do Greenplum e dependências de fornecedores. 
  Alguns diretórios adicionais são submódulos e serão disponibilizados ao longo do tempo.

* __gpcontrib/__

  Assim como o diretório contrib/ do PostgreSQL, este diretório contém
  extensões como gpfdist e gpmapreduce que são específicas do Greenplum.

* __doc/__

  No PostgreSQL, o manual do usuário reside aqui. No Greenplum, o manual do usuário é mantido 
  separadamente e apenas as páginas de referência usadas para construir páginas de manual estão aqui.

* __gpdb-doc/__

  Contém a documentação do Greenplum no formato DITA XML. Consulte `gpdb-doc/README.md` para obter informações 
  sobre como construir e trabalhar com a documentação.

* __ci/__

  Contém arquivos de configuração para o sistema de integração contínua GPDB.

* __src/back-end/cdb/__

  Contém módulos de back-end específicos do Greenplum maiores. Por exemplo,
  comunicação entre segmentos, transformando planos em planos paralelizáveis, espelhamento, 
  transação distribuída e gerenciamento de instantâneos,
  etc. __cdb__ significa __Cluster Database__ - que era um nome de trabalho usado anteriomente. 
  Esse nome não é mais usado, mas o prefixo __cdb__ permanece.

* __src/back-end/gpopt/__

  Contém a chamada biblioteca __translator__, para usar o otimizador GPORCA com Greenplum. 
  A biblioteca do tradutor é escrita em código C++ e contém código cola para traduzir planos e consultas
  entre o formato DXL usado pelo GPORCA e a representação interna do PostgreSQL.

* __src/backend/gporca/__

  Contém o código e os testes do otimizador GPORCA. Isso é escrito em C++. Ver
  [README.md](src/backend/gporca/README.md) para obter mais informações e como testar a unidade do GPORCA.

* __src/backend/fts/__

  FTS é um processo que roda no nó coordenador, e periodicamente
  pesquisa os segmentos para manter o status de cada segmento.

## Contribuindo

O Greenplum é mantido por uma equipe principal de desenvolvedores com direitos de compromisso para o
[principal repositório gpdb](https://github.com/greenplum-db/gpdb) no GitHub. Ao mesmo tempo, estamos 
ansiosos para receber contribuições de qualquer pessoa da comunidade Greenplum mais ampla. Esta seção 
abrange tudo o que você precisa saber se deseja que suas alterações de código ou documentação sejam 
adicionadas ao Greenplum e apareçam em versões futuras.

### Começando

Greenplum é desenvolvido no GitHub, e qualquer pessoa que deseje contribuir para isso
tem que [ter uma conta GitHub](https://github.com/signup/free) e estar familiarizado
com [ferramentas Git e fluxo de trabalho](https://wiki.postgresql.org/wiki/Working_with_Git).
Também é recomendável que você siga a [lista de discussão do desenvolvedor](https://greenplum.org/community/)
já que algumas das contribuições podem gerar discussões mais detalhadas lá.

Depois de ter sua conta GitHub, [fork](https://github.com/greenplum-db/gpdb/fork) este repositório 
para que você possa ter sua cópia privada para começar a hackear e usar como fonte de pull request.

Qualquer pessoa que contribua para o Greenplum deve estar coberta pelo Contrato de Licença de Colaborador 
Corporativo ou Individual. Se ainda não o fez, preencha e envie o [Contrato de licença do colaborador](https://cla.pivotal.io/sign/greenplum).
Observe que permitimos que mudanças realmente triviais sejam contribuídas sem um
CLA se eles se enquadrarem na rubrica de [correções óbvias](https://cla.pivotal.io/about#obvious-fixes).
No entanto, como nosso fluxo de trabalho do GitHub verifica o CLA por padrão, você pode achar mais fácil enviar 
um em vez de reivindicar uma exceção de "correção óbvia".

### Licenciamento de contribuições Greenplum

Se a contribuição que você está enviando for um trabalho original, você pode assumir que a Pivotal
irá lançá-lo como parte de um lançamento geral do Greenplum disponível para os consumidores downstream sob a licença Apache, versão 2.0. 
No entanto, além disso, Pivotal também pode decidir lançá-lo sob uma licença diferente (como [PostgreSQL License](https://www.postgresql.org/about/licence/) 
para os consumidores upstream que o exigem. Um exemplo típico aqui seria upstreaming Pivotal de seu contribuição de volta à comunidade PostgreSQL 
(o que pode ser feito textualmente ou sua contribuição sendo upstreamed como parte do changeset maior).

Se a contribuição que você está enviando NÃO é um trabalho original, você deve indicar o nome
da licença e também certifique-se de que ela é semelhante em termos à Licença Apache 2.0.
A Apache Software Foundation mantém uma lista dessas licenças na [Categoria A](https://www.apache.org/legal/resolved.html#category-a). 
Além disso, você pode ser obrigado a fazer a devida atribuição no [arquivo NOTICE](https://github.com/greenplum-db/gpdb/blob/main/NOTICE) 
semelhante a [estes exemplos](https://github.com/greenplum-db/gpdb/blob/main/ AVISO#L278).

Por fim, lembre-se de que NUNCA é uma boa ideia remover os cabeçalhos de licenciamento do trabalho que não é originalmente seu.
Mesmo se você estiver usando partes do arquivo que originalmente tinham um cabeçalho de licenciamento na parte superior, você deve preservá-lo.
Como sempre, se você não tiver certeza sobre as implicações de licenciamento de suas contribuições,
sinta-se à vontade para entrar em contato conosco na lista de discussão do desenvolvedor.

### Diretrizes de codificação

Suas chances de obter feedback e ver seu código integrado ao projeto
dependem muito de quão granulares são suas alterações. Se por acaso você tiver um maior
mudança em mente, é altamente recomendável entrar na lista de discussão do desenvolvedor
primeiro e compartilhando sua proposta conosco antes de gastar muito tempo escrevendo
código. Mesmo quando sua proposta for validada pela comunidade, ainda recomendamos
fazendo o trabalho real como uma série de pequenos commits independentes. Isto faz
o trabalho do revisor fica muito mais fácil e aumenta a pontualidade do feedback.

Quando se trata de partes C e C++ do Greenplum, tentamos seguir
[Convenções de codificação do PostgreSQL](https://www.postgresql.org/docs/devel/source.html).
Além disso, exigimos que:
   * Todo o código Python passa [Pylint](https://www.pylint.org/)
   * Todo o código Go é formatado de acordo com [gofmt](https://golang.org/cmd/gofmt/)

Recomendamos usar ```git diff --color``` ao revisar suas alterações para que você não tenha nenhum 
problema de espaço em branco espúrio no código que você enviar.

Todas as novas funcionalidades que são contribuídas para o Greenplum devem ser cobertas por testes de regressão 
que são contribuídos junto com ele. Se você não tiver certeza sobre como testar ou documentar
seu trabalho, levante a questão na lista de discussão gpdb-dev e a comunidade de desenvolvedores fará o possível para ajudá-lo.

No mínimo, você deve estar sempre executando
```make installcheck-world```
para ter certeza de que você não está quebrando nada.

### Alterações aplicáveis ​​ao PostgreSQL upstream

Se a alteração na qual você está trabalhando toca na funcionalidade comum entre PostgreSQL
e Greenplum, você pode ser solicitado a encaminhá-lo para o PostgreSQL. Isso não só para continuarmos reduzindo o delta entre os dois 
projetos, mas também para que qualquer mudança que é relevante para o PostgreSQL pode se beneficiar de uma revisão muito mais ampla da 
comunidade PostgreSQL upstream. Em geral, é uma boa ideia manter ambas as bases de código à mão para
você pode ter certeza se suas alterações precisam ser transferidas.

### Tempo de envio

Para melhorar as chances de uma discussão correta sobre seu patch ou ideia acontecer, preste atenção em qual é o ciclo de trabalho da 
comunidade. Por exemplo, se você enviar uma ideia totalmente nova na fase beta de um lançamento, podemos adiar a revisão ou direcionar sua 
inclusão para uma versão posterior. Sinta-se à vontade para perguntar na lista de discussão para saber mais sobre a política e o 
cronograma de lançamento do Greenplum.

### Envio de correção

Quando estiver pronto para compartilhar seu trabalho com a equipe principal do Greenplum e o resto da comunidade 
Greenplum, você deve enviar todos os commits para um branch em seu próprio
repositório bifurcado do Greenplum oficial e
[envie-nos um pull request](https://help.github.com/articles/about-pull-requests/).

Aceitamos envios que estão em andamento para obter feedback no início do processo de desenvolvimento. 
Ao abrir um pull request, selecione "Rascunho" no menu suspenso ao criar o PR para marcar claramente a 
intenção da pull request. Prefixar o título com "WIP:" também é uma boa prática.

Todos os novos recursos devem ser enviados contra o ramo principal. As correções de bugs também 
devem ser enviadas contra o principal, a menos que existam apenas em um
back-branch. Se o bug existir nos ramos principais e secundários, explique isso
na descrição PR.

### Verificações de validação e CI

Depois de enviar seu pull request, você verá imediatamente uma série de verificações de validação
realizadas por nossos pipelines de CI automatizados. Haverá também um cheque CLA
informando se o seu CLA foi reconhecido. Se alguma dessas verificações falhar, você
precisará atualizar sua pull request para cuidar do problema. Pull request com verificações de 
validação com falha são muito improváveis ​​de receber qualquer outro ponto
avaliação dos membros da comunidade.

Lembre-se de que o motivo mais comum para uma falha na verificação do CLA é uma incompatibilidade
entre um e-mail em arquivo e um e-mail registrado nos commits enviados como
parte da pull request.

Se você não conseguir descobrir por que uma determinada verificação de validação falhou, sinta-se à 
vontade para perguntar na lista de discussão do desenvolvedor, mas certifique-se de incluir um link direto 
para um pull request em seu e-mail.

### Revisão do patch

Presume-se que um pull request enviada com aprovação nas verificações de validação esteja disponível 
para revisão por pares. A revisão por pares é o processo que garante que as contribuições para o Greenplum
são de alta qualidade e se alinham bem com o roteiro e as expectativas da comunidade. Todo
membro da comunidade Greenplum é encorajado a revisar pull requests e fornecer
retorno. Como você não precisa ser um membro da equipe principal para poder fazer isso, nós
recomendamos seguir um fluxo de pull reviews para qualquer um que esteja interessado em se tornar
um colaborador de longo prazo da Greenplum. Como [Linus diria](https://en.wikipedia.org/wiki/Linus's_Law)
"com olhos suficientes, todos os bugs são superficiais".

Um resultado da revisão por pares pode ser um consenso de que você precisa modificar seu
pull request de certas maneiras. O GitHub permite que você envie confirmações adicionais para
uma ramificação da qual um pull request foi enviada. Esses commits adicionais ficarão visíveis para todos os revisores.

Uma revisão por pares converge quando recebe pelo menos um +1 e nenhum -1s dos participantes. Nesse ponto, você 
deve esperar que um dos membros principais da equipe traga suas alterações para o projeto.

A Greenplum se orgulha de ser um ambiente colaborativo e orientado para o consenso.
Não acreditamos em vetos e em qualquer voto -1 lançado como parte da revisão por pares
tem que ter uma explicação técnica detalhada do que há de errado com a mudança.
Caso surja um forte desentendimento, pode ser aconselhável levar o assunto para a lista de discussão, pois 
permite um fluxo mais natural da conversa.

A qualquer momento durante a revisão do patch, você pode sofrer atrasos com base na disponibilidade de revisores 
e membros da equipe principal. Por favor, seja paciente. Dito isto, também não desanime. Se você não estiver recebendo 
o feedback esperado por alguns dias, adicione um comentário solicitando atualizações no próprio pull request ou 
envie um e-mail para a lista de discussão.

### Commits diretos para o repositório

Ocasionalmente, você verá os principais membros da equipe se comprometerem diretamente com o repositório sem passar 
pelo fluxo de trabalho da solicitação pull. Isso é reservado apenas para pequenas alterações e a regra geral que usamos 
é esta: se a alteração afetar qualquer funcionalidade
que pode resultar em uma falha de teste, então ele tem que passar por um fluxo de trabalho de pull request.
Se, por outro lado, a alteração for na parte não funcional da base de código
(como corrigir um erro de digitação dentro de um bloco de comentário) os membros principais da equipe podem decidir apenas 
aceitar o commit diretamente ao repositório.

## Documentação

Para obter a documentação do banco de dados Greenplum, verifique a
[documentação online](http://docs.greenplum.org/).

Para obter mais informações além do escopo deste README, consulte
[nosso wiki](https://github.com/greenplum-db/gpdb/wiki)
