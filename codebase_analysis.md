# Analyse du code source — plan9port (`/usr/local/plan9`)

> Périmètre : l'ensemble de l'arbre `/usr/local/plan9`. Niveau : vue
> d'ensemble structurée — architecture globale, rôle de chaque répertoire,
> pile technologique et système de build, sans détailler un à un les
> ~270 programmes. Le sous-projet `src/cmd/acme/mail/` n'est pas couvert.

## 1. Vue d'ensemble du projet

- **Type** : *Plan 9 from User Space* (« plan9port ») — un portage sur
  Unix d'un large ensemble de bibliothèques et de programmes de
  l'OS de recherche **Plan 9** (Bell Labs). Ce n'est pas une application
  unique mais une **suite complète** : éditeurs, shell, outils de
  composition de documents (troff), graphisme, réseau, crypto, etc.
- **Langage** : essentiellement **C** (style Plan 9, ANSI/C89), avec
  quelques grammaires **yacc** (`.y`) et **lex** (`.lx`), des scripts
  **rc** (le shell de Plan 9) et du shell POSIX pour l'amorçage.
- **Modèle** : bibliothèque portable `lib9` qui réimplémente l'API Plan 9
  au-dessus de POSIX, puis tout le reste construit dessus. Le code Plan 9
  d'origine est ainsi peu modifié.
- **Concurrence** : `libthread` fournit un modèle CSP (threads coopératifs
  + `Channel`s), porté sur les threads natifs / `fork` selon la plateforme.
- **Licence** : MIT (Plan 9 Foundation, 2021), avec exceptions documentées
  (bzip2, polices B&H — voir `LICENSE` et `font/LICENSE`).
- **Taille** : ~274 Mo sur disque. 32 bibliothèques (`src/lib*`),
  68 répertoires de commandes (`src/cmd/*/`) plus de nombreux programmes
  d'un seul fichier directement dans `src/cmd/`, 45 en-têtes publics,
  ~310 pages de manuel.
- **Amont** : <https://github.com/9fans/plan9port>. Mainteneur historique :
  Russ Cox. Cet arbre local diverge de l'amont (voir `CODE_CHANGES.md` à
  la racine et la fonctionnalité d'emphase dans `src/cmd/acme/`).

## 2. Structure des répertoires de premier niveau

```
/usr/local/plan9/
├── INSTALL, configure, Makefile  — amorçage et build
├── config, LOCAL.config          — config générée (gitignored)
├── README.md, CHANGES, LICENSE, CONTRIBUTING.md, install.txt
├── CODE_CHANGES.md               — note de divergence locale (feature Emph)
├── src/        — TOUT le code source (libs + commandes)
├── include/    — 45 en-têtes publics partagés
├── bin/        — exécutables compilés (gitignored)
├── lib/        — archives .a compilées + données runtime
├── man/        — pages de manuel (man1, man3, man4, man7, man8, man9)
├── font/       — polices bitmap Plan 9
├── face/       — vignettes pour faces(1)
├── postscript/ — polices et prologues PostScript (troff -> PS)
├── troff/      — polices et tables de terminaux pour troff
├── tmac/       — paquets de macros troff (-ms, -me, -man, ...)
├── tmac/me/    — variante du paquet -me
├── ndb/        — fichiers de base de données réseau
├── plumb/      — règles du service plumber
├── acid/       — scripts du débogueur acid
├── sky/        — catalogue d'étoiles pour scat(1)
├── dict/       — données de dictionnaires
├── lp/         — spouleur d'impression (line printer)
├── dist/       — empaquetage (Debian, distribution troff)
├── mac/        — bundles d'applications macOS (9term.app, Plumb.app)
├── unix/       — mkfiles d'amorçage pour construire mk lui-même
├── proto/      — listes de fichiers d'installation
├── news/       — annonces
├── rcmain      — script d'init du shell rc
└── .github/    — CI (GitHub Actions)
```

### `src/` — le code source

Deux familles :

- **`src/lib*/`** — 32 bibliothèques (voir § 4).
- **`src/cmd/`** — les commandes. Les programmes simples sont un fichier
  `.c` posé directement dans `src/cmd/` (`cat.c`, `ls.c`, `echo.c`,
  `9pserve.c`, ...). Les programmes complexes ont leur propre
  sous-répertoire avec un `mkfile` (`acme/`, `sam/`, `rio/`, `troff/`,
  `venti/`, `upas/`, `fossil/`, `devdraw/`, ...).

