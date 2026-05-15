# Analyse du code source — `acme` (plan9port)

> Périmètre : `/usr/local/plan9/src/cmd/acme/` uniquement, **avec son
> répertoire `include/`** (en-têtes plan9port utilisés par acme). Le
> sous-projet `mail/` (mkfile séparé) est exclu. La couche `libframe`
> (`src/libframe/`) est hors périmètre mais référencée : la fonctionnalité
> d'emphase s'appuie sur une extension de cette bibliothèque.

## 1. Vue d'ensemble du projet

- **Type** : éditeur de texte graphique multi-fenêtres, à la souris, issu de Plan 9 et porté sur Unix par plan9port.
- **Langage** : C ANSI (style Plan 9, en-têtes `u.h`/`libc.h`/`draw.h`/`thread.h` fournis par plan9port).
- **Modèle de concurrence** : multi-thread coopératif via `libthread` (channels CSP, `threadmain`).
- **Interface système** : sert un système de fichiers 9P via `9pserve` (`fsys.c`). Les fenêtres et leurs commandes sont contrôlables par lecture/écriture de fichiers virtuels (`ctl`, `body`, `tag`, `addr`, `data`, `event`, etc.).
- **Architecture** : boucle d'événements + serveur 9P. Chaque sous-système (clavier, souris, plumb, commandes, xfid) tourne dans son propre thread et communique par channels.
- **Taille** (au 2026-05-15) : **~15 500 lignes** de C sur 23 fichiers `.c` + 3 fichiers `.h`. Plus grands : `exec.c` (1870), `text.c` (1865), `ecmd.c` (1396), `acme.c` (1176), `xfid.c` (1171), `look.c` (942), `rows.c` (857), `regx.c` (843).
- **Build** : `mkfile` Plan 9 (`mk`), via `$PLAN9/src/mkone` et `$PLAN9/src/mkdirs`.

## 2. Structure du répertoire

```
src/cmd/acme/
├── *.c, *.h            — sources et en-têtes (23 .c + 3 .h)
├── mkfile              — règles de build mk
├── NOTES               — notes du mainteneur (idées, références man pages)
├── TASK                — énoncé original de la fonctionnalité Emph
├── STAGE0_SUMMARY.md   — bilan du stage 0 (infrastructure de tests)
├── CODE_CHANGES.md     — explication des changements vs amont (feature Emph)
├── CLAUDE.md           — instructions pour assistants IA
├── codebase_analysis.md — ce fichier
├── .ai-jail            — garde-fou pour assistants IA
├── tests/              — suite de tests (créée pour Emph)
│   ├── emph_test.c     — logique pure des ranges (groupes A-F)
│   ├── g_test.c        — intégration regex rxcompile/rxexecute (G1-G4)
│   ├── test_ctl.rc     — tests d'interface 9P
│   └── mkfile
├── include/            — en-têtes plan9port partagés (45 fichiers)
└── (mail/ — sous-projet séparé, exclu de cette analyse)
```

> Note : il n'existe **pas** de fichiers `ACME-SPEC` / `ACME-USER` dans ce
> répertoire (d'anciennes versions du `CLAUDE.md` et du `TASK` y font
> référence). La documentation de la fonctionnalité Emph vit dans les
> manpages plan9port : `man/man1/acme.1` (CLI, commandes) et
> `man/man4/acme.4` (verbes `ctl`).

### Le répertoire `include/`

Snapshot local des 45 en-têtes plan9port utilisés (directement ou
indirectement) par acme. Ce ne sont **pas** des en-têtes propres à acme :
ce sont les API publiques de plan9port. Regroupés par rôle :

**Système & C standard (Plan 9)**
- `u.h`, `libc.h`, `lib9.h` — types et libc Plan 9.
- `utf.h`, `fmt.h`, `bio.h` — chaînes UTF, printf, buffered I/O.
- `bin.h`, `ar.h`, `mach.h` — formats binaires et machine.

**Graphique & UI** (cœur du rendu acme)
- `draw.h` — primitives de dessin (Image, Rect, draw, string, stringnwidth).
- `frame.h` — `libframe` : structures `Frame` et `Frbox`. **Étendu pour
  Emph** : `Frbox` porte désormais un champ `Font *font` (`nil` => police
  du frame), la macro `FRBOXFONT(f,b)` choisit la bonne police, et la
  fonction publique `frsetboxfont(Frame*, ulong, ulong, Font*)` affecte
  une police à une plage de boîtes (cf. § 11).