### `include/` — en-têtes publics

45 en-têtes définissant les API publiques. Les fondamentaux : `u.h`
(types machine), `libc.h` / `lib9.h` (la libc Plan 9 portée), `utf.h`,
`fmt.h`, `bio.h`. Puis les API par domaine : `draw.h`, `frame.h`,
`memdraw.h`, `memlayer.h` (graphisme) ; `thread.h`, `mux.h` (concurrence) ;
`9p.h`, `9pclient.h`, `fcall.h`, `plumb.h` (protocole 9P et plumbing) ;
`regexp.h`, `regexp9.h` (regex) ; `auth.h`, `authsrv.h`, `libsec.h`,
`mp.h` (crypto) ; `venti.h`, `disk.h`, `diskfs.h` (stockage) ; etc.

### `bin/` et `lib/` — produits de build

`bin/` reçoit tous les exécutables (entièrement listé dans `.gitignore`).
`lib/` contient les archives statiques `lib*.a` **et** des données runtime
non compilées : `fortunes`, `keyboard`, `mimetype`, `units`, `yaccpar`,
`acme.rc`, l'index des polices, etc.

### Données et ressources (non-code)

`font/`, `face/`, `postscript/`, `troff/`, `tmac/` portent la chaîne de
composition de documents (troff) et le graphisme : polices bitmap et
PostScript, macros, tables de terminaux. `ndb/`, `plumb/`, `acid/`,
`sky/`, `dict/` sont des données de configuration ou d'application.

## 3. Le système de build

plan9port **n'utilise pas** `make` mais **`mk`**, l'outil de build de
Plan 9. Chaîne d'amorçage :