- `memdraw.h`, `memlayer.h` — gestion d'images en mémoire.
- `cursor.h`, `mouse.h`, `keyboard.h` — événements pointeur/clavier.
- `event.h` — pompe d'événements.
- `drawfcall.h` — protocole `devdraw` (X11 bridge).
- `geometry.h` — primitives géométriques.
- `complete.h` — auto-complétion.

**Threading & IPC**
- `thread.h` — `libthread` (Channels, threadmain, proccreate).
- `mux.h` — multiplexage de messages.

**9P (filesystem protocol)**
- `9p.h` — serveur 9P (utilisé par `fsys.c`).
- `9pclient.h` — client 9P.
- `fcall.h` — messages 9P (`Fcall`).
- `plumb.h` — `libplumb` (routage de messages inter-apps via `plumber`).

**Texte & regex**
- `regexp.h`, `regexp9.h` — moteurs regex Plan 9. `regx.c` d'acme
  consomme `regexp9.h` (sur runes) ; `regexp.h` est l'API octets.

**Réseau & crypto** (utilisés indirectement par les commandes Edit/Get/Put)
- `ip.h`, `ndb.h` — réseau, base de données réseau.
- `auth.h`, `authsrv.h`, `libsec.h` — authentification, crypto.
- `mp.h` — grands entiers (multi-précision).
- `httpd.h`, `sunrpc.h`, `nfs3.h` — serveurs intégrés (présents pour
  cohérence du snapshot).

**Stockage & sérialisation**
- `disk.h`, `diskfs.h` — couche disque.
- `flate.h` — compression.
- `venti.h` — stockage adressable par contenu (non utilisé par acme).
- `avl.h`, `libString.h` — structures de données.

**API d'acme côté client**
- `acme.h` — **important** : API consommée par les clients externes
  (`mail/`, `win`, scripts). Exposée *par* acme, pas consommée *par* lui.

**Divers**
- `html.h` — parseur HTML (utilisé par `mail/`).

Note : ce répertoire est une copie locale ; en build normal ces en-têtes
viennent de `$PLAN9/include`.

## 3. Décomposition par fichier

### Cœur applicatif