1. `./INSTALL` définit `$PLAN9` (= racine de l'arbre), génère `config`
   via `./configure`, puis construit `mk` à partir des mkfiles
   d'amorçage de `unix/` (avec le `make`/`cc` du système).
2. Une fois `mk` disponible, `mk` pilote tout le reste de la
   construction et de l'installation dans `bin/` et `lib/`.
3. Le `Makefile` racine est un *leurre* : il ne fait qu'afficher
   « read the README file ».

Wrappers de compilation dans `bin/` : **`9c`** (compile), **`9l`**
(link), **`9`** (exécute une commande dans l'environnement Plan 9),
**`9 man`** (pages de manuel). Chaque répertoire a un `mkfile` qui
inclut les fragments partagés `$PLAN9/src/mkhdr`, `mkone`, `mkmany`,
`mkdirs`. Variable maîtresse : **`$PLAN9`** doit pointer la racine ;
`bin/` doit être dans le `PATH`.

## 4. Les bibliothèques (`src/lib*`)

| Bibliothèque | Rôle |
|---|---|
| `lib9` | cœur du portage : réimplémente la libc Plan 9 (`u.h`/`libc.h`) sur POSIX — chaînes, `print`/`fmt`, `utf`, `rune`, processus |
| `libthread` | concurrence CSP : `Channel`, `threadmain`, `proccreate` |
| `libbio` | I/O bufferisée (`Biobuf`) |
| `libdraw` | primitives graphiques portables (`Image`, `Rect`, `draw`, `string`) — dialogue avec `devdraw` |
| `libmemdraw`, `libmemlayer` | rendu d'images en mémoire, fenêtrage en couches |
| `libframe` | rendu de texte en « boîtes » (`Frame`/`Frbox`) — utilisé par acme, sam, 9term |
| `libgeometry` | géométrie (matrices, points 3D) |
| `lib9p`, `lib9pclient` | serveur / client du protocole de fichiers **9P** |
| `libmux` | multiplexage de messages (transport 9P) |
| `libplumb` | client du service de routage de messages `plumber` |
| `libregexp` | moteur d'expressions régulières |
| `libsec` | cryptographie (TLS, hash, chiffrement) |
| `libmp` | arithmétique multi-précision (grands entiers) |
| `libauth`, `libauthsrv` | authentification Plan 9 (factotum, secstore) |
| `libip` | utilitaires réseau / parsing d'adresses IP |
| `libndb` | base de données réseau (`ndb`) |
| `libhttpd` | serveur HTTP minimal |
| `libsunrpc` | Sun RPC (utilisé par les services NFS) |
| `libdisk`, `libdiskfs` | accès disque et systèmes de fichiers |
| `libventi` | stockage adressable par contenu (archiveur Venti) |
| `libflate` | compression deflate/inflate |
| `libbin` | allocation par « bassins » (pool allocator) |
| `libmach` | abstraction d'architecture machine (débogueurs, `acid`) |
| `libhtml` | parseur HTML |
| `libString` | type chaîne dynamique (`String`) |
| `libavl` | arbres AVL |
| `libcomplete` | auto-complétion de noms de fichiers |
| `libacme` | API cliente pour piloter l'éditeur acme via 9P |

## 5. Les programmes (`src/cmd`)

68 répertoires + de nombreux programmes mono-fichier. Regroupement par
domaine :

- **Éditeurs / shells** : `acme` (éditeur graphique, voir
  `src/cmd/acme/codebase_analysis.md` pour son analyse dédiée), `sam`
  + `samterm` (éditeur structurel), `rc` (le shell), `ed`.
- **Graphisme / fenêtrage** : `devdraw` (pont vers X11/Quartz),
  `rio` (gestionnaire de fenêtres), `9term` (terminal), `page`,
  `paint`, `jpg`/`gif`/`png`/`ppm`/... (codecs image), `colors`, `tweak`.
- **Composition de documents** : `troff`, `tbl`, `eqn`, `pic`, `grap`,
  `troff2html`, `htmlroff`, `proof`, et la chaîne `postscript/`.
- **Réseau / 9P** : `9pserve`, `9pfuse`, `9p`, `9import`, `srv`,
  `import`, `ndb`, `ip/` (pile et démons réseau), `listen1`.
- **Stockage / sauvegarde** : `venti`, `vac`, `fossil`, `vbackup`,
  `9660`/`9660srv` (ISO 9660), `tapefs`, `disk/`.
- **Sécurité** : `auth/` (`factotum`, `secstore`).
- **Mail** : `upas/` (suite de courrier — fs, smtp, marshal, etc.).
- **Développement** : `mk`, `yacc`, `lex`, `acid` (débogueur),
  `acidtypes`, `cb`, `db`.
- **Utilitaires Unix classiques** : `ls`, `cat`, `grep`, `sed`, `sort`,
  `diff`, `tar`, `gzip`/`bzip2`/`compress`, `awk`, `hoc`, `units`, etc.
- **Divers** : `astro`, `scat` (carte du ciel), `dict`, `faces`,
  `calendar`, `fortune`, `map`.

## 6. Architecture transversale

### Le protocole 9P — l'épine dorsale

L'idée centrale de Plan 9 : *tout est fichier*, et les services
s'exposent comme des arborescences de fichiers servies via le protocole
**9P**. Sur Unix, `9pserve` multiplexe ces services ; `9pfuse` permet de
les monter via FUSE. acme, le plumber, `fossil`, `venti`, `upas/fs` sont
tous des serveurs 9P. Les clients lisent/écrivent des fichiers virtuels
plutôt que d'appeler des API.

### La pile graphique

```
  Programme  --libdraw-->  devdraw  -->  X11 / Quartz
                  |
            libmemdraw / libmemlayer (rendu en mémoire)
                  |
            libframe (texte en boîtes : acme, sam, 9term)
```

`devdraw` est un processus séparé qui isole la dépendance au système de
fenêtrage hôte ; les programmes graphiques lui parlent via le protocole
`drawfcall`.

### Le plumbing

`plumber` route des messages typés entre applications selon les règles
de `plumb/`. C'est le mécanisme « clic pour ouvrir » : un clic sur un
chemin de fichier dans acme envoie un message plumb qui ouvre le bon
programme.

### Couche de portabilité

`lib9` est le socle : tout le reste du code Plan 9 compile quasiment tel
quel parce que `lib9` recrée son environnement (types, libc, processus,
UTF-8 natif). Les spécificités hôtes (macOS vs Linux/BSD) sont confinées
dans `lib9`, `devdraw` et les mkfiles d'amorçage de `unix/`.

## 7. Pile technologique

| Couche | Choix |
|---|---|
| Langage | C (style Plan 9, ANSI) ; yacc, lex, shell rc/POSIX en appoint |
| Build | `mk` (amorcé via shell + `make`/`cc` système) |
| Concurrence | `libthread` — CSP, channels, threads coopératifs |
| Texte | Runes Unicode / UTF-8 partout (Plan 9 a inventé UTF-8) |
| IPC | protocole 9P (`9pserve`, `9pfuse`) |
| Graphisme | `libdraw` + `devdraw` -> X11 / Quartz |
| Crypto | `libsec`, `libmp`, `libauth` |
| Stockage | `venti` (adressage par contenu), `fossil` (FS versionné) |
| Tests | pas de framework global ; quelques `test*.c` épars par lib ; CI GitHub Actions construit l'arbre |
| CI/CD | `.github/workflows/actions.yaml` ; Coverity Scan |
| Plateformes | Linux, *BSD, macOS, et autres Unix |

## 8. Schéma d'architecture

```
        +-------------------------------------------------------+
        |              Programmes (src/cmd/*)                   |
        |  acme  sam  rio  9term  troff  venti  upas  rc  ...    |
        +----------+-----------------+--------------+-----------+
                   |                 |              |
            +------v------+   +-------v------+  +----v--------+
            |  libdraw    |   |  lib9p /     |  | libsec/auth |
            |  libframe   |   |  9pclient    |  | libmp       |
            |  libmem*    |   |  libplumb    |  | libregexp   |
            +------+------+   +-------+------+  +----+--------+
                   |                 |              |
            +------v-----------------v--------------v----------+
            |   libthread   |   libbio   |   ...   bibliothèques |
            +--------------------------+--------------------------+
            |              lib9  (couche de portabilité)          |
            +--------------------------+--------------------------+
                   |                 |              |
            +------v------+   +-------v------+  +----v--------+
            |  devdraw    |   |  9pserve     |  |  noyau Unix |
            |  -> X11/Qz  |   |  9pfuse/FUSE |  |  (POSIX)    |
            +-------------+   +--------------+  +-------------+

   Build :  ./INSTALL --(shell+cc)--> mk --(mkfiles)--> bin/ + lib/
```

## 9. Environnement et installation

- **Pré-requis** : un Unix avec `cc`/`gcc`, X11 (headers de dev) sur
  Linux/BSD, ou les outils Xcode sur macOS.
- **Variables** : `$PLAN9` (racine de l'arbre, posée par `INSTALL`),
  `$PATH` doit inclure `$PLAN9/bin`. `config`/`LOCAL.config` portent les
  réglages détectés (`FONTSRV`, variantes de `egrep`, ...).
- **Installation** :
  ```
  ./INSTALL          # build + install
  ./INSTALL -b       # build seulement
  ./INSTALL -c       # install (clean) seulement
  ```
  Détails dans `install.txt` et la page `install(1)`.
- **Documentation** : après une install réussie, `9 man <section> <nom>`.
  `intro(1)` liste les pages décrivant les différences avec Plan 9 natif.

## 10. Observations et recommandations

- **Qualité** : base de code mature et stable, style Plan 9 homogène
  (fonctions courtes, peu de commentaires, identifiants concis). Très peu
  de tests automatisés — la vérification est historiquement manuelle.
- **Portabilité** : le pari « tout confiner dans `lib9` + `devdraw` »
  fonctionne bien ; ajouter une plateforme se concentre sur ces points.
- **Sécurité** : usage intensif de pointeurs bruts et de buffers de
  taille fixe (typique du C de l'époque). `libsec` est fonctionnel mais
  n'est pas une cible cryptographique moderne — à ne pas utiliser pour de
  nouveaux besoins sensibles.
- **Build** : `mk` est puissant mais peu familier ; toujours vérifier que
  `$PLAN9` et le `PATH` sont corrects avant de diagnostiquer un échec.
- **Divergence locale** : cet arbre n'est pas vanilla. `CODE_CHANGES.md`
  (racine) et `src/cmd/acme/` documentent une fonctionnalité d'emphase
  ajoutée localement (extension de `libframe` avec une police par boîte).
  Utiliser `mk diffplan9` (là où la cible existe) pour visualiser la
  dérive avant tout envoi en amont.
- **Pour un nouveau contributeur** : commencer par `lib9` (la couche de
  portabilité), puis le protocole 9P (`lib9p` + `9 man 9 9p`), puis un
  programme ciblé. Les pages de manuel `man/man3` sont la référence
  faisant autorité pour les API — davantage que les commentaires.

---

Fichier généré le 2026-05-15. Vue d'ensemble structurée de tout l'arbre
`/usr/local/plan9`. Pour l'analyse détaillée de l'éditeur acme, voir
`src/cmd/acme/codebase_analysis.md`.