- **`acme.c`** (1176 lignes) : `threadmain`, parsing CLI (`-f` police variable, `-F` chasse fixe, `-e`/`-E` polices d'emphase), `fontnames[4]` global, démarrage des threads, boucle principale d'événements. Fournit `rfget(int fix, int save, int setfont, char *name)` (~ligne 899) — cache de `Reffont` par nom, avec comptage de références.
- **`fsys.c`** (749 lignes) : serveur 9P d'acme. Définit le `Dirtab` (table des fichiers virtuels : `cons`, `index`, `log`, `new`, et par fenêtre `addr`/`body`/`ctl`/`nctl`/`data`/`event`/`tag`/`xdata`/...). Énumération `Q…`/`QW…` au sommet de `dat.h`.
- **`xfid.c`** (1171 lignes) : implémentation des opérations 9P côté serveur. **Crochet `/ctl`** : `xfidctlwrite` gère les verbes `clean`, `dirty`, `show`, `name X`, `font X`, `del`, `get`, `put`, `dot=addr`, `addr=dot`, `limit=addr`, `nomark`, `mark`, `cleartag`, `dumpdir`, `dump`. Tout `xfidctlwrite` tourne sous `winlock(w, 'F')`. **Crochet `/nctl`** : `xfidnctlwrite` gère les verbes d'emphase `emph=REGEX`, `noemph`, `emphfont <path>`, `emphfont` ; la lecture de `nctl` renvoie le nom de la police d'emphase courante.
- **`exec.c`** (1870 lignes) : table `exectab[]` des commandes acme (Cut, Paste, Del, Edit, Font, Get, Put, Look, Send, Sort, Tab, Undo, Zerox, ...). Dispatch `execute()` qui mappe un nom de commande vers un handler `void f(Text *et, Text *t, Text *argt, int flag1, int flag2, Rune *arg, int narg)`. Commandes d'emphase : `LEmph`/`emph`, `LEmphFont`/`emphfontx`, `LEmphMe`/`emphme`, `LEmphAll`/`emphall`, `LEmphNone`/`emphnone`, `LAutoEmph`/`autoemphx`. Le handler `get` rejoue `emphrecompute`+`emphapply`+`emphauto` ; le handler `fontx` (commande `Font`) appelle `emphfontupdate`.
- **`wind.c`** (733 lignes) : cycle de vie d'une `Window`. `wininit` initialise les champs Emph à zéro/`nil` (wind.c:84-89) ; `winclose` appelle `emphfree` (wind.c:334).
- **`text.c`** (1865 lignes) : couche `Text` (Frame libframe + buffer) — rendu, sélection, insertion (`textinsert`), suppression (`textdelete`), double-clic, fill, redraw. Contient tout le **pipeline de politique d'emphase** (cf. § 6).
- **`cols.c`** (591 lignes), **`rows.c`** (857 lignes) : géométrie (colonnes / lignes de fenêtres) — drag, resize, drop.
- **`scrl.c`** (159 lignes) : barre de défilement.

### Données et buffers

- **`dat.h`** (605 lignes) : structures centrales — `Buffer`, `Block`, `File`, `Elog`, `Text`, `Window`, `Column`, `Row`, `Reffont`, `Range`, `Rangeset`, `Dirtab`, `Fid`, `Xfid`, `Command`, `Mntdir`, `Timer`, `Expand`. Déclare `fontnames[4]`, les channels globaux, et les prototypes des fonctions d'emphase. **`Window`** porte 8 champs d'emphase : `emphon`, `emphpat`/`nemphpat`, `emphmatch`/`nemphmatch`/`aemphmatch`, `emphfont`, `emphfontpath`. Variable globale `autoemph` (int, définie dans dat.c) : active l'auto-emphase à l'ouverture de fichier.
- **`dat.c`** (62 lignes) : définitions/instances des variables globales.
- **`buff.c`** (325 lignes) : `Buffer` (insertion/suppression de runes, pagination disque via `Block`).
- **`disk.c`** (133 lignes) : stockage temporaire des blocs sur disque.
- **`file.c`** (311 lignes) : `File` (un `Buffer` + historique undo/redo via `delta`/`epsilon`, nom, mtime, sha1).
- **`elog.c`** (354 lignes) : journal des éditions en attente (commandes `Edit`).

### Édition et adresses

- **`edit.h`, `edit.c`** (686 lignes), **`ecmd.c`** (1396 lignes) : langage `Edit` (style `sam` : `a/`, `c/`, `d`, `s///`, `x/`, `g/`, ...). Parseur (yacc-ish hand-written) + AST (`Cmd`, `Addr`) + exécuteur par commande.
- **`addr.c`** (297 lignes) : évaluation des adresses (`#n`, lignes, regex, `,`, `;`, `+`, `-`).
- **`regx.c`** (843 lignes) : moteur regex d'acme (compile/exécute) sur runes. Fonctions clés : `rxcompile`, `rxexecute`, `rxbexecute`. **Stateful global** : `lastregexp` — une seule regex compilée à la fois ; `rxcompile` recompile seulement si le pattern change.
- **`logf.c`** (199 lignes) : fichier `log` (flux d'événements de fenêtres).
- **`look.c`** (942 lignes) : recherche, expansion B3, navigation (`Look`, plumb).

### Emphase (ajout récent)

- **`emphranges.c`** (117 lignes) : logique **pure** de manipulation des tableaux de `Range`, sans dépendance à acme ni à l'affichage :
  - `emphpush` — append d'une plage, capacité doublée au besoin.
  - `rangeshift` — décale les plages après une insertion/suppression ; **abandonne** les plages qui chevauchent l'édition.
  - `rangemerge` — fusionne deux tableaux triés en gardant l'ordre.
  - `emphfreearr` — libère le tableau et le remet à zéro.
  Testée hors GUI ; couverte par 44 tests (groupes A-F de `tests/emph_test.c`).

### Service / utilitaires

- **`fns.h`** (109 lignes) : prototypes inter-fichiers (les fonctions d'emphase sont déclarées dans `dat.h`, pas ici).
- **`util.c`** (497 lignes) : `emalloc`, `erealloc`, `estrdup`, `runemalloc`, `warning`, `error`, gestion de processus, utilitaires runes/octets. Ces allocs abortent à l'échec — pas de null-check après.
- **`time.c`** (124 lignes) : timers via channels.
- **`mkfile`** : build Plan 9 mk ; `emphranges.$O` dans `OFILES` ; cibles `test` (recurse dans `tests/`), `likeplan9` et `diffplan9`.

### Tests

- **`tests/emph_test.c`** : runner C autonome, sans framework. Groupes A-F (logique de ranges) — **44 PASS** : A (`emphpush`), B/C (`rangeshift` insertion/suppression), D (compaction multi-ranges), E (`rangemerge`), F (`emphfreearr`).
- **`tests/g_test.c`** : intégration regex (G1-G4, 11 tests) — appelle `rxcompile`/`rxexecute` directement (sans dépendance `textreadc`), couvre notamment les matches vides (`^`, `$`) sans boucle infinie.
- **`tests/test_ctl.rc`** : tests d'interface 9P (T01-T04, T06). SKIP propre (exit 77, TAP) si `/mnt/acme` non monté.
- **`tests/mkfile`** : build des runners.

### Documentation / état

- **`NOTES`** : notes du mainteneur (idées, références aux man pages 9).
- **`TASK`** : énoncé original de la fonctionnalité `emph=regex` / `Emph`.
- **`STAGE0_SUMMARY.md`** : bilan du stage 0 (infrastructure de tests).
- **`CODE_CHANGES.md`** : explication détaillée, pour un nouveau développeur, de tous les changements apportés vs l'amont par la fonctionnalité Emph (couches libframe + acme).
- **`CLAUDE.md`** : guide pour assistants IA.

## 4. Endpoints — fichiers virtuels 9P

Acme n'expose pas d'API HTTP mais un système de fichiers 9P. Résumé des
fichiers globaux et par fenêtre sous `/mnt/acme` :

| Fichier      | Sémantique principale |
|--------------|-----------------------|
| `cons`       | sortie standard partagée des commandes |
| `consctl`    | stub de compatibilité |
| `index`      | une ligne par fenêtre (id, tailles, flags, tag) |
| `label`      | stub |
| `log`        | flux des événements de fenêtres (new/zerox/get/put/del) |
| `new/`       | accéder à un fichier ici crée une nouvelle fenêtre |
| `<id>/addr`  | définit/lit l'adresse courante (pour `data`/`xdata`) |
| `<id>/body`  | corps de la fenêtre |
| `<id>/tag`   | barre de tag |
| `<id>/data`  | accès aléatoire au corps, indexé par `addr` |
| `<id>/xdata` | comme `data`, lecture bornée à fin d'`addr` |
| `<id>/ctl`   | lecture : 10 entiers + nom de police ; écriture : messages texte (`dot=addr`, `clean`, `name X`, `font X`, ...) |
| `<id>/nctl`  | emphase par fenêtre : écriture : `emph=REGEX`, `noemph`, `emphfont <path>`, `emphfont` ; lecture : nom de la police d'emphase courante |
| `<id>/event` | rapporte/intercepte les actions souris B2/B3 |
| `<id>/errors`| ajoute au `+Errors` du dossier |
| `<id>/editout` | sortie des commandes `Edit` |

## 5. Architecture interne — flux des données

```
              +------------------+
              |  threadmain()    |  acme.c
              +---------+--------+
                        |
        +---------------+---------------+----------------+
        |               |               |                |
        v               v               v                v
  +-----------+   +-----------+   +-----------+   +-------------+
  | keyboard  |   | mouse     |   | plumb     |   | 9P server   |
  | thread    |   | thread    |   | thread    |   | (fsys.c)    |
  +-----+-----+   +-----+-----+   +-----+-----+   +------+------+
        |               |               |                |
        |   channels (cwait, ckill, ccommand, ...)        |
        +---------------+---------------+----------------+
                        |
                        v
                 +-------------+
                 |  Window     |  wind.c  (locked by Window.lk)
                 |  +--------+ |
                 |  |  Text  | |  text.c  -> Frame (libframe)
                 |  | tag    | |
                 |  +--------+ |
                 |  |  Text  | |
                 |  | body   | |  -> File -> Buffer -> Block*  (disque)
                 |  +--------+ |
                 +------+------+
                        |
                        v
                 +-------------+
                 |  Disk       |  disk.c (stockage paginé)
                 +-------------+

  Edit (sam-style)  :  ecmd.c  ->  edit.c  ->  elog.c  ->  buffer
  Regex             :  regx.c  (rxcompile / rxexecute / rxbexecute)
  Adresses          :  addr.c  (#n, /re/, ., $, , ; + -)
  Exécution cmds    :  exec.c  (exectab[] -> handlers)
  Contrôle 9P       :  fsys.c (Dirtab) -> xfid.c (xfidctlwrite, ...)
  Emphase regex     :  text.c (setemph, emphrecompute, emphapply)
                       + emphranges.c (logique pure des ranges)
                       + libframe (frsetboxfont, police par boîte)
```

### Pile de données (bas vers haut)

```
Block (disk.c)   blocs runes de taille fixe, pageable disque
  ↑
Buffer (buff.c)  séquence de runes avec bloc courant en cache
  ↑
File (file.c)    Buffer + delta/epsilon undo + nom/mtime/sha1
  ↑
Text (text.c)    File + libframe Frame + Reffont + sélection + géométrie
  ↑
Window (wind.c)  tag Text + body Text + état 9P + état d'emphase
  ↑
Column / Row     géométrie (cols.c, rows.c)
```

### Cycle d'une écriture `ctl` (ex. `dot=addr`, ou `emph=foo`)

1. Client écrit dans `/mnt/acme/<id>/ctl`.
2. `fsys.c` route le message 9P vers un `Xfid` ; `xfidwrite` détecte `QWctl`.
3. `xfidctlwrite` (xfid.c) lit ligne par ligne, compare le préfixe contre une liste de mots-clés.
4. Le handler manipule la `Window`/`Text` correspondante sous `winlock(w, 'F')`.
5. Réponse 9P renvoyée au client.

### Cycle d'une commande utilisateur (ex. `Edit`, `Emph`)

1. L'utilisateur clique B2 sur le mot dans un tag/corps.
2. `execute()` (exec.c) cherche dans `exectab[]`.
3. Handler trouvé (ou commande shell sinon) sur le thread editor.
4. Le handler `emph` ne prend **pas** de verrou interne : la sérialisation
   est garantie par l'appelant (voir § 11, piège du deadlock).

## 6. Polices et emphase — état au 2026-05-15

La fonctionnalité d'emphase (`Emph`) est **implémentée et opérationnelle**.
Elle met en évidence toutes les occurrences d'un regex dans le corps d'une
fenêtre, en les rendant dans une police différente **et une couleur différente**.

### Polices

Tableau `fontnames[4]` (acme.c, dat.h) :
- `[0]` police variable principale (`-f`)
- `[1]` police à chasse fixe principale (`-F`)
- `[2]` police variable d'emphase (`-e`) — exportée via `$emphfont`
- `[3]` police à chasse fixe d'emphase (`-E`)

Chargement via `rfget(int fix, int save, int setfont, char *name)`
(acme.c:899). La police d'emphase d'une fenêtre est chargée **à la
demande** lors du premier `Emph` et conservée dans `Window.emphfont`
(un `Reffont*`). `emphfontname` (text.c) choisit `[2]` ou `[3]` selon le
mode (variable/fixe) du corps de la fenêtre ; `emphfontupdate` la recharge
quand la commande `Font` bascule la police du corps.

> Contrainte : la police d'emphase doit avoir la **même hauteur de ligne**
> que la police principale. La largeur des glyphes peut différer (les
> lignes se recomposent), mais libframe suppose une hauteur uniforme.

### Couleur d'emphase

L'option `-C colorspec` (acme.c) définit la couleur de premier plan du texte
emphasé. Le `colorspec` accepte 3 ou 6 chiffres hexadécimaux RGB :
- 3 chiffres : chaque chiffre est doublé (ex: `82f` → `0x8822ffff`)
- 6 chiffres : utilisé tel quel (ex: `8822ff` → `0x8822ffff`)

Parsing via `parsecolor(char *spec, ulong *rgb)` (util.c). Si `-C` n'est pas
fourni, la couleur par défaut est bleu foncé (`0x0000AAFF`).

Les couleurs sont stockées dans `tagcols[EMPH]` et `textcols[EMPH]` (initialisées
dans `iconinit()`, acme.c). libframe a été étendu avec un slot `EMPH` dans
l'enum des couleurs (frame.h, NCOL passe de 5 à 6). Les fonctions de dessin
`_frdrawtext()` et `frdrawsel0()` (frdraw.c) utilisent `f->cols[EMPH]` au lieu
de `f->cols[TEXT]` lorsqu'une boîte a une police personnalisée (`b->font != nil`).

### Architecture en deux couches

```
   COUCHE ACME (la politique : quoi emphaser, quelle couleur)  -- text.c, emphranges.c
   ---------------------------------------------------------------
   commande Emph / ctl emph=        ->  setemph()
   emphmatch[] : liste triée des plages qui matchent le regex
   emphapply() : projette emphmatch[] sur les boîtes visibles
                          |
                          v   appelle frsetboxfont()
   ---------------------------------------------------------------
   COUCHE LIBFRAME (le mécanisme : rendre une boîte autrement)
   ---------------------------------------------------------------
   Frbox.font : police propre à la boîte (nil => police du frame)
   Frame.cols[EMPH] : couleur de premier plan pour texte emphasé
   FRBOXFONT(f,b) : macro qui choisit la bonne police
   frsetboxfont() : affecte une police à une plage de boîtes
   _frdrawtext() / frdrawsel0() : utilisent cols[EMPH] si b->font != nil
```

### État de l'emphase (couche acme)

- **`/ctl` verbes** : `emph=REGEX` et `noemph` (xfid.c:799/818, sous `winlock('F')`).
- **Commande utilisateur** : `Emph regex` (B2), `Emph` seul (toggle on/off du dernier pattern), chord 2-1 (exec.c:1110).
- **Point d'entrée unique** : `setemph` (text.c:1730) — branche « off » : éteint et redessine ; branche « on » : compile le regex, charge la police à la demande (`emphfontname`), recalcule `emphmatch[]`, applique, redessine.
- **Source de vérité** : `Window.emphmatch[]` — tableau dynamique trié, sans chevauchement, de `Range` ; stocke des **offsets dans le fichier** (survivent au défilement).
- **Calcul des matches** : `emphrecompute` (text.c:1772) balaye tout le fichier via `rxexecute` ; `emphrefreshlocal` (text.c:1792) ne réexamine qu'une fenêtre locale autour d'une édition.
- **Application au rendu** : `emphclear` (text.c:1685) remet toutes les boîtes en police standard ; `emphapply` (text.c:1693) efface tout puis applique la police d'emphase aux boîtes des plages *visibles*, en convertissant les offsets fichier en offsets frame via `body.org`, puis rejoue la mise en page via `frrelayout` (libframe) + `textfill` — `frsetboxfont` ne refait pas le repli des lignes, donc sans cela une police d'emphase plus large déborde le corps sur la tagline et finit par planter acme ; `emphapplylocal` (text.c:1721) fait suivre d'un `frredraw`.
- **Hooks d'édition** : `textinsert` (text.c:407) et `textdelete` (text.c:517) appellent `emphshift` + `emphrefreshlocal` + `emphapplylocal`. `textredraw` / `textsetorigin` (text.c:69, 1659) rejouent `emphapply` après remplissage du frame — sinon l'emphase disparaît au scroll (les boîtes sont reconstruites avec `font = nil`).
- **Couplage avec `Font`** : `fontx` (exec.c) appelle `emphfontupdate` après avoir changé la police du corps — un seul `Font` bascule corps *et* emphase (variable <-> fixe).
- **Cycle de vie** : `wininit` initialise les champs à zéro/`nil` ; `winclose` -> `emphfree` (text.c:1850) libère regex, tableau de plages et police (`rfclose`).
- **Tests** : 44 PASS pour la logique de ranges (A-F), 11 pour l'intégration regex (G1-G4).
- **Documentation** : manpages `man/man1/acme.1` et `man/man4/acme.4`.

## 7. Environnement et build

- **Pré-requis** : installation plan9port (`$PLAN9` défini, binaires `mk`, `9c`, etc. sur le `PATH`).
- **Construction** :
  ```
  mk             # build acme + mail/
  mk install     # installe dans $PLAN9/bin
  mk clean
  mk test        # recurse dans tests/, lance o.emph_test, o.g_test, test_ctl.rc
  mk likeplan9   # vue "comme Plan 9" via sed (rewrites de field access)
  mk diffplan9   # diff vs ../plan9 (sibling upstream) pour voir la dérive
  ```
- **Variables d'environnement notables** : `$PLAN9`, `$font`, `$emphfont`, `$acmeshell`, `$home`.
- **Lancement** :
  ```
  acme [-a] [-c ncol] [-C colorspec] [-f varfont] [-F fixfont] [-e emphfont] [-E emphfixfont]
       [-l loadfile] [-W winsize] [file ...]
  ```
- **Pas de CI/CD** : ni `.github/workflows`, ni Dockerfile, ni `.gitlab-ci.yml`. Build et test sont locaux.

## 8. Pile technologique

| Couche        | Choix |
|---------------|-------|
| Langage       | C (style Plan 9, ANSI) |
| Concurrence   | `libthread` (CSP : threads coopératifs + channels) |
| Graphique     | `libdraw`, `libframe` (frames de texte runes, police par boîte) |
| Texte         | Runes Unicode (UTF-8 dehors, rune-32 dedans) — convertir uniquement aux frontières 9P/disque (`bytetorune`, `runetobyte`, `cvttorunes`) |
| Regex         | Moteur interne `regx.c` sur runes (`rxcompile`, `rxexecute`, `rxbexecute`) |
| Stockage      | Buffer paginé en blocs, swap disque (`disk.c`) |
| IPC externe   | 9P via `9pserve` |
| Build         | `mk` (plan9port) |
| Plumbing      | `libplumb` (routage de messages inter-apps) |
| Tests         | Runner C autonome (`emph_test.c`, `g_test.c`) + script `rc` (`test_ctl.rc`). Aucun framework externe. |

## 9. Schéma visuel

```
       Utilisateur (souris, clavier)
                 |
                 v
   +--------------------------+         +----------------------+
   |  acme (process)          |<------->|  9pserve (process)   |
   |  threads CSP             |  9P     |  -> /mnt/acme        |
   |  - mouse, kbd, plumb     |         +----------------------+
   |  - command dispatcher    |                  ^
   |  - 9P server (fsys.c)    |                  |
   |  - per-Xfid worker       |                  |
   +--------------------------+         +----------------------+
                 |                      | Programmes externes  |
                 v                      | (sed, awk, win, ...) |
   +--------------------------+         | echo emph=foo > ctl  |
   |  Window (lk-protected)   |         +----------------------+
   |  - tag Text              |
   |  - body Text             |
   |  - File -> Buffer        |
   |  - emphmatch[] (regex)   |
   +--------------------------+
                 |
                 v
   +--------------------------+        +----------------------+
   |  libframe (Frame)        |<------>|  libdraw (Image)     |
   |  - box[] layout          |        |  -> X11/devdraw      |
   |  - Frbox.font par boîte  |        +----------------------+
   |  - FRBOXFONT / frsetbox  |
   +--------------------------+
```

## 10. Qualité, observations

- **Style** : très Plan 9 — fonctions courtes, peu de commentaires, identifiants concis. Pas de tests automatisés historiquement ; les tests présents (`tests/`) ont été ajoutés *pour* la fonctionnalité Emph.
- **Sécurité / robustesse** : code mature, usage intensif de pointeurs bruts et `emalloc` (qui aborte au lieu de retourner nil). Rester rigoureux sur la libération des `Rune*` lorsqu'on ajoute des champs `Window`.
- **Concurrence** : tout passage par `xfidctlwrite` est sérialisé par `winlock` — bonne pratique pour tout nouveau verbe `/ctl`. Le handler `emph` ne verrouille **pas** : la sérialisation vient de l'appelant. Reprendre `winlock` y provoquerait un deadlock (`qlock` non réentrant — cf. § 11).
- **Compatibilité Plan 9 amont** : la cible `likeplan9` du `mkfile` aligne ce fork via des `sed`. Toute extension locale (fontnames[2..3], `emphranges.c`, `Frbox.font`) divergera — surveiller avec `mk diffplan9`. Si on renomme un champ ciblé par les `sed` (`fcall`, `lk`, `b`, `fr`, `ref`, `m`, `u`, `u1`), mettre la règle à jour.
- **Rétrocompatibilité libframe** : une boîte dont `font == nil` se comporte exactement comme avant ; `sam`, `samterm`, `9term` n'appellent jamais `frsetboxfont` et ne voient aucun changement. Ces programmes initialisent `cols[EMPH]` à `display->black` pour compatibilité avec l'extension NCOL=6.
- **Documentation** : les manpages `man/man1/acme.1` et `man/man4/acme.4` doivent être mises à jour à chaque ajout de commande utilisateur ou de verbe `/ctl`.
- **Dépendance Dirtab** : ne pas casser l'ordre du `Dirtab` ni l'énumération `Q…`/`QW…` — les macros `QID` et `FILE` en dépendent.

## 11. La fonctionnalité Emph — historique et conception

### Le problème central

libframe découpe le texte visible en **boîtes** (`Frbox`) et utilisait
*toujours* une unique police (`frame->font`). Pour emphaser, il faut que
certaines boîtes soient rendues dans une autre police : libframe était par
construction « mono-police ».

### Approche abandonnée : l'overlay

Une première implémentation **superposait** un second passage de dessin
(`textemphdraw` / `emphpaint`) par-dessus le rendu standard. Cette approche
a produit cinq bugs structurels (sélection souris incohérente, artefacts de
pixels, emphase fantôme après changement de regex, crashs) — **insolubles**
tant que libframe restait mono-police. Elle a été entièrement supprimée.

### Approche retenue : police par boîte

libframe a été étendu pour qu'une boîte porte sa *propre* police. La
politique (« quelles plages emphaser ») reste dans acme ; libframe ne gagne
que la *capacité* technique de rendre boîte par boîte. Modifications :

- `include/frame.h` : champ `Frbox.font`, macro `FRBOXFONT(f,b)`, prototype `frsetboxfont`.
- `src/libframe/frboxfont.c` *(nouveau)* : `frsetboxfont` — découpe les boîtes aux bornes de la plage puis affecte la police, recalcule `b->wid`.
- `src/libframe/frdraw.c`, `frutil.c`, `frbox.c`, `frptofchar.c`, `frinsert.c` : remplacement de `f->font` par `FRBOXFONT(f,b)` aux points de dessin, de mesure et de conversion pixel↔caractère ; `_frclean` interdit de fusionner deux boîtes de polices différentes ; `bxscan` initialise `b->font = nil`.

### Pièges résolus (récapitulatif `CODE_CHANGES.md`)

| Problème | Cause | Solution |
|---|---|---|
| libframe mono-police | `f->font` partout | champ `Frbox.font` + macro `FRBOXFONT` |
| Approche overlay buggée | second passage de dessin fragile | abandonnée au profit de la police par boîte |
| Crash au clic souris | `frptofchar` marchait en police standard sur des boîtes plus larges | `FRBOXFONT` dans `frptofchar.c` |
| Emphase perdue au scroll / `Get` | boîtes reconstruites avec `font = nil` | rejouer `emphapply` dans `textsetorigin`, `textredraw`, `get` |
| Fusion de boîtes incohérente | `_frclean` fusionnait des polices différentes | garde `b[0].font == b[1].font` |
| Deadlock de la commande `Emph` | `winlock` non réentrant, déjà tenu par l'appelant | suppression du verrou interne dans le handler |
| Emphase fantôme après collage | `emphapply` corrige la donnée mais ne repeint pas | `emphapplylocal` fait suivre d'un `frredraw` |
| Débordement / crash après `Emph` (police plus large) | `frsetboxfont` élargit les boîtes sans refaire le repli des lignes | `frrelayout` rejoue `_frdraw` ; `emphapply` l'appelle puis `textfill` |
| Police d'emphase fixe (`-E`) jamais utilisée | `setemph` chargeait toujours `fontnames[2]` | `emphfontname` choisit `[2]`/`[3]` selon le mode du corps |
| `Font` ne basculait pas l'emphase | `fontx` ignorait `w->emphfont` | `emphfontupdate` appelé après le changement de police |

Détail des matchs : après chaque match on avance `p = q1` ; mais si le
match est *vide* (`q0 == q1`, ancres `^`/`$`), on avance `p = q0 + 1` pour
éviter une boucle infinie.

---

Fichier mis à jour le 2026-05-15. Reflète l'état du code **après** la
refonte par-boîte-police (overlay supprimé, `frsetboxfont` en place,
fonctionnalité Emph opérationnelle).
